#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/s3.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <numeric>
#include <sstream>
#include <thread>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/third/xml/xml.hpp>
#include <arbiter/util/ini.hpp>
#include <arbiter/util/json.hpp>
#include <arbiter/util/md5.hpp>
#include <arbiter/util/sha256.hpp>
#include <arbiter/util/transforms.hpp>
#include <arbiter/util/util.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace
{
#ifdef ARBITER_CURL
    // Re-fetch credentials when there are less than 4 minutes remaining.  New
    // ones are guaranteed by AWS to be available within 5 minutes remaining.
    constexpr int64_t reauthSeconds(60 * 4);
#endif

    // See:
    // https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/iam-roles-for-amazon-ec2.html
    const std::string ec2CredIp("169.254.169.254");
    const std::string ec2TokenBase(ec2CredIp + "/latest/api/token");
    const std::string ec2CredBase(
            ec2CredIp + "/latest/meta-data/iam/security-credentials");

    const std::string defaultDnsSuffix = "amazonaws.com";

    // https://docs.aws.amazon.com/AmazonECS/latest/developerguide/task-iam-roles.html
    const std::string fargateCredIp("169.254.170.2");

    std::string line(const std::string& data) { return data + "\n"; }
    const std::vector<char> empty;

    typedef Xml::xml_node<> XmlNode;
    const std::string badResponse("Unexpected contents in AWS response");

    std::string toLower(const std::string& in)
    {
        return std::accumulate(
                in.begin(),
                in.end(),
                std::string(),
                [](const std::string& out, const char c) -> std::string
                {
                    return out + static_cast<char>(::tolower(c));
                });
    }

    // Trims sequential whitespace into a single character, and trims all
    // leading and trailing whitespace.
    std::string trim(const std::string& in)
    {
        std::string s = std::accumulate(
                in.begin(),
                in.end(),
                std::string(),
                [](const std::string& out, const char c) -> std::string
                {
                    if (
                        std::isspace(c) &&
                        (out.empty() || std::isspace(out.back())))
                    {
                        return out;
                    }
                    else
                    {
                        return out + c;
                    }
                });

        // Might have one trailing whitespace character.
        if (s.size() && std::isspace(s.back())) s.pop_back();
        return s;
    }

    bool isVerbose()
    {
        std::string verbose;
        if (auto e = env("VERBOSE")) verbose = *e;
        else if (auto e = env("CURL_VERBOSE")) verbose = *e;
        else if (auto e = env("ARBITER_VERBOSE")) verbose = *e;
        return (!verbose.empty()) && !!std::stol(verbose);
    }

    bool doSignRequests()
    {
        return !env("AWS_NO_SIGN_REQUEST");
    }
}

namespace drivers
{

using namespace http;

S3::S3(
        Pool& pool,
        std::string profile,
        std::unique_ptr<Auth> auth,
        std::unique_ptr<Config> config)
    : Http(pool, "s3", "http", profile == "default" ? "" : profile)
    , m_auth(std::move(auth))
    , m_config(std::move(config))
{ }

std::unique_ptr<S3> S3::create(
    Pool& pool,
    const std::string s,
    std::string profile)
{
    if (profile.empty())
    {
        profile = "default";
        if (auto p = env("AWS_DEFAULT_PROFILE")) profile = *p;
        if (auto p = env("AWS_PROFILE")) profile = *p;
    }

    auto auth(doSignRequests() ? Auth::create(s, profile) : nullptr);
    auto config = makeUnique<Config>(s, profile);
    return makeUnique<S3>(pool, profile, std::move(auth), std::move(config));
}

std::unique_ptr<S3::Auth> S3::Auth::create(
    const std::string s,
    const std::string profile)
{
    const json config = s.size() ? json::parse(s) : json();

    if (
            !config.is_null() &&
            config.count("access") &&
            (config.count("secret") || config.count("hidden")))
    {
        return makeUnique<Auth>(
                config["access"].get<std::string>(),
                config.count("secret") ?
                    config["secret"].get<std::string>() :
                    config["hidden"].get<std::string>(),
                config.value("token", ""));
    }

    // Try environment settings next - this only works for the default profile.
    if (profile == "default")
    {
        auto access(env("AWS_ACCESS_KEY_ID"));
        auto hidden(env("AWS_SECRET_ACCESS_KEY"));
        auto token(env("AWS_SESSION_TOKEN"));

        if (access && hidden)
        {
            return makeUnique<Auth>(*access, *hidden, token ? *token : "");
        }

        access = env("AMAZON_ACCESS_KEY_ID");
        hidden = env("AMAZON_SECRET_ACCESS_KEY");
        token = env("AMAZON_SESSION_TOKEN");

        if (access && hidden)
        {
            return makeUnique<Auth>(*access, *hidden, token ? *token : "");
        }
    }

    const std::string credPath(
            env("AWS_CREDENTIAL_FILE") ?
                *env("AWS_CREDENTIAL_FILE") : "~/.aws/credentials");

    // Try reading credentials file.
    drivers::Fs fsDriver;
    if (std::unique_ptr<std::string> c = fsDriver.tryGet(credPath))
    {
        const std::string accessKey("aws_access_key_id");
        const std::string hiddenKey("aws_secret_access_key");
        const std::string tokenKey("aws_session_token");
        const ini::Contents creds(ini::parse(*c));
        if (creds.count(profile))
        {
            const auto section(creds.at(profile));
            if (section.count(accessKey) && section.count(hiddenKey))
            {
                const auto access(section.at(accessKey));
                const auto hidden(section.at(hiddenKey));
                if (section.count(tokenKey))
                {
                    const auto token(section.at(tokenKey));
                    return makeUnique<Auth>(access, hidden, token);
                }
                return makeUnique<Auth>(access, hidden);
           }
        }
    }

#ifdef ARBITER_CURL
    http::Pool pool;
    drivers::Http httpDriver(pool);

    // Nothing found in the environment or on the filesystem.  However we may
    // be running in an EC2 instance with an instance profile set up.
    try
    {
        std::string token;

        try
        {
            // The below request is for the IMDSv2 token.  On EC2 instances
            // which only support v1, this request will fail.  That's ok, the
            // next request looks the same anyway (except without the token of
            // course), and that corresponds to the IMDSv1 flow as a fallback.
            const auto res = httpDriver.internalPut(
                ec2TokenBase,
                std::vector<char>(),
                {{ "X-aws-ec2-metadata-token-ttl-seconds", "21600" }},
                {{ }},
                0,
                1);


            if (!res.ok()) throw ArbiterError("Failed to get IMDSv2 token");

            const auto tokenvec = res.data();
            token = std::string(tokenvec.data(), tokenvec.size());
        }
        catch (...) { }

        http::Headers headers;
        if (!token.empty()) headers["X-aws-ec2-metadata-token"] = token;

        const auto res = httpDriver.internalGet(
            ec2CredBase,
            headers,
            {{ }},
            0,
            0,
            1);
        if (!res.ok()) throw ArbiterError("Failed to get IAM role");

        const auto rolevec = res.data();
        const auto iamRole = std::string(rolevec.begin(), rolevec.end());

        if (!iamRole.empty())
        {
            const bool imdsv2 = !token.empty();
            return makeUnique<Auth>(ec2CredBase + "/" + iamRole, imdsv2);
        }
    }
    catch (...) { }

    // We also may be running in Fargate, which looks very similar but with a
    // different IP.
    if (const auto relUri = env("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI"))
    {
        return makeUnique<Auth>(fargateCredIp + "/" + *relUri);
    }
#endif

    return std::unique_ptr<Auth>();
}

S3::Config::Config(const std::string s, const std::string profile)
    : m_region(extractRegion(s, profile))
    , m_baseUrl(extractBaseUrl(s, m_region))
{
    const json c(s.size() ? json::parse(s) : json());
    if (c.is_null()) return;

    m_precheck = c.value("precheck", false);

    if (c.value("sse", false) || env("AWS_SSE"))
    {
        m_baseHeaders["x-amz-server-side-encryption"] = "AES256";
    }

    if (c.value("requesterPays", false) || env("AWS_REQUESTER_PAYS"))
    {
        m_baseHeaders["x-amz-request-payer"] = "requester";
    }

    if (c.count("headers"))
    {
        const json& headers(c["headers"]);

        if (headers.is_object())
        {
            for (const auto& p : headers.items())
            {
                m_baseHeaders[p.key()] = p.value().get<std::string>();
            }
        }
    }
}

std::string S3::Config::extractRegion(
    const std::string s,
    const std::string profile)
{
    const std::string configPath(
            env("AWS_CONFIG_FILE") ?
                *env("AWS_CONFIG_FILE") : "~/.aws/config");

    drivers::Fs fsDriver;

    const json c(s.size() ? json::parse(s) : json());

    if (!c.is_null() && c.count("region"))
    {
        return c.at("region").get<std::string>();
    }
    else if (auto p = env("AWS_REGION"))
    {
        return *p;
    }
    else if (auto p = env("AWS_DEFAULT_REGION"))
    {
        return *p;
    }
    else if (std::unique_ptr<std::string> c = fsDriver.tryGet(configPath))
    {
        const ini::Contents settings(ini::parse(*c));
        if (settings.count(profile))
        {
            const auto section(settings.at(profile));
            if (section.count("region")) return section.at("region");
        }
    }

    if (!c.is_null() && c.value("verbose", false))
    {
        std::cout << "Region not found - defaulting to us-east-1" << std::endl;
    }

    return "us-east-1";
}

std::string S3::Config::extractBaseUrl(
    const std::string s,
    const std::string region)
{
    if (auto p = env("AWS_ENDPOINT_URL"))
    {
        const std::string path = *p;
        return path.back() == '/' ? path : path + '/';
    }

    const json c(s.size() ? json::parse(s) : json());

    if (!c.is_null() &&
            c.count("endpoint") &&
            c["endpoint"].get<std::string>().size())
    {
        const std::string path(c["endpoint"].get<std::string>());
        return path.back() == '/' ? path : path + '/';
    }

    std::string endpointsPath("~/.aws/endpoints.json");

    if (const auto e = env("AWS_ENDPOINTS_FILE"))
    {
        endpointsPath = *e;
    }

    drivers::Fs fsDriver;
    if (std::unique_ptr<std::string> e = fsDriver.tryGet(endpointsPath))
    {
        const json ep(json::parse(*e));

        for (const auto& partition : ep["partitions"])
        {
            if (
                !partition.count("regions") || 
                !partition.at("regions").count(region))
            {
                continue;
            }

            // Look for an explicit hostname for this region/service.
            if (
                partition.count("services") && 
                partition["services"].count("s3") &&
                partition["services"]["s3"].count("endpoints"))
            {
                const auto& endpoints(partition["services"]["s3"]["endpoints"]);

                for (const auto& r : endpoints.items())
                {
                    if (r.key() == region &&
                            endpoints.value("region", json::object())
                                .count("hostname"))
                    {
                        return endpoints["region"]["hostname"].get<std::string>() +
                            '/';
                    }
                }
            }

            // No explicit hostname found, so build it from our region/DNS suffix.
            std::string dnsSuffix = partition.value("dnsSuffix", defaultDnsSuffix);
            return "s3." + region + "." + dnsSuffix + "/";
        }
    }

    // https://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
    if (region == "us-east-1") return "s3." + defaultDnsSuffix + "/";
    else return "s3-" + region + "." + defaultDnsSuffix + "/";
}

S3::AuthFields S3::Auth::fields() const
{
#ifdef ARBITER_CURL
    if (m_credUrl)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const Time now;
        if (!m_expiration || *m_expiration - now < reauthSeconds)
        {
            http::Pool pool;
            drivers::Http httpDriver(pool);

            std::string token;

            if (m_imdsv2)
            {
                try
                {
                    const auto res = httpDriver.internalPut(
                        ec2TokenBase,
                        std::vector<char>(),
                        {{ "X-aws-ec2-metadata-token-ttl-seconds", "21600" }},
                        {{ }},
                        0,
                        1);

                    if (!res.ok())
                    {
                        throw ArbiterError("Failed to get IMDSv2 token");
                    }

                    const auto tokenvec = res.data();
                    token = std::string(tokenvec.data(), tokenvec.size());
                }
                catch (...) { }
            }

            http::Headers headers;
            if (!token.empty()) headers["X-aws-ec2-metadata-token"] = token;

            const json creds = json::parse(
                httpDriver.get(*m_credUrl, headers));

            m_access = creds.at("AccessKeyId").get<std::string>();
            m_hidden = creds.at("SecretAccessKey").get<std::string>();
            m_token = creds.at("Token").get<std::string>();
            m_expiration.reset(
                    new Time(
                        creds.at("Expiration").get<std::string>(),
                        arbiter::Time::iso8601));

            if (*m_expiration - now < reauthSeconds)
            {
                throw ArbiterError("Got invalid instance profile credentials");
            }
        }

        // If we're using an IAM role, make sure to create this before
        // releasing the lock.
        return S3::AuthFields(m_access, m_hidden, m_token);
    }
#endif

