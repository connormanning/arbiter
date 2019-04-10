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
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
#ifdef ARBITER_CURL
    // Re-fetch credentials when there are less than 4 minutes remaining.  New
    // ones are guaranteed by AWS to be available within 5 minutes remaining.
    constexpr int64_t reauthSeconds(60 * 4);
#endif

    // See:
    // https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/iam-roles-for-amazon-ec2.html
    const std::string credIp("169.254.169.254/");
    const std::string credBase(
            credIp + "latest/meta-data/iam/security-credentials/");

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
}

namespace drivers
{

using namespace http;

S3::S3(
        Pool& pool,
        std::string profile,
        std::unique_ptr<Auth> auth,
        std::unique_ptr<Config> config)
    : Http(pool)
    , m_profile(profile)
    , m_auth(std::move(auth))
    , m_config(std::move(config))
{ }

std::vector<std::unique_ptr<S3>> S3::create(Pool& pool, const std::string s)
{
    std::vector<std::unique_ptr<S3>> result;

    const json config(s.size() ? json::parse(s) : json());

    if (config.is_array())
    {
        for (const json& curr : config)
        {
            if (auto s = createOne(pool, curr.dump()))
            {
                result.push_back(std::move(s));
            }
        }
    }
    else if (auto s = createOne(pool, config.dump()))
    {
        result.push_back(std::move(s));
    }

    return result;
}

std::unique_ptr<S3> S3::createOne(Pool& pool, const std::string s)
{
    const json j(s.size() ? json::parse(s) : json());
    const std::string profile(extractProfile(j.dump()));

    auto auth(Auth::create(j.dump(), profile));
    if (!auth) return std::unique_ptr<S3>();

    std::unique_ptr<Config> config(new Config(j.dump(), profile));
    auto s3 = makeUnique<S3>(pool, profile, std::move(auth), std::move(config));
    return s3;
}

std::string S3::extractProfile(const std::string s)
{
    const json config(s.size() ? json::parse(s) : json());

    if (
            !config.is_null() &&
            config.count("profile") &&
            config["profile"].get<std::string>().size())
    {
        return config["profile"].get<std::string>();
    }

    if (auto p = env("AWS_PROFILE")) return *p;
    if (auto p = env("AWS_DEFAULT_PROFILE")) return *p;
    else return "default";
}

std::unique_ptr<S3::Auth> S3::Auth::create(
        const std::string s,
        const std::string profile)
{
    const json config(s.size() ? json::parse(s) : json());

    // Try explicit JSON configuration first.
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

    // Try environment settings next.
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

    // Finally, try reading credentials file.
    drivers::Fs fsDriver;
    if (std::unique_ptr<std::string> c = fsDriver.tryGet(credPath))
    {
        const std::string accessKey("aws_access_key_id");
        const std::string hiddenKey("aws_secret_access_key");
        const ini::Contents creds(ini::parse(*c));
        if (creds.count(profile))
        {
            const auto section(creds.at(profile));
            if (section.count(accessKey) && section.count(hiddenKey))
            {
                const auto access(section.at(accessKey));
                const auto hidden(section.at(hiddenKey));
                return makeUnique<Auth>(access, hidden);
            }
        }
    }

#ifdef ARBITER_CURL
    // Nothing found in the environment or on the filesystem.  However we may
    // be running in an EC2 instance with an instance profile set up.
    //
    // By default we won't search for this since we don't really want to make
    // an HTTP request on every Arbiter construction - but if we're allowed,
    // see if we can request an instance profile configuration.
    if (
            (!config.is_null() &&
                config.value("allowInstanceProfile", false)) ||
            env("AWS_ALLOW_INSTANCE_PROFILE"))
    {
        http::Pool pool;
        drivers::Http httpDriver(pool);

        if (const auto iamRole = httpDriver.tryGet(credBase))
        {
            return makeUnique<Auth>(*iamRole);
        }
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

    if (c.value("sse", false)|| env("AWS_SSE"))
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
        else
        {
            std::cout << "s3.headers expected to be object - skipping" <<
                std::endl;
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

    if (c.is_null()) return "us-east-1";

    if (c.count("region"))
    {
        return c["region"].get<std::string>();
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

    if (c.value("verbose", false))
    {
        std::cout << "Region not found - defaulting to us-east-1" << std::endl;
    }

    return "us-east-1";
}

std::string S3::Config::extractBaseUrl(
        const std::string s,
        const std::string region)
{
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
    else if (c.count("endpointsFile"))
    {
        endpointsPath = c["endpointsFile"].get<std::string>();
    }

    std::string dnsSuffix("amazonaws.com");

    drivers::Fs fsDriver;
    if (std::unique_ptr<std::string> e = fsDriver.tryGet(endpointsPath))
    {
        const json ep(json::parse(*e));

        for (const auto& partition : ep["partitions"])
        {
            if (partition.count("dnsSuffix"))
            {
                dnsSuffix = partition["dnsSuffix"].get<std::string>();
            }

            const auto& endpoints(
                    partition.at("services").at("s3").at("endpoints"));

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
    }

    if (dnsSuffix.size() && dnsSuffix.back() != '/') dnsSuffix += '/';

    // https://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
    if (region == "us-east-1") return "s3." + dnsSuffix;
    else return "s3-" + region + "." + dnsSuffix;
}

S3::AuthFields S3::Auth::fields() const
{
#ifdef ARBITER_CURL
    if (m_role)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const Time now;
        if (!m_expiration || *m_expiration - now < reauthSeconds)
        {
            http::Pool pool;
            drivers::Http httpDriver(pool);

            const json creds(json::parse(httpDriver.get(credBase + *m_role)));
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

std::string S3::type() const
{
    if (m_profile == "default") return "s3";
    else return m_profile + "@s3";
}

std::unique_ptr<std::size_t> S3::tryGetSize(std::string rawPath) const
{
    std::unique_ptr<std::size_t> size;

    Headers headers(m_config->baseHeaders());
    headers.erase("x-amz-server-side-encryption");

    const Resource resource(m_config->baseUrl(), rawPath);
    const ApiV4 apiV4(
            "HEAD",
            m_config->region(),
            resource,
            m_auth->fields(),
            Query(),
            headers,
            empty);

    drivers::Http http(m_pool);
    Response res(http.internalHead(resource.url(), apiV4.headers()));

    if (res.ok() && res.headers().count("Content-Length"))
    {
        const std::string& str(res.headers().at("Content-Length"));
        size.reset(new std::size_t(std::stoul(str)));
    }

    return size;
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
                tryGetSize(rawPath) : nullptr);

    const Resource resource(m_config->baseUrl(), rawPath);
    const ApiV4 apiV4(
            "GET",
            m_config->region(),
            resource,
            m_auth->fields(),
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
    else
    {
        std::cout << res.code() << ": " << res.str() << std::endl;
        return false;
    }
}

void S3::put(
        const std::string rawPath,
        const std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    const Resource resource(m_config->baseUrl(), rawPath);

    Headers headers(m_config->baseHeaders());
    headers.insert(userHeaders.begin(), userHeaders.end());

    if (Arbiter::getExtension(rawPath) == "json")
    {
        headers["Content-Type"] = "application/json";
    }

    const ApiV4 apiV4(
            "PUT",
            m_config->region(),
            resource,
            m_auth->fields(),
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
    const std::string& bucket(resource.bucket());
    const std::string& object(resource.object());

    Query query;

    if (object.size()) query["prefix"] = object;

    bool more(false);
    std::vector<char> data;

    do
    {
        if (verbose) std::cout << "." << std::flush;

        if (!get(resource.bucket() + "/", data, Headers(), query))
        {
            throw ArbiterError("Couldn't S3 GET " + resource.bucket());
        }

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
                                    type() + "://" + bucket + "/" + key);
                        }

                        if (more)
                        {
                            query["marker"] =
                                object + key.substr(object.size());
                        }
                    }
                    else
                    {
                        throw ArbiterError(badResponse);
                    }
                }
            }
            else
            {
                throw ArbiterError(badResponse);
            }
        }
        else
        {
            throw ArbiterError(badResponse);
        }

        xml.clear();
    }
    while (more);

    return results;
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
    const std::string canonicalUri("/" + resource.object());

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
    , m_virtualHosted(true)
{
    fullPath = sanitize(fullPath);
    const std::size_t split(fullPath.find("/"));

    m_bucket = fullPath.substr(0, split);
    if (split != std::string::npos) m_object = fullPath.substr(split + 1);

    // Always use virtual-host style paths.  We'll use HTTP for our back-end
    // calls to allow this.  If we were to use HTTPS on the back-end, then we
    // would have to use non-virtual-hosted paths if the bucket name contained
    // '.' characters.
    //
    // m_virtualHosted = m_bucket.find_first_of('.') == std::string::npos;
}

std::string S3::Resource::baseUrl() const
{
    return m_baseUrl;
}

std::string S3::Resource::bucket() const
{
    return m_virtualHosted ? m_bucket : "";
}

std::string S3::Resource::url() const
{
    if (m_virtualHosted)
    {
        return "http://" + m_bucket + "." + m_baseUrl + m_object;
    }
    else
    {
        return "https://" + m_baseUrl + m_bucket + "/" + m_object;
    }
}

std::string S3::Resource::object() const
{
    // We can't use virtual-host style paths if the bucket contains dots.
    if (m_virtualHosted) return m_object;
    else return m_bucket + "/" + m_object;
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
