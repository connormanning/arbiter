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
#include <arbiter/third/json/json.hpp>
#endif

struct iequals
{
    bool operator()( char lhs, char rhs ) const
    {
        return ::tolower( static_cast<unsigned char>( lhs ) )
            == ::tolower( static_cast<unsigned char>( rhs ) );
    }
};

namespace arbiter
{

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
    // get the Dropbox header
    auto it = res.headers().find("original-content-length");

    // if the header isn't there, we can't check
    if (it == res.headers().end())
        return false;

    std::string j(it->second);
    long size = stol(j, 0, 10);

    // if the size doesn't match, we're no good
    if (size != res.data().size())
        return false;

    return true;
}
std::vector<std::string> Dropbox::httpGetHeaders(std::string content_type) const
{
    const std::string authHeader(
            "Authorization: Bearer " + m_auth.token() );
    const std::string content( "Content-Type: " + content_type );
    std::vector<std::string> headers;
    headers.push_back(authHeader);
    headers.push_back(content);
    headers.push_back("Transfer-Encoding: chunked");
    headers.push_back("Expect: 100-continue");
    return headers;
}


std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}


bool Dropbox::get(std::string rawPath, std::vector<char>& data) const
{
    rawPath = Http::sanitize(rawPath);

    //don't pass a content type
    Headers headers(httpGetHeaders(""));

    const std::string baseUrl("https://content.dropboxapi.com/2/files/");
    std::string endpoint = baseUrl + "download";

    auto http(m_pool.acquire());

    Json::Value filename;
    Json::FastWriter writer;
    filename["path"] = std::string("/"+ rawPath);

    std::string f = writer.write(filename) ;
    f.erase(std::remove(f.begin(), f.end(), '\n'), f.end());
    headers.push_back("Dropbox-API-Arg: " + f);

    std::vector<char> postData;
    HttpResponse res(http.post(endpoint, postData, headers));

    if (res.ok())
    {
        data = res.data();
        if (!checkData(res))
            throw ArbiterError("Data size check failed!");
        return true;
    }
    else if (res.client_error() || res.server_error())
    {
        // res.data() can have \0 in it
        std::string message = std::string(res.data().data(), res.data().size());

        std::ostringstream oss;
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

std::string sanitizeJson(Json::Value v)
{
    Json::FastWriter writer;
    std::string f = writer.write(v) ;
    f.erase(std::remove(f.begin(), f.end(), '\n'), f.end());
    return f;

}

std::string Dropbox::continueFileInfo(std::string cursor) const
{
    Headers headers(httpGetHeaders("application/json"));

    std::string endpoint = "https://api.dropboxapi.com/2/files/list_folder/continue";

    auto http(m_pool.acquire());

    Json::Value request;
    request["cursor"] = cursor;
    std::string f = sanitizeJson(request);

    std::vector<char> postData(f.begin(), f.end());
    HttpResponse res(http.post(endpoint, postData, headers));
    std::string output = std::string(res.data().data(), res.data().size());
    if (res.ok())
    {
        std::string output = std::string(res.data().data(), res.data().size());
        return output;
    }
    else if (res.client_error() || res.server_error())
    {
        // res.data() can have \0 in it
        std::string message = std::string(res.data().data(), res.data().size());

        std::ostringstream oss;
        oss << "Server responded with '" << message << "'";
        throw ArbiterError(oss.str());
    }

    return std::string("");
}
std::vector<std::string> Dropbox::glob(std::string rawPath, bool verbose) const
{
    std::vector<std::string> results;
    rawPath = rawPath.substr(0, rawPath.size() -2 );
    rawPath = Http::sanitize(rawPath);

    std::string endpoint = "https://api.dropboxapi.com/2/files/list_folder";

    auto listPath = [endpoint, this , verbose](std::string path) -> std::string
    {
        auto http(this->m_pool.acquire());
        http.verbose(verbose);
        Headers headers(httpGetHeaders("application/json"));

        Json::Value request;
        request["path"] = std::string("/"+ path);
        request["recursive"] = true;
        request["include_media_info"] = false;
        request["include_deleted"] = false;

        std::string f = sanitizeJson(request);

        std::vector<char> postData(f.begin(), f.end());
        HttpResponse res(http.post(endpoint, postData, headers));
        std::string listing;
        if (res.ok())
        {
            listing = std::string(res.data().data(), res.data().size());
        }
        else if (res.client_error() || res.server_error())
        {
            // res.data() can have \0 in it
            std::string message = std::string(res.data().data(), res.data().size());

            std::ostringstream oss;
            oss << "Server responded with '" << message << "'";
            throw ArbiterError(oss.str());
        }
        return listing;
    };

    bool bMore(false);
    auto processPath = [&results, &bMore](std::string json)
    {

        Json::Value root;
        Json::Reader reader;
        reader.parse(json, root, false);

        Json::Value entries = root["entries"];
        if (entries.isNull())
            throw ArbiterError("Returned JSON from Dropbox was NULL!");
        if (!entries.isArray())
            throw ArbiterError("Returned JSON from Dropbox was not an array!");
        bMore = root["has_more"].asBool();

        for(int i = 0; i < entries.size(); ++i)
        {
            Json::Value& v = entries[i];
            std::string tag = v[".tag"].asString();

            std::string file("file");
            std::string folder("folder");
            if (std::equal( file.begin(), file.end(), tag.begin(), iequals() ))
            {
                results.push_back(v["path_lower"].asString());
            }
        }

    };

    std::string listing = listPath(rawPath);
    processPath(listing);
    if (bMore)
    {
        do
        {
            listing = continueFileInfo("");
            processPath(listing);

        }
        while (bMore);
    }
    return results;
}
} // namespace drivers
} // namespace arbiter