    return S3::AuthFields(m_access, m_hidden, m_token);
}

std::unique_ptr<std::size_t> S3::tryGetSize(
    const std::string rawPath,
    const http::Headers userHeaders,
    const http::Query query) const
{
    Headers headers(m_config->baseHeaders());
    headers.erase("x-amz-server-side-encryption");
    headers.insert(userHeaders.begin(), userHeaders.end());

    const Resource resource(m_config->baseUrl(), rawPath);
    const ApiV4 apiV4(
            "HEAD",
            m_config->region(),
            resource,
            authFields(),
            query,
            headers,
            empty);

    drivers::Http http(m_pool);
    Response res(http.internalHead(resource.url(), apiV4.headers()));

    if (res.ok())
    {
        const auto cl = findHeader(res.headers(), "Content-Length");
        if (cl) return makeUnique<std::size_t>(std::stoull(*cl));
    }

    return std::unique_ptr<std::size_t>();
}

bool S3::get(
        const std::string rawPath,
        std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    Headers headers(m_config->baseHeaders());
    headers.erase("x-amz-server-side-encryption");
    headers.insert(userHeaders.begin(), userHeaders.end());

    std::unique_ptr<std::size_t> size(
            m_config->precheck() && !headers.count("Range") ?
                tryGetSize(rawPath, userHeaders, query) : nullptr);

    const Resource resource(m_config->baseUrl(), rawPath);
    const ApiV4 apiV4(
            "GET",
            m_config->region(),
            resource,
            authFields(),
            query,
            headers,
            empty);

    drivers::Http http(m_pool);
    Response res(
            http.internalGet(
                resource.url(),
                apiV4.headers(),
                apiV4.query(),
                size ? *size : 0));

    if (res.ok())
    {
        data = res.data();
        return true;
    }

    if (isVerbose()) std::cout << res.code() << ": " << res.str() << std::endl;

    return false;
}

