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

    // Trims sequential whitespace into a single character, and trims all
    // leading and trailing whitespace.
    std::string trimStr(const std::string& in)
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

AZ::AZ(
        Pool& pool,
        std::string profile,
        std::unique_ptr<Config> config)
    : Http(pool)
    , m_profile(profile)
    , m_config(std::move(config))
{ }

std::vector<std::unique_ptr<AZ>> AZ::create(Pool& pool, const std::string s)
{
    std::vector<std::unique_ptr<AZ>> result;

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

std::unique_ptr<AZ> AZ::createOne(Pool& pool, const std::string s)
{
    const json j(s.size() ? json::parse(s) : json());
    const std::string profile(extractProfile(j.dump()));

    std::unique_ptr<Config> config(new Config(j.dump(), profile));
    auto az = makeUnique<AZ>(pool, profile, std::move(config));
    return az;
}

std::string AZ::extractProfile(const std::string s)
{
    const json config(s.size() ? json::parse(s) : json());

    if (
            !config.is_null() &&
            config.count("profile") &&
            config["profile"].get<std::string>().size())
    {
        return config["profile"].get<std::string>();
    }

    if (auto p = env("AZ_PROFILE")) return *p;
    if (auto p = env("AZ_DEFAULT_PROFILE")) return *p;
    else return "default";
}

AZ::Config::Config(const std::string s, const std::string profile)
    : m_service(extractService(s, profile))
    , m_storageAccount(extractStorageAccount(s,profile))
    , m_storageAccessKey(extractStorageAccessKey(s,profile))
    , m_endpoint(extractEndpoint(s, profile))
    , m_baseUrl(extractBaseUrl(s, m_service, m_endpoint, m_storageAccount))
{
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

std::string AZ::Config::extractStorageAccount(
    const std::string s,
    const std::string profile)
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

   throw ArbiterError("Couldn't find Azure Storage account value - this is mandatory");
}

std::string AZ::Config::extractStorageAccessKey(
    const std::string s,
    const std::string profile)
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

    if (!c.is_null() && c.value("verbose", false))
    {
        std::cout << "access key not found - request signin will be disable" << std::endl;
    }

    return "";
}

std::string AZ::Config::extractService(
        const std::string s,
        const std::string profile)
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

    if (!c.is_null() && c.value("verbose", false))
    {
        std::cout << "service not found - defaulting to blob" << std::endl;
    }

    return "blob";
}

std::string AZ::Config::extractEndpoint(
    const std::string s,
    const std::string profile)
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

std::string AZ::type() const
{
    if (m_profile == "default") return "AZ";
    else return m_profile + "@AZ";
}

std::unique_ptr<std::size_t> AZ::tryGetSize(std::string rawPath) const
{
    std::unique_ptr<std::size_t> size;

    Headers headers(m_config->baseHeaders());

    const Resource resource(m_config->baseUrl(), rawPath);
    const ApiV1 ApiV1(
            "HEAD",
            resource,
            m_config->authFields(),
            Query(),
            headers,
            emptyVect);

    drivers::Http http(m_pool);
    Response res(http.internalHead(resource.url(), ApiV1.headers()));

    if (res.ok() && res.headers().count("Content-Length"))
    {
        const std::string& str(res.headers().at("Content-Length"));
        size.reset(new std::size_t(std::stoul(str)));
    }

    return size;
}

bool AZ::get(
        const std::string rawPath,
        std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    Headers headers(m_config->baseHeaders());
    headers.insert(userHeaders.begin(), userHeaders.end());

    std::unique_ptr<std::size_t> size(
            m_config->precheck() && !headers.count("Range") ?
                tryGetSize(rawPath) : nullptr);

    const Resource resource(m_config->baseUrl(), rawPath);
    const ApiV1 ApiV1(
            "GET",
            resource,
            m_config->authFields(),
            query,
            headers,
            emptyVect);

    drivers::Http http(m_pool);
    Response res(
            http.internalGet(
                resource.url(),
                ApiV1.headers(),
                ApiV1.query(),
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

void AZ::put(
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

    const ApiV1 ApiV1(
            "PUT",
            resource,
            m_config->authFields(),
            query,
            headers,
            data);

    drivers::Http http(m_pool);
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
}

void AZ::copy(const std::string src, const std::string dst) const
{
    Headers headers;
    const Resource resource(m_config->baseUrl(), src);
    headers["x-ms-copy-source"] = resource.bucket() + '/' + resource.object();
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
    const std::string& object(resource.object());

    Query query;

    query["restype"] = "container";
    query["comp"] = "list";

    if (object.size()) query["prefix"] = object;

    std::vector<char> data;

    if (verbose) std::cout << "." << std::flush;

    if (!get(resource.bucket() + "/", data, Headers(), query))
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
        if (XmlNode* conNode = topNode->first_node("Blobs"))
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
                                type() + "://" + bucket + "/" + key);
                    }
                }
                else
                {
                    throw ArbiterError(badAZResponse);
                }
            }
        }
        else
        {
            throw ArbiterError(badAZResponse);
        }
    }
    else
    {
            throw ArbiterError(badAZResponse);
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
    msHeaders["x-ms-date"] = m_time.str(Time::iso8601NoSeparators);
    msHeaders["x-ms-version"] = "2019-12-12";

    if (verb == "PUT" || verb == "POST")
    {
        if (!m_headers.count("Content-Type"))
        {
            m_headers["Content-Type"] = "application/octet-stream";
        }
        m_headers.erase("Transfer-Encoding");
        m_headers.erase("Expect");
    }

    const std::string canonicalHeaders(buildCanonicalHeader(msHeaders,m_headers));

    const std::string canonicalResource(buildCanonicalResource(resource, query));

    const std::string stringToSign(buildStringToSign(verb,m_headers,canonicalHeaders,canonicalResource));

    const std::string signature(calculateSignature(stringToSign));

    m_headers["Authorization"] = getAuthHeader(signature);
    m_headers["x-ms-date"] = msHeaders["x-ms-date"];
    m_headers["x-ms-version"] = msHeaders["x-ms-version"];
}

std::string AZ::ApiV1::buildCanonicalHeader(
        http::Headers & msHeaders,
        const http::Headers & existingHeaders) const
{
    for (auto & h : existingHeaders)
    {
        if (h.first.rfind("x-ms-") == 0 || h.first.rfind("Content-MD5") == 0)
        {
            msHeaders[makeLower(h.first)] = trimStr(h.second);
        }
    }
    auto canonicalizeHeaders([](const std::string& s, const Headers::value_type& h)
    {
        const std::string keyVal(
                sanitize(h.first, "") + ":" +
                sanitize(h.second, ""));

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
                sanitize(q.second, ""));

        return s + (s.size() ? "\n" : "") + keyVal;
    });

    const std::string canonicalQuery(
            std::accumulate(
                query.begin(),
                query.end(),
                std::string(),
                canonicalizeQuery));

    return makeLine(canonicalUri) + canonicalQuery;
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
        makeLine(canonicalHeaders) +
        makeLine(headerValues) +
        canonicalRequest;
}

std::string AZ::ApiV1::calculateSignature(
        const std::string& stringToSign) const
{
    return crypto::hmacSha256(crypto::decodeBase64(m_authFields.key()),stringToSign);
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

std::string AZ::Resource::host() const
{
    return m_baseUrl.substr(0, m_baseUrl.size() - 1);
}

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif
