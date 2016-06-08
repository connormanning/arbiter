#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
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
#include <thread>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/third/xml/xml.hpp>
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

namespace
{
    const std::string dateFormat("%Y%m%d");
    const std::string timeFormat("%H%M%S");

    std::string getBaseUrl(const std::string& region)
    {
        // https://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
        if (region == "us-east-1") return ".s3.amazonaws.com/";
        else return ".s3-" + region + ".amazonaws.com/";
    }

    drivers::Fs fsDriver;

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
                [](const std::string& out, const char c)
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
                [](const std::string& out, const char c)
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

    std::vector<std::string> condense(const std::vector<std::string>& in)
    {
        return std::accumulate(
                in.begin(),
                in.end(),
                std::vector<std::string>(),
                [](const std::vector<std::string>& base, const std::string& in)
                {
                    auto out(base);

                    std::string current(in);
                    current.erase(
                            std::remove_if(
                                current.begin(),
                                current.end(),
                                [](char c) { return std::isspace(c); }),
                            current.end());

                    out.push_back(current);
                    return out;
                });
    }

    std::vector<std::string> split(const std::string& in, char delimiter = '\n')
    {
        std::size_t index(0);
        std::size_t pos(0);
        std::vector<std::string> lines;

        do
        {
            index = in.find(delimiter, pos);
            std::string line(in.substr(pos, index - pos));

            line.erase(
                    std::remove_if(line.begin(), line.end(), ::isspace),
                    line.end());

            lines.push_back(line);

            pos = index + 1;
        }
        while (index != std::string::npos);

        return lines;
    }
}

namespace drivers
{

using namespace http;

S3::S3(
        Pool& pool,
        const S3::Auth& auth,
        const std::string region,
        const bool sse)
    : Http(pool)
    , m_auth(auth)
    , m_region(region)
    , m_baseUrl(getBaseUrl(region))
    , m_baseHeaders()
{
    if (sse)
    {
        // This could grow to support other SSE schemes, like KMS and customer-
        // supplied keys.
        m_baseHeaders["x-amz-server-side-encryption"] = "AES256";
    }
}

std::unique_ptr<S3> S3::create(Pool& pool, const Json::Value& json)
{
    std::unique_ptr<Auth> auth;
    std::unique_ptr<S3> s3;

    const std::string profile(extractProfile(json));
    const bool sse(json["sse"].asBool());

    if (!json.isNull() && json.isMember("access") & json.isMember("hidden"))
    {
        auth.reset(
                new Auth(
                    json["access"].asString(),
                    json["hidden"].asString()));
    }
    else
    {
        auth = Auth::find(profile);
    }

    if (!auth) return s3;

    // Try to get the region from the config file, or default to US standard.
    std::string region("us-east-1");
    bool regionFound(false);

    const std::string configPath(
            util::env("AWS_CONFIG_FILE") ?
                *util::env("AWS_CONFIG_FILE") : "~/.aws/config");

    if (auto p = util::env("AWS_REGION"))
    {
        region = *p;
        regionFound = true;
    }
    else if (auto p = util::env("AWS_DEFAULT_REGION"))
    {
        region = *p;
        regionFound = true;
    }
    else if (!json.isNull() && json.isMember("region"))
    {
        region = json["region"].asString();
        regionFound = true;
    }
    else if (std::unique_ptr<std::string> config = fsDriver.tryGet(configPath))
    {
        const std::vector<std::string> lines(condense(split(*config)));

        if (lines.size() >= 3)
        {
            std::size_t i(0);

            const std::string profileFind("[" + profile + "]");
            const std::string outputFind("output=");
            const std::string regionFind("region=");

            while (i < lines.size() - 2 && !regionFound)
            {
                if (lines[i].find(profileFind) != std::string::npos)
                {
                    auto parse([&](
                                const std::string& outputLine,
                                const std::string& regionLine)
                    {
                        std::size_t outputPos(outputLine.find(outputFind));
                        std::size_t regionPos(regionLine.find(regionFind));

                        if (
                                outputPos != std::string::npos &&
                                regionPos != std::string::npos)
                        {
                            region = regionLine.substr(
                                    regionPos + regionFind.size(),
                                    regionLine.find(';'));

                            return true;
                        }

                        return false;
                    });


                    const std::string& l1(lines[i + 1]);
                    const std::string& l2(lines[i + 2]);

                    regionFound = parse(l1, l2) || parse(l2, l1);
                }

                ++i;
            }
        }
    }
    else
    {
        std::cout <<
            "~/.aws/config not found - using region us-east-1" << std::endl;
    }

    if (!regionFound)
    {
        std::cout <<
            "Region not found in ~/.aws/config - using us-east-1" << std::endl;
    }

    s3.reset(new S3(pool, *auth, region, sse));

    return s3;
}

std::string S3::extractProfile(const Json::Value& json)
{
    if (auto p = util::env("AWS_PROFILE"))
    {
        return *p;
    }
    else if (auto p = util::env("AWS_DEFAULT_PROFILE"))
    {
        return *p;
    }
    else if (
            !json.isNull() &&
            json.isMember("profile") &&
            json["profile"].asString().size())
    {
        return json["profile"].asString();
    }
    else
    {
        return "default";
    }
}

std::unique_ptr<std::size_t> S3::tryGetSize(std::string rawPath) const
{
    std::unique_ptr<std::size_t> size;

    const Resource resource(m_baseUrl, rawPath);
    const ApiV4 apiV4(
            "HEAD",
            m_region,
            resource,
            m_auth,
            Query(),
            Headers(),
            empty);

    Response res(Http::internalHead(resource.url(), apiV4.headers()));

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
        const Headers headers,
        const Query query) const
{
    const Resource resource(m_baseUrl, rawPath);
    const ApiV4 apiV4(
            "GET",
            m_region,
            resource,
            m_auth,
            query,
            headers,
            empty);

    Response res(
            Http::internalGet(
                resource.url(),
                apiV4.headers(),
                apiV4.query()));

    if (res.ok())
    {
        data = res.data();
        return true;
    }
    else
    {
        std::cout << std::string(res.data().data(), res.data().size()) <<
            std::endl;
        return false;
    }
}

void S3::put(
        const std::string rawPath,
        const std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    const Resource resource(m_baseUrl, rawPath);

    Headers headers(m_baseHeaders);
    headers.insert(userHeaders.begin(), userHeaders.end());

    const ApiV4 apiV4(
            "PUT",
            m_region,
            resource,
            m_auth,
            query,
            headers,
            data);

    Response res(
            Http::internalPut(
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

std::vector<std::string> S3::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;
    path.pop_back();

    const bool recursive(path.back() == '*');
    if (recursive) path.pop_back();

    // https://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGET.html
    const Resource resource(m_baseUrl, path);
    const std::string& bucket(resource.bucket);
    const std::string& object(resource.object);

    Query query;

    if (object.size()) query["prefix"] = object;

    bool more(false);
    std::vector<char> data;

    do
    {
        if (verbose) std::cout << "." << std::flush;

        if (!get(resource.bucket + "/", data, Headers(), query))
        {
            throw ArbiterError("Couldn't S3 GET " + resource.bucket);
        }

        data.push_back('\0');

        Xml::xml_document<> xml;

        try
        {
            xml.parse<0>(data.data());
        }
        catch (Xml::parse_error)
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
                            results.push_back("s3://" + bucket + "/" + key);
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
        const S3::Auth& auth,
        const Query& query,
        const Headers& headers,
        const std::vector<char>& data)
    : m_auth(auth)
    , m_region(region)
    , m_formattedTime()
    , m_headers(headers)
    , m_query(query)
    , m_signedHeadersString()
{
    m_headers["Host"] = resource.host();
    m_headers["X-Amz-Date"] = m_formattedTime.amazonDate();
    m_headers["X-Amz-Content-Sha256"] =
            crypto::encodeAsHex(crypto::sha256(data));

    if (verb == "PUT" || verb == "POST")
    {
        m_headers["Content-Type"] = "application/octet-stream";
        m_headers["Transfer-Encoding"] = "";
        m_headers["Expect"] = "";
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
    const std::string canonicalUri(sanitize("/" + resource.object));

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
        line(m_formattedTime.amazonDate()) +
        line(m_formattedTime.date() + "/" + m_region + "/s3/aws4_request") +
        crypto::encodeAsHex(crypto::sha256(canonicalRequest));
}

std::string S3::ApiV4::calculateSignature(
        const std::string& stringToSign) const
{
    const std::string kDate(
            crypto::hmacSha256(
                "AWS4" + m_auth.hidden(),
                m_formattedTime.date()));

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
        "Credential=" + m_auth.access() + '/' +
            m_formattedTime.date() + "/" + m_region + "/s3/aws4_request, " +
        "SignedHeaders=" + signedHeadersString + ", " +
        "Signature=" + signature;
}

S3::Resource::Resource(std::string baseUrl, std::string fullPath)
    : baseUrl(baseUrl)
    , bucket()
    , object()
{
    fullPath = sanitize(fullPath);
    const std::size_t split(fullPath.find("/"));

    bucket = fullPath.substr(0, split);

    if (split != std::string::npos)
    {
        object = fullPath.substr(split + 1);
    }
}

std::string S3::Resource::url() const
{
    return "https://" + bucket + baseUrl + object;
}

std::string S3::Resource::host() const
{
    return bucket + baseUrl.substr(0, baseUrl.size() - 1); // Pop slash.
}

S3::FormattedTime::FormattedTime()
    : m_date(formatTime(dateFormat))
    , m_time(formatTime(timeFormat))
{ }

std::string S3::FormattedTime::formatTime(const std::string& format) const
{
    std::time_t time(std::time(nullptr));
    std::vector<char> buf(80, 0);

    if (std::strftime(
                buf.data(),
                buf.size(),
                format.data(),
                std::gmtime(&time)))
    {
        return std::string(buf.data());
    }
    else
    {
        throw ArbiterError("Could not format time");
    }
}

S3::Auth::Auth(const std::string access, const std::string hidden)
    : m_access(access)
    , m_hidden(hidden)
{ }

std::unique_ptr<S3::Auth> S3::Auth::find(std::string profile)
{
    std::unique_ptr<S3::Auth> auth;

    auto access(util::env("AWS_ACCESS_KEY_ID"));
    auto hidden(util::env("AWS_SECRET_ACCESS_KEY"));

    if (access && hidden)
    {
        auth.reset(new S3::Auth(*access, *hidden));
        return auth;
    }

    access = util::env("AMAZON_ACCESS_KEY_ID");
    hidden = util::env("AMAZON_SECRET_ACCESS_KEY");

    if (access && hidden)
    {
        auth.reset(new S3::Auth(*access, *hidden));
        return auth;
    }

    const std::string credFile("~/.aws/credentials");

    // First, try reading credentials file.
    if (std::unique_ptr<std::string> cred = fsDriver.tryGet(credFile))
    {
        const std::vector<std::string> lines(condense(split(*cred)));

        if (lines.size() >= 3)
        {
            std::size_t i(0);

            const std::string profileFind("[" + profile + "]");
            const std::string accessFind("aws_access_key_id=");
            const std::string hiddenFind("aws_secret_access_key=");

            while (i < lines.size() - 2 && !auth)
            {
                if (lines[i].find(profileFind) != std::string::npos)
                {
                    const std::string& accessLine(lines[i + 1]);
                    const std::string& hiddenLine(lines[i + 2]);

                    std::size_t accessPos(accessLine.find(accessFind));
                    std::size_t hiddenPos(hiddenLine.find(hiddenFind));

                    if (
                            accessPos != std::string::npos &&
                            hiddenPos != std::string::npos)
                    {
                        const std::string access(
                                accessLine.substr(
                                    accessPos + accessFind.size(),
                                    accessLine.find(';')));

                        const std::string hidden(
                                hiddenLine.substr(
                                    hiddenPos + hiddenFind.size(),
                                    hiddenLine.find(';')));

                        auth.reset(new S3::Auth(access, hidden));
                    }
                }

                ++i;
            }
        }
    }

    return auth;
}

std::string S3::Auth::access() const { return m_access; }
std::string S3::Auth::hidden() const { return m_hidden; }

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