std::vector<char> S3::put(
        const std::string rawPath,
        const std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    const Resource resource(m_config->baseUrl(), rawPath);

    Headers headers(m_config->baseHeaders());
    headers.insert(userHeaders.begin(), userHeaders.end());

    if (getExtension(rawPath) == "json")
    {
        headers["Content-Type"] = "application/json";
    }

    const ApiV4 apiV4(
            "PUT",
            m_config->region(),
            resource,
            authFields(),
            query,
            headers,
            data);

    drivers::Http http(m_pool);
    Response res(
            http.internalPut(
                resource.url(),
                data,
                apiV4.headers(),
                apiV4.query()));

    if (!res.ok())
    {
        throw ArbiterError(
                "Couldn't S3 PUT to " + rawPath + ": " +
                std::string(res.data().data(), res.data().size()));
    }

    return res.data();
}

void S3::copy(const std::string src, const std::string dst) const
{
    Headers headers;
    const Resource resource(m_config->baseUrl(), src);
    headers["x-amz-copy-source"] = resource.bucket() + '/' + resource.object();
    put(dst, std::vector<char>(), headers, Query());
}

std::vector<std::string> S3::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;
    path.pop_back();

    const bool recursive(path.back() == '*');
    if (recursive) path.pop_back();

    // https://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGET.html
    const Resource resource(m_config->baseUrl(), path);
    const std::string bucket(resource.bucket());
    const std::string object(resource.object());

    Query query;

    if (object.size()) query["prefix"] = object;

    bool more(false);
    std::vector<char> data;

    do
    {
        if (verbose) std::cout << "." << std::flush;

        if (!get(bucket + "/", data, Headers(), query))
        {
            throw ArbiterError("Couldn't S3 GET " + bucket);
        }

        // XML parsing mucks with the data, so copy it out in case we need it.
        const std::string datastring(data.data(), data.size());
        data.push_back('\0');

        Xml::xml_document<> xml;

        try
        {
            xml.parse<0>(data.data());
        }
        catch (Xml::parse_error&)
        {
            throw ArbiterError("Could not parse S3 response.");
        }

        if (XmlNode* topNode = xml.first_node("ListBucketResult"))
        {
            if (XmlNode* truncNode = topNode->first_node("IsTruncated"))
            {
                std::string t(truncNode->value());
                std::transform(t.begin(), t.end(), t.begin(), ::tolower);

                more = (t == "true");
            }

            if (XmlNode* conNode = topNode->first_node("Contents"))
            {
                for ( ; conNode; conNode = conNode->next_sibling())
                {
                    if (XmlNode* keyNode = conNode->first_node("Key"))
                    {
                        std::string key(keyNode->value());
                        const bool isSubdir(
                                key.find('/', object.size()) !=
                                std::string::npos);

                        // The prefix may contain slashes (i.e. is a sub-dir)
                        // but we only want to traverse into subdirectories
                        // beyond the prefix if recursive is true.
                        if (recursive || !isSubdir)
                        {
                            results.push_back(
                                    profiledProtocol() + "://" +
                                    bucket + "/" + key);
                        }

                        if (more)
                        {
                            query["marker"] =
                                object + key.substr(object.size());
                        }
                    }
                    else
                    {
                        if (isVerbose())
                        {
                            std::cout << "Missing Key: " << datastring <<
                             std::endl;
                        }
                        throw ArbiterError(badResponse);
                    }
                }
            }
            else
            {
                if (isVerbose())
                {
                    std::cout << "Missing Contents: " << datastring <<
                        std::endl;
                }
                throw ArbiterError(badResponse);
            }
        }
        else
        {
            if (isVerbose())
            {
                std::cout << "Missing ListBucketResult: " << datastring <<
                    std::endl;
            }
            throw ArbiterError(badResponse);
        }

        xml.clear();
    }
    while (more);

    return results;
}

S3::AuthFields S3::authFields() const
{
    return m_auth ? m_auth->fields() : S3::AuthFields();
}

S3::ApiV4::ApiV4(
        const std::string verb,
        const std::string& region,
        const Resource& resource,
        const S3::AuthFields authFields,
        const Query& query,
        const Headers& headers,
        const std::vector<char>& data)
    : m_authFields(authFields)
    , m_region(region)
    , m_time()
    , m_headers(headers)
    , m_query(query)
    , m_signedHeadersString()
{
    m_headers["Host"] = resource.host();
    m_headers["X-Amz-Date"] = m_time.str(Time::iso8601NoSeparators);
    if (m_authFields.token().size())
    {
        m_headers["X-Amz-Security-Token"] = m_authFields.token();
    }
    m_headers["X-Amz-Content-Sha256"] =
            crypto::encodeAsHex(crypto::sha256(data));

    if (verb == "PUT" || verb == "POST")
    {
        if (!m_headers.count("Content-Type"))
        {
            m_headers["Content-Type"] = "application/octet-stream";
        }
        m_headers.erase("Transfer-Encoding");
        m_headers.erase("Expect");
    }

    if (!m_authFields) return;

    const Headers normalizedHeaders(
            std::accumulate(
                m_headers.begin(),
                m_headers.end(),
                Headers(),
                [](const Headers& in, const Headers::value_type& h)
                {
                    Headers out(in);
                    out[toLower(h.first)] = trim(h.second);
                    return out;
                }));

    m_canonicalHeadersString =
            std::accumulate(
                normalizedHeaders.begin(),
                normalizedHeaders.end(),
                std::string(),
                [](const std::string& in, const Headers::value_type& h)
                {
                    return in + h.first + ':' + h.second + '\n';
                });

    m_signedHeadersString =
            std::accumulate(
                normalizedHeaders.begin(),
                normalizedHeaders.end(),
                std::string(),
                [](const std::string& in, const Headers::value_type& h)
                {
                    return in + (in.empty() ? "" : ";") + h.first;
                });

    const std::string canonicalRequest(
            buildCanonicalRequest(verb, resource, query, data));

    const std::string stringToSign(buildStringToSign(canonicalRequest));

    const std::string signature(calculateSignature(stringToSign));

    m_headers["Authorization"] =
            getAuthHeader(m_signedHeadersString, signature);
}

std::string S3::ApiV4::buildCanonicalRequest(
        const std::string verb,
        const Resource& resource,
        const Query& query,
        const std::vector<char>& data) const
{
    const std::string canonicalUri = resource.canonicalUri();

    auto canonicalizeQuery([](const std::string& s, const Query::value_type& q)
    {
        const std::string keyVal(
                sanitize(q.first, "") + '=' +
                sanitize(q.second, ""));

        return s + (s.size() ? "&" : "") + keyVal;
    });

    const std::string canonicalQuery(
            std::accumulate(
                query.begin(),
                query.end(),
                std::string(),
                canonicalizeQuery));

    return
        line(verb) +
        line(canonicalUri) +
        line(canonicalQuery) +
        line(m_canonicalHeadersString) +
        line(m_signedHeadersString) +
        crypto::encodeAsHex(crypto::sha256(data));
}

std::string S3::ApiV4::buildStringToSign(
        const std::string& canonicalRequest) const
{
    return
        line("AWS4-HMAC-SHA256") +
        line(m_time.str(Time::iso8601NoSeparators)) +
        line(m_time.str(Time::dateNoSeparators) +
                "/" + m_region + "/s3/aws4_request") +
        crypto::encodeAsHex(crypto::sha256(canonicalRequest));
}

std::string S3::ApiV4::calculateSignature(
        const std::string& stringToSign) const
{
    const std::string kDate(
            crypto::hmacSha256(
                "AWS4" + m_authFields.hidden(),
                m_time.str(Time::dateNoSeparators)));

    const std::string kRegion(crypto::hmacSha256(kDate, m_region));
    const std::string kService(crypto::hmacSha256(kRegion, "s3"));
    const std::string kSigning(
            crypto::hmacSha256(kService, "aws4_request"));

    return crypto::encodeAsHex(crypto::hmacSha256(kSigning, stringToSign));
}

std::string S3::ApiV4::getAuthHeader(
        const std::string& signedHeadersString,
        const std::string& signature) const
{
    return
        std::string("AWS4-HMAC-SHA256 ") +
        "Credential=" + m_authFields.access() + '/' +
            m_time.str(Time::dateNoSeparators) + "/" +
            m_region + "/s3/aws4_request, " +
        "SignedHeaders=" + signedHeadersString + ", " +
        "Signature=" + signature;
}

S3::Resource::Resource(std::string base, std::string fullPath)
    : m_baseUrl(base)
    , m_bucket()
    , m_object()
    , m_virtualHosted()
{
    fullPath = sanitize(fullPath);
    const std::size_t split(fullPath.find("/"));

    m_bucket = fullPath.substr(0, split);
    if (split != std::string::npos) m_object = fullPath.substr(split + 1);

    // By default, we use virtual-hosted URLs if the bucket name contains no dots.
    // This can be overridden with the AWS_VIRTUAL_HOSTING environment variable
    // (set to "TRUE" or "FALSE")
    //
    // We would prefer to use virtual-hosted URLs all the time since path-style
    // URLs are being deprecated in 2020.  We also want to use HTTPS all the
    // time, which is required for KMS-managed server-side encryption.  However,
    // these two desires are incompatible if the bucket name contains dots,
    // because the SSL cert will not allow virtual-hosted paths to be accessed
    // over HTTPS.  So we'll fall back to path-style requests for buckets
    // containing dots.  A fix for this should be announced by September 30,
    // 2020, at which point maybe we can use virtual-hosted paths all the time.

    // Deprecation plan for path-style URLs:
    // https://aws.amazon.com/blogs/aws/amazon-s3-path-deprecation-plan-the-rest-of-the-story/

    // Dots in bucket name limitation with virtual-hosting over HTTPS:
    // https://docs.aws.amazon.com/AmazonS3/latest/dev/VirtualHosting.html#VirtualHostingLimitations

    // 2021 note: the deprecation date got delayed, and buckets containing
    // dots still has no fix - see the note at the top of the first link above.
    // So for the time being, we'll keep this forked logic below.
    m_virtualHosted = parseBoolFromEnv(
        "AWS_VIRTUAL_HOSTING",
        m_bucket.find_first_of('.') == std::string::npos
    );
}

std::string S3::Resource::canonicalUri() const
{
    if (m_virtualHosted)
    {
        return "/" + m_object;
    }
    else
    {
        return "/" + m_bucket + "/" + m_object;
    }
}

std::string S3::Resource::baseUrl() const
{
    return m_baseUrl;
}

std::string S3::Resource::bucket() const
{
    return m_bucket;
}

std::string S3::Resource::url() const
{
    if (m_virtualHosted)
    {
        return "https://" + m_bucket + "." + m_baseUrl + m_object;
    }
    else
    {
        return "https://" + m_baseUrl + m_bucket + "/" + m_object;
    }
}

std::string S3::Resource::object() const
{
    return m_object;
}

std::string S3::Resource::host() const
{
    if (m_virtualHosted)
    {
        // Pop slash.
        return m_bucket + "." + m_baseUrl.substr(0, m_baseUrl.size() - 1);
    }
    else
    {
        return m_baseUrl.substr(0, m_baseUrl.size() - 1);
    }
}

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif
