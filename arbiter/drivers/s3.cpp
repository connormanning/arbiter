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
#include <arbiter/util/transforms.hpp>
#include <arbiter/util/sha256.hpp>
#endif

namespace arbiter
{

namespace
{
    const std::string baseUrl(".s3.amazonaws.com/");
    const std::string dateFormat("%Y%m%d");
    const std::string timeFormat("%H%M%S");

    std::string line(const std::string& data) { return data + "\n"; }
    const std::vector<char> empty;

    std::string getQueryString(const Query& query)
    {
        std::string result;

        bool first(true);
        for (const auto& q : query)
        {
            result += (first ? "?" : "&") + q.first + "=" + q.second;
            first = false;
        }

        return result;
    }

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

    std::string trim(const std::string& in)
    {
        return std::accumulate(
                in.begin(),
                in.end(),
                std::string(),
                [](const std::string& out, const char c)
                {
                    if (c == ' ' && (out.empty() || out.back() == ' '))
                    {
                        return out;
                    }
                    else
                    {
                        return out + c;
                    }
                });
    }

    typedef Xml::xml_node<> XmlNode;

    const std::string badResponse("Unexpected contents in AWS response");
}

namespace drivers
{

AwsAuth::AwsAuth(const std::string access, const std::string hidden)
    : m_access(access)
    , m_hidden(hidden)
{ }

std::unique_ptr<AwsAuth> AwsAuth::find(std::string user)
{
    std::unique_ptr<AwsAuth> auth;

    if (user.empty())
    {
        user = getenv("AWS_PROFILE") ? getenv("AWS_PROFILE") : "default";
    }

    drivers::Fs fs;
    std::unique_ptr<std::string> file(fs.tryGet("~/.aws/credentials"));

    // First, try reading credentials file.
    if (file)
    {
        std::size_t index(0);
        std::size_t pos(0);
        std::vector<std::string> lines;

        do
        {
            index = file->find('\n', pos);
            std::string line(file->substr(pos, index - pos));

            line.erase(
                    std::remove_if(line.begin(), line.end(), ::isspace),
                    line.end());

            lines.push_back(line);

            pos = index + 1;
        }
        while (index != std::string::npos);

        if (lines.size() >= 3)
        {
            std::size_t i(0);

            const std::string userFind("[" + user + "]");
            const std::string accessFind("aws_access_key_id=");
            const std::string hiddenFind("aws_secret_access_key=");

            while (i < lines.size() - 2 && !auth)
            {
                if (lines[i].find(userFind) != std::string::npos)
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

                        auth.reset(new AwsAuth(access, hidden));
                    }
                }

                ++i;
            }
        }
    }

    // Fall back to environment settings.
    if (!auth)
    {
        if (getenv("AWS_ACCESS_KEY_ID") && getenv("AWS_SECRET_ACCESS_KEY"))
        {
            auth.reset(
                    new AwsAuth(
                        getenv("AWS_ACCESS_KEY_ID"),
                        getenv("AWS_SECRET_ACCESS_KEY")));
        }
        else if (
                getenv("AMAZON_ACCESS_KEY_ID") &&
                getenv("AMAZON_SECRET_ACCESS_KEY"))
        {
            auth.reset(
                    new AwsAuth(
                        getenv("AMAZON_ACCESS_KEY_ID"),
                        getenv("AMAZON_SECRET_ACCESS_KEY")));
        }
    }

    return auth;
}

std::string AwsAuth::access() const
{
    return m_access;
}

std::string AwsAuth::hidden() const
{
    return m_hidden;
}

S3::S3(
        HttpPool& pool,
        const AwsAuth auth,
        const std::string sseKey)
    : m_pool(pool)
    , m_auth(auth)
    , m_sseHeaders()
{
    if (!sseKey.empty())
    {
        Headers h;
        h["x-amz-server-side-encryption-customer-algorithm"] = "AES256";
        h["x-amz-server-side-encryption-customer-key"] = sseKey;
        h["x-amz-server-side-encryption-customer-key-MD5"] =
            crypto::md5(sseKey);

        m_sseHeaders.reset(new Headers(h));
    }
}

std::unique_ptr<S3> S3::create(HttpPool& pool, const Json::Value& json)
{
    std::unique_ptr<S3> s3;

    const std::string sseKey(json["sse"].asString());

    if (!json.isNull() && json.isMember("access") & json.isMember("hidden"))
    {
        AwsAuth auth(json["access"].asString(), json["hidden"].asString());
        s3.reset(new S3(pool, auth));
    }
    else
    {
        auto auth(AwsAuth::find(json.isNull() ? "" : json["user"].asString()));
        if (auth) s3.reset(new S3(pool, *auth));
    }

    return s3;
}

std::unique_ptr<std::size_t> S3::tryGetSize(std::string rawPath) const
{
    std::unique_ptr<std::size_t> size;

    const Resource resource(rawPath);
    const AuthV4 authV4("HEAD", resource, m_auth, Query(), Headers(), empty);

    auto http(m_pool.acquire());
    HttpResponse res(http.head(resource.buildPath(), authV4.headers()));

    if (res.ok() && res.headers().count("Content-Length"))
    {
        const std::string& str(res.headers().at("Content-Length"));
        size.reset(new std::size_t(std::stoul(str)));
    }

    return size;
}

bool S3::get(std::string rawPath, std::vector<char>& data) const
{
    return get(rawPath, Query(), Headers(), data);
}

bool S3::get(
        std::string rawPath,
        const Query& query,
        const Headers& headers,
        std::vector<char>& data) const
{
    const Resource resource(rawPath);
    const AuthV4 authV4("GET", resource, m_auth, query, headers, empty);

    auto http(m_pool.acquire());
    HttpResponse res(http.get(resource.buildPath(query), authV4.headers()));

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

void S3::put(std::string rawPath, const std::vector<char>& data) const
{
    const Resource resource(rawPath);

    Headers headers(m_sseHeaders ? *m_sseHeaders : Headers());
    const AuthV4 authV4("PUT", resource, m_auth, Query(), headers, data);

    auto http(m_pool.acquire());
    HttpResponse res(http.put(resource.buildPath(), data, authV4.headers()));

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
    const Resource resource(path);
    const std::string& bucket(resource.bucket);
    const std::string& object(resource.object);

    Query query;

    if (object.size()) query["prefix"] = object;

    bool more(false);
    std::vector<char> data;

    do
    {
        if (verbose) std::cout << "." << std::flush;

        if (!get(resource.bucket + "/", query, Headers(), data))
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

S3::AuthV4::AuthV4(
        const std::string verb,
        const Resource& resource,
        const AwsAuth& auth,
        const Query& query,
        const Headers& headers,
        const std::vector<char>& data)
    : m_auth(auth)
    , m_formattedTime()
    , m_headers(headers)
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

std::string S3::AuthV4::buildCanonicalRequest(
        const std::string verb,
        const Resource& resource,
        const Query& query,
        const std::vector<char>& data) const
{
    const std::string canonicalUri(Http::sanitize("/" + resource.object));

    auto canonicalizeQuery([](const std::string& s, const Query::value_type& q)
    {
        const std::string keyVal(
                Http::sanitize(q.first, "") + '=' +
                Http::sanitize(q.second, ""));

        return (s.size() ? "&" : "") + keyVal;
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

std::string S3::AuthV4::buildStringToSign(
        const std::string& canonicalRequest) const
{
    return
        line("AWS4-HMAC-SHA256") +
        line(m_formattedTime.amazonDate()) +
        line(m_formattedTime.date() + "/us-east-1/s3/aws4_request") +
        crypto::encodeAsHex(crypto::sha256(canonicalRequest));
}

std::string S3::AuthV4::calculateSignature(
        const std::string& stringToSign) const
{
    const std::string kDate(
            crypto::hmacSha256(
                "AWS4" + m_auth.hidden(),
                m_formattedTime.date()));

    const std::string kRegion(
            crypto::hmacSha256(kDate, "us-east-1"));

    const std::string kService(
            crypto::hmacSha256(kRegion, "s3"));

    const std::string kSigning(
            crypto::hmacSha256(kService, "aws4_request"));

    return crypto::encodeAsHex(crypto::hmacSha256(kSigning, stringToSign));
}

std::string S3::AuthV4::getAuthHeader(
        const std::string& signedHeadersString,
        const std::string& signature) const
{
    return
        std::string("AWS4-HMAC-SHA256 ") +
        "Credential=" + m_auth.access() + '/' +
            m_formattedTime.date() + "/us-east-1/s3/aws4_request, " +
        "SignedHeaders=" + signedHeadersString + ", " +
        "Signature=" + signature;
}




// These functions allow a caller to directly pass additional headers into
// their GET request.  This is only applicable when using the S3 driver
// directly, as these are not available through the Arbiter.

std::vector<char> S3::getBinary(std::string rawPath, Headers headers) const
{
    std::vector<char> data;
    if (!get(Arbiter::stripType(rawPath), Query(), headers, data))
    {
        throw ArbiterError("Couldn't S3 GET " + rawPath);
    }

    return data;
}

std::string S3::get(std::string rawPath, Headers headers) const
{
    std::vector<char> data(getBinary(rawPath, headers));
    return std::string(data.begin(), data.end());
}

S3::Resource::Resource(std::string fullPath)
    : bucket()
    , object()
{
    fullPath = Http::sanitize(fullPath);
    const std::size_t split(fullPath.find("/"));

    bucket = fullPath.substr(0, split);

    if (split != std::string::npos)
    {
        object = fullPath.substr(split + 1);
    }
}

std::string S3::Resource::buildPath(Query query) const
{
    const std::string queryString(getQueryString(query));
    return "http://" + bucket + baseUrl + object + queryString;
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
} // namespace drivers
} // namespace arbiter

