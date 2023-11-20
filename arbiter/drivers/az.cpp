#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/az.hpp>
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
    std::string makeLine(const std::string& data) { return data + "\n"; }
    const std::vector<char> emptyVect;

    typedef Xml::xml_node<> XmlNode;
    const std::string badAZResponse("Unexpected contents in Azure response");

    std::string makeLower(const std::string& in)
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
}

namespace drivers
{

using namespace http;

AZ::AZ(
        Pool& pool,
        std::string profile,
        std::unique_ptr<Config> config)
    : Http(pool, "az", "http", profile == "default" ? "" : profile)
    , m_config(std::move(config))
{ }

std::unique_ptr<AZ> AZ::create(
    Pool& pool,
    const std::string s,
    std::string profile)
{
    if (profile.empty()) profile = "default";
    if (auto p = env("AZ_DEFAULT_PROFILE")) profile = *p;
    if (auto p = env("AZ_PROFILE")) profile = *p;

    std::unique_ptr<Config> config(new Config(s));
    return makeUnique<AZ>(pool, profile, std::move(config));
}

AZ::Config::Config(const std::string s)
    : m_service(extractService(s))
    , m_storageAccount(extractStorageAccount(s))
    , m_storageAccessKey(extractStorageAccessKey(s))
    , m_endpoint(extractEndpoint(s))
    , m_baseUrl(extractBaseUrl(s, m_service, m_endpoint, m_storageAccount))
{
    const std::string sasString = extractSasToken(s);
    if (!sasString.empty())
    {
        const auto params = split(sasString, '&');
        for (const auto& param : params)
        {
            const auto kv = split(param, '=');
            m_sasToken[kv.at(0)] = kv.at(1);
        }
    }

    const json c(s.size() ? json::parse(s) : json());
    if (c.is_null()) return;

    m_precheck = c.value("precheck", false);

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
            std::cout << "AZ.headers expected to be object - skipping" <<
                std::endl;
        }
    }
}

std::string AZ::Config::extractStorageAccount(const std::string s)
{
    const json c(s.size() ? json::parse(s) : json());

    if (!c.is_null() && c.count("account"))
    {
        return c.at("account").get<std::string>();
    }
    else if (auto p = env("AZURE_STORAGE_ACCOUNT"))
    {
        return *p;
    }
    else if (auto p = env("AZ_STORAGE_ACCOUNT"))
    {
        return *p;
    }

   throw ArbiterError("Couldn't find Azure Storage account value - this is mandatory");
}

std::string AZ::Config::extractStorageAccessKey(const std::string s)
{
    const json c(s.size() ? json::parse(s) : json());

    if (!c.is_null() && c.count("key"))
    {
        return c.at("key").get<std::string>();
    }
    else if (auto p = env("AZURE_STORAGE_ACCESS_KEY"))
    {
        return *p;
    }
    else if (auto p = env("AZ_STORAGE_ACCESS_KEY"))
    {
        return *p;
    }

    if (!c.is_null() && c.value("verbose", false))
    {
        std::cout << "access key not found - request signin will be disable" << std::endl;
    }

    return "";
}

std::string AZ::Config::extractSasToken(const std::string s)
{
    const json c(s.size() ? json::parse(s) : json());

    if (!c.is_null() && c.count("sas"))
    {
        return c.at("sas").get<std::string>();
    }
    else if (auto p = env("AZURE_SAS_TOKEN"))
    {
        return *p;
    }
    else if (auto p = env("AZ_SAS_TOKEN"))
    {
        return *p;
    }
    return "";
}

std::string AZ::Config::extractService(const std::string s)
{
    const json c(s.size() ? json::parse(s) : json());

    if (!c.is_null() && c.count("service"))
    {
        return c.at("service").get<std::string>();
    }
    else if (auto p = env("AZURE_SERVICE"))
    {
        return *p;
    }
    else if (auto p = env("AZURE_DEFAULT_SERVICE"))
    {
        return *p;
    }
    else if (auto p = env("AZ_SERVICE"))
    {
        return *p;
    }
    else if (auto p = env("AZ_DEFAULT_SERVICE"))
    {
        return *p;
    }

    if (!c.is_null() && c.value("verbose", false))
    {
        std::cout << "service not found - defaulting to blob" << std::endl;
    }

    return "blob";
}

std::string AZ::Config::extractEndpoint(const std::string s)
{
    const json c(s.size() ? json::parse(s) : json());

    if (!c.is_null() && c.count("endpoint"))
    {
        return c.at("endpoint").get<std::string>();
    }
    else if (auto p = env("AZURE_ENDPOINT"))
    {
        return *p;
    }
    else if (auto p = env("AZ_ENDPOINT"))
    {
        return *p;
    }

    if (!c.is_null() && c.value("verbose", false))
    {
        std::cout << "endpoint not found - defaulting to core.windows.net" << std::endl;
    }

    return "core.windows.net";
}

std::string AZ::Config::extractBaseUrl(
     const std::string s,
     const std::string service,
     const std::string endpoint,
     const std::string account)
{
    return account + "." + service + "." + endpoint + "/";
}

std::unique_ptr<std::size_t> AZ::tryGetSize(
    const std::string rawPath,
    const http::Headers userHeaders,
    const http::Query query) const
{
    Headers headers(m_config->baseHeaders());

    drivers::Http http(m_pool);
    const Resource resource(m_config->baseUrl(), rawPath);
    std::unique_ptr<Response> res;

    if (m_config->hasSasToken())
    {
        Query q = m_config->sasToken();
        q.insert(std::begin(query), std::end(query));
        res.reset(new Response(http.internalHead(resource.url(), headers, q)));
    }
    else
    {
        const ApiV1 ApiV1(
                "HEAD",
                resource,
                m_config->authFields(),
                query,
                headers,
                emptyVect);
        res.reset(new Response(http.internalHead(resource.url(), ApiV1.headers())));
    }

    if (res->ok())
    {
        const auto cl = findHeader(res->headers(), "Content-Length");
        if (cl) return makeUnique<std::size_t>(std::stoull(*cl));
    }

    return std::unique_ptr<std::size_t>();
}

bool AZ::get(
        const std::string rawPath,
        std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    Headers headers(m_config->baseHeaders());
    headers.insert(userHeaders.begin(), userHeaders.end());

    const Resource resource(m_config->baseUrl(), rawPath);
    drivers::Http http(m_pool);

    std::unique_ptr<Response> res;

    if (m_config->hasSasToken())
    {
        Query q = m_config->sasToken();
        q.insert(query.begin(), query.end());
        res.reset(new Response(http.internalGet(resource.url(), headers, q)));
    }
    else
    {
        const ApiV1 ApiV1(
                "GET",
                resource,
                m_config->authFields(),
                query,
                headers,
                emptyVect);

        res.reset(
            new Response(
                http.internalGet(
                    resource.url(),
                    ApiV1.headers(),
                    ApiV1.query())));
    }

    if (res->ok())
    {
        data = res->data();
        return true;
    }
    else
    {
        std::cout << res->code() << ": " << res->str() << std::endl;
        return false;
    }
}

std::vector<char> AZ::put(
        const std::string rawPath,
        const std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    const Resource resource(m_config->baseUrl(), rawPath);

    Headers headers(m_config->baseHeaders());
    headers.insert(userHeaders.begin(), userHeaders.end());


    drivers::Http http(m_pool);

    if (m_config->hasSasToken())
    {
        Headers headers(userHeaders);
        headers["Content-Type"] = "application/octet-stream";
        if (getExtension(rawPath) == "json")
        {
            headers["Content-Type"] = "application/json";
        }
        headers["Content-Length"] = std::to_string(data.size());
        headers["x-ms-blob-type"] = "BlockBlob";

        Query q = m_config->sasToken();
        q.insert(query.begin(), query.end());

        Response res(
            http.internalPut(
                resource.url(),
                data,
                headers,
                q));

        if (!res.ok())
        {
            throw ArbiterError(
                    "Couldn't Azure PUT to " + rawPath + ": " +
                    std::string(res.data().data(), res.data().size()));
        }
        return res.data();
    }

    const ApiV1 ApiV1(
            "PUT",
            resource,
            m_config->authFields(),
            query,
            headers,
            data);

    Response res(
            http.internalPut(
                resource.url(),
                data,
                ApiV1.headers(),
                ApiV1.query()));

    if (!res.ok())
    {
        throw ArbiterError(
                "Couldn't Azure PUT to " + rawPath + ": " +
                std::string(res.data().data(), res.data().size()));
    }

    return res.data();
}

void AZ::copy(const std::string src, const std::string dst) const
{
    Headers headers;
    const Resource resource(m_config->baseUrl(), src);
    headers["x-ms-copy-source"] = resource.object();
    put(dst, std::vector<char>(), headers, Query());
}

std::vector<std::string> AZ::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;
    path.pop_back();

    const bool recursive(path.back() == '*');
    if (recursive) path.pop_back();

    const Resource resource(m_config->baseUrl(), path);
    const std::string& bucket(resource.bucket());
    const std::string& object(resource.blob());

    Query query;

    query["restype"] = "container";
    query["comp"] = "list";

    if (object.size()) query["prefix"] = object;

    std::vector<char> data;

    if (verbose) std::cout << "." << std::flush;

    if (!get(resource.bucket(), data, Headers(), query))
    {
        throw ArbiterError("Couldn't AZ GET " + resource.bucket());
    }

    data.push_back('\0');

    Xml::xml_document<> xml;

    try
    {
        xml.parse<0>(data.data());
    }
    catch (Xml::parse_error&)
    {
        throw ArbiterError("Could not parse AZ response.");
    }
    //read https://docs.microsoft.com/en-us/rest/api/storageservices/list-blobs
    if (XmlNode* topNode = xml.first_node("EnumerationResults"))
    {
        if (XmlNode* blobsNode = topNode->first_node("Blobs"))
        {
            if (XmlNode* conNode = blobsNode->first_node("Blob"))
            {
                for ( ; conNode; conNode = conNode->next_sibling())
                {
                    if (XmlNode* keyNode = conNode->first_node("Name"))
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
                    }
                }
            }
        }
        else
        {
                throw ArbiterError("No blobs node");
        }
    }
    else
    {
            throw ArbiterError("No EnumerationResults node");
    }

    xml.clear();

    return results;
}

//read https://docs.microsoft.com/en-us/rest/api/storageservices/operations-on-blobs
AZ::ApiV1::ApiV1(
        const std::string verb,
        const Resource& resource,
        const AZ::AuthFields authFields,
        const Query& query,
        const Headers& headers,
        const std::vector<char>& data)
    : m_authFields(authFields)
    , m_time()
    , m_headers(headers)
    , m_query(query)
{
    Headers msHeaders;
    msHeaders["x-ms-date"] = m_time.str(Time::rfc822);
    msHeaders["x-ms-version"] = "2019-12-12";

    if (verb == "PUT" || verb == "POST")
    {
        if (!findHeader(m_headers, "Content-Type"))
        {
            m_headers["Content-Type"] = "application/octet-stream";
        }
        m_headers["Content-Length"] = std::to_string(data.size());
        m_headers.erase("Transfer-Encoding");
        m_headers.erase("Expect");
        msHeaders["x-ms-blob-type"] = "BlockBlob";
    }

    const std::string canonicalHeaders(buildCanonicalHeader(msHeaders,m_headers));

    const std::string canonicalResource(buildCanonicalResource(resource, query));

    const std::string stringToSign(buildStringToSign(verb,m_headers,canonicalHeaders,canonicalResource));

    const std::string signature(calculateSignature(stringToSign));

    m_headers["Authorization"] = getAuthHeader(signature);
    m_headers["x-ms-date"] = msHeaders["x-ms-date"];
    m_headers["x-ms-version"] = msHeaders["x-ms-version"];
    m_headers["x-ms-blob-type"] = msHeaders["x-ms-blob-type"];
}

std::string AZ::ApiV1::buildCanonicalHeader(
        http::Headers & msHeaders,
        const http::Headers & existingHeaders) const
{
    auto trim([](const std::string& s)
    {
        const std::string whitespace = " \t\r\n";
        const size_t left = s.find_first_not_of(whitespace);
        const size_t right = s.find_first_of(whitespace);
        if (left == std::string::npos)
        {
            return std::string();
        }
        return s.substr(left,right - left +1);
    });

    for (auto & h : existingHeaders)
    {
        if (h.first.rfind("x-ms-") == 0 || h.first.rfind("Content-MD5") == 0)
        {
            msHeaders[makeLower(h.first)] = trim(h.second);
        }
    }
    auto canonicalizeHeaders([](const std::string& s, const Headers::value_type& h)
    {
        const std::string keyVal(h.first + ":" + h.second);

        return s + (s.size() ? "\n" : "") + keyVal;
    });

   const std::string canonicalHeader(
   std::accumulate(
       msHeaders.begin(),
       msHeaders.end(),
       std::string(),
       canonicalizeHeaders));

   return  canonicalHeader;
}

std::string AZ::ApiV1::buildCanonicalResource(
        const Resource& resource,
        const Query& query) const
{
    const std::string canonicalUri("/" + resource.storageAccount() + "/" + resource.object());

    auto canonicalizeQuery([](const std::string& s, const Query::value_type& q)
    {
        const std::string keyVal(
                sanitize(q.first, "") + ":" +
                q.second);

        return s + "\n" + keyVal;
    });

    const std::string canonicalQuery(
            std::accumulate(
                query.begin(),
                query.end(),
                std::string(),
                canonicalizeQuery));

    return canonicalUri + canonicalQuery;
}

std::string AZ::ApiV1::buildStringToSign(
            const std::string& verb,
            const http::Headers& headers,
            const std::string& canonicalHeaders,
            const std::string& canonicalRequest) const
{
    http::Headers h(headers);
    std::string headerValues;
    headerValues += makeLine(h["Content-Encoding"]);
    headerValues += makeLine(h["Content-Language"]);

    if (h["Content-Length"] == "0")
        headerValues += makeLine("");
    else
        headerValues += makeLine(h["Content-Length"]);

    headerValues += makeLine(h["Content-MD5"]);
    headerValues += makeLine(h["Content-Type"]);
    headerValues += makeLine(h["Date"]);
    headerValues += makeLine(h["If-Modified-Since"]);
    headerValues += makeLine(h["If-Match"]);
    headerValues += makeLine(h["If-None-Match"]);
    headerValues += makeLine(h["If-Unmodified-Since"]);
    headerValues += h["Range"];


    return
        makeLine(verb) +
        makeLine(headerValues) +
        makeLine(canonicalHeaders) +
        canonicalRequest;
}

std::string AZ::ApiV1::calculateSignature(
        const std::string& stringToSign) const
{
    return crypto::encodeBase64(crypto::hmacSha256(crypto::decodeBase64(m_authFields.key()),stringToSign));
}

std::string AZ::ApiV1::getAuthHeader(
        const std::string& signedHeadersString) const
{
    return "SharedKey " +
         m_authFields.account() + ":" +
            signedHeadersString;
}

AZ::Resource::Resource(std::string base, std::string fullPath)
    : m_baseUrl(base)
    , m_bucket()
    , m_object()
{
    fullPath = sanitize(fullPath);
    const std::size_t split(fullPath.find("/"));

    m_bucket = fullPath.substr(0, split);
    if (split != std::string::npos) m_object = fullPath.substr(split + 1);

    base = sanitize(base);
    const std::size_t urlSplit(base.find("."));
    m_storageAccount = base.substr(0,urlSplit);
}

std::string AZ::Resource::storageAccount() const
{
    return m_storageAccount;
}

std::string AZ::Resource::baseUrl() const
{
    return m_baseUrl;
}

std::string AZ::Resource::bucket() const
{
    return m_bucket;
}

std::string AZ::Resource::url() const
{
    return "https://" + m_baseUrl + m_bucket + "/" + m_object;
}

std::string AZ::Resource::object() const
{
    return m_bucket + "/" + m_object;
}

std::string AZ::Resource::blob() const
{
    return m_object;
}

std::string AZ::Resource::host() const
{
    return m_baseUrl.substr(0, m_baseUrl.size() - 1);
}

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif
