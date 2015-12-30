#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/drivers/dropbox.hpp>
#include <arbiter/third/xml/xml.hpp>
#include <arbiter/util/crypto.hpp>
#endif

namespace arbiter
{

namespace
{
    const std::string baseUrl("https://content.dropboxapi.com/2/files/");

    std::string getQueryString(const drivers::Query& query)
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

    struct Resource
    {
        Resource(std::string fullPath)
        {
            const std::size_t split(fullPath.find("/"));

            bucket = fullPath.substr(0, split);

            if (split != std::string::npos)
            {
                object = fullPath.substr(split + 1);
            }
        }

        std::string buildPath(drivers::Query query = drivers::Query()) const
        {
            const std::string queryString(getQueryString(query));
            return "http://" + bucket + baseUrl + object + queryString;
        }

        std::string bucket;
        std::string object;
    };

    typedef Xml::xml_node<> XmlNode;

}

namespace drivers
{

DropboxAuth::DropboxAuth(const std::string token)
    : m_token(token)
{ }

std::unique_ptr<DropboxAuth> DropboxAuth::find(std::string token)
{
    std::unique_ptr<DropboxAuth> auth(new DropboxAuth(token));
    return auth;
}

std::string DropboxAuth::token() const
{
    return m_token;
}

Dropbox::Dropbox(HttpPool& pool, const DropboxAuth auth)
    : m_pool(pool)
    , m_auth(auth)
{ }

bool checkData(HttpResponse res)
{
    // get the Dropbox dropbox-api-result header


    // Check that res.data().size() == original size

    return true;
}
std::vector<std::string> Dropbox::httpGetHeaders(std::string filePath) const
{
    const std::string authHeader(
            "Authorization: Bearer " + m_auth.token() );
    const std::string content( "Content-Type: " );
    std::vector<std::string> headers;
    headers.push_back(authHeader);
    headers.push_back(content);
    headers.push_back("Transfer-Encoding: chunked");
    return headers;
}
bool Dropbox::get(std::string rawPath, std::vector<char>& data) const
{
    rawPath = Http::sanitize(rawPath);
    Headers headers(httpGetHeaders(rawPath));
    std::string endpoint = baseUrl + "download";
//
    auto http(m_pool.acquire());


    std::ostringstream filename;
    filename <<  "{\"path\": \"/" << rawPath << "\"}";
    headers.push_back("Dropbox-API-Arg: " + filename.str());

    std::vector<char> postData;
    HttpResponse res(http.post(endpoint, postData, headers));
//     for(auto i: res.headers())
//     {
//         std::cout << "header: '" << i << "'" << std::endl;
//     }

    if (res.ok())
    {
        data = res.data();
        checkData(res);
        return true;
    }
    else if (res.client_error() || res.server_error())
    {
        std::ostringstream oss;
        std::string message = std::string(res.data().data(), res.data().size());
        oss << "Server responded with '" << message << "'";
        throw ArbiterError(oss.str());
    }
    return false;
}

void Dropbox::put(std::string rawPath, const std::vector<char>& data) const
{
//     const Resource resource(rawPath);
//
//     const std::string path(resource.buildPath());
//     const Headers headers(httpPutHeaders(rawPath));
//
//     auto http(m_pool.acquire());
//
//     if (!http.put(path, data, headers).ok())
//     {
//         throw ArbiterError("Couldn't Dropbox PUT to " + rawPath);
//     }
}

std::vector<std::string> Dropbox::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;
    std::string rawPath("project_1/extrabytes.las");
    rawPath = Http::sanitize(rawPath);

    Headers headers(httpGetHeaders(rawPath));
    std::string endpoint = "https://api.dropboxapi.com/2-beta-2/files/get_metadata";
//
    auto http(m_pool.acquire());
    std::string asdf = "{\"path\": \"/project_1/extrabytes.las\"}";
    HttpResponse res(http.post(endpoint, std::vector<char>(asdf.begin(), asdf.end()), headers));
//     if (res.ok())
//     {
//         data = res.data();
//         std::string output = std::string(data.begin(), data.end());
//         return true;
//     }
//     else
//     {
//         return false;
//     }
//     path.pop_back();
//
//     // https://docs.aws.amazon.com/AmazonDropbox/latest/API/RESTBucketGET.html
//     const Resource resource(path);
//     const std::string& bucket(resource.bucket);
//     const std::string& object(resource.object);
//     const std::string prefix(resource.object.empty() ? "" : resource.object);
//
//     Query query;
//
//     if (prefix.size()) query["prefix"] = prefix;
//
//     bool more(false);
//
//     do
//     {
//         if (verbose) std::cout << "." << std::flush;
//
//         auto data = get(resource.bucket + "/", query);
//         data.push_back('\0');
//
//         Xml::xml_document<> xml;
//
//         try
//         {
//             xml.parse<0>(data.data());
//         }
//         catch (Xml::parse_error)
//         {
//             throw ArbiterError("Could not parse Dropbox response.");
//         }
//
//         if (XmlNode* topNode = xml.first_node("ListBucketResult"))
//         {
//             if (XmlNode* truncNode = topNode->first_node("IsTruncated"))
//             {
//                 std::string t(truncNode->value());
//                 std::transform(t.begin(), t.end(), t.begin(), tolower);
//
//                 more = (t == "true");
//             }
//
//             if (XmlNode* conNode = topNode->first_node("Contents"))
//             {
//                 for ( ; conNode; conNode = conNode->next_sibling())
//                 {
//                     if (XmlNode* keyNode = conNode->first_node("Key"))
//                     {
//                         std::string key(keyNode->value());
//
//                         // The prefix may contain slashes (i.e. is a sub-dir)
//                         // but we only include the top level after that.
//                         if (key.find('/', prefix.size()) == std::string::npos)
//                         {
//                             results.push_back("s3://" + bucket + "/" + key);
//
//                             if (more)
//                             {
//                                 query["marker"] =
//                                     object + key.substr(prefix.size());
//                             }
//                         }
//                     }
//                     else
//                     {
//                         throw ArbiterError(badResponse);
//                     }
//                 }
//             }
//             else
//             {
//                 throw ArbiterError(badResponse);
//             }
//         }
//         else
//         {
//             throw ArbiterError(badResponse);
//         }
//
//         xml.clear();
//     }
//     while (more);
//
    return results;
}
} // namespace drivers
} // namespace arbiter

