#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/drivers/dropbox.hpp>
#include <arbiter/third/xml/xml.hpp>
#include <arbiter/util/crypto.hpp>
#include <arbiter/third/json/json.hpp>
#endif

namespace arbiter
{

namespace
{
    const std::string getUrl("https://content.dropboxapi.com/2/files/download");
    const std::string listUrl("https://api.dropboxapi.com/2/files/list_folder");
    const std::string continueListUrl(listUrl + "/continue");

    const auto ins([](unsigned char lhs, unsigned char rhs)
    {
        return std::tolower(lhs) == std::tolower(rhs);
    });

    std::string toSanitizedString(const Json::Value& v)
    {
        Json::FastWriter writer;
        std::string f(writer.write(v));
        f.erase(std::remove(f.begin(), f.end(), '\n'), f.end());
        return f;
    }
}

namespace drivers
{

Dropbox::Dropbox(HttpPool& pool, const DropboxAuth auth)
    : m_pool(pool)
    , m_auth(auth)
{ }

std::unique_ptr<Dropbox> Dropbox::create(
        HttpPool& pool,
        const Json::Value& json)
{
    std::unique_ptr<Dropbox> dropbox;

    if (json.isMember("token"))
    {
        dropbox.reset(new Dropbox(pool, DropboxAuth(json["token"].asString())));
    }

    return dropbox;
}

Headers Dropbox::httpGetHeaders(const std::string contentType) const
{
    Headers headers;

    headers["Authorization"] = "Bearer " + m_auth.token();
    headers["Transfer-Encoding"] = "chunked";
    headers["Expect"] = "100-continue";
    headers["Content-Type"] = contentType;

    return headers;
}

bool Dropbox::get(const std::string rawPath, std::vector<char>& data) const
{
    const std::string path(Http::sanitize(rawPath));
    Headers headers(httpGetHeaders());
    auto http(m_pool.acquire());

    Json::Value json;
    json["path"] = std::string("/" + path);
    headers["Dropbox-API-Arg"] = toSanitizedString(json);

    std::vector<char> postData;
    HttpResponse res(http.post(getUrl, postData, headers));

    if (res.ok())
    {
        if (!res.headers().count("original-content-length")) return false;

        data = res.data();

        const std::size_t size(
                std::stol(res.headers().at("original-content-length")));

        if (size == res.data().size()) return true;
        else throw ArbiterError("Data size check failed");
    }
    else if (res.clientError() || res.serverError())
    {
        std::string message(std::string(res.data().data(), res.data().size()));
        throw ArbiterError("Server responded with '" + message + "'");
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

std::string Dropbox::continueFileInfo(std::string cursor) const
{
    Headers headers(httpGetHeaders("application/json"));

    auto http(m_pool.acquire());

    Json::Value json;
    json["cursor"] = cursor;
    std::string f = toSanitizedString(json);

    std::vector<char> postData(f.begin(), f.end());
    HttpResponse res(http.post(continueListUrl, postData, headers));

    if (res.ok())
    {
        return std::string(res.data().data(), res.data().size());
    }
    else if (res.clientError() || res.serverError())
    {
        std::string message(std::string(res.data().data(), res.data().size()));
        throw ArbiterError("Server responded with '" + message + "'");
    }

    return std::string("");
}

std::vector<std::string> Dropbox::glob(std::string rawPath, bool verbose) const
{
    std::vector<std::string> results;

    const std::string path(
            Http::sanitize(rawPath.substr(0, rawPath.size() - 2)));

    auto listPath = [this](std::string path)->std::string
    {
        auto http(m_pool.acquire());
        Headers headers(httpGetHeaders("application/json"));

        Json::Value request;
        request["path"] = std::string("/" + path);
        request["recursive"] = true;
        request["include_media_info"] = false;
        request["include_deleted"] = false;

        std::string f = toSanitizedString(request);

        std::vector<char> postData(f.begin(), f.end());
        HttpResponse res(http.post(listUrl, postData, headers));
        std::string listing;
        if (res.ok())
        {
            listing = std::string(res.data().data(), res.data().size());
        }
        else if (res.clientError() || res.serverError())
        {
            std::string message(std::string(res.data().data(), res.data().size()));
            throw ArbiterError("Server responded with '" + message + "'");
        }

        return listing;
    };

    bool more(false);
    auto processPath = [&results, &more](std::string json)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(json, root, false);

        Json::Value entries = root["entries"];
        if (entries.isNull())
            throw ArbiterError("Returned JSON from Dropbox was NULL");
        if (!entries.isArray())
            throw ArbiterError("Returned JSON from Dropbox was not an array");
        more = root["has_more"].asBool();

        for(int i = 0; i < entries.size(); ++i)
        {
            Json::Value& v = entries[i];
            std::string tag = v[".tag"].asString();

            std::string file("file");
            std::string folder("folder");

            if (std::equal(file.begin(), file.end(), tag.begin(), ins))
            {
                results.push_back(v["path_lower"].asString());
            }
        }
    };

    processPath(listPath(path));

    if (more)
    {
        do
        {
            processPath(continueFileInfo(""));
        }
        while (more);
    }

    return results;
}

} // namespace drivers
} // namespace arbiter

