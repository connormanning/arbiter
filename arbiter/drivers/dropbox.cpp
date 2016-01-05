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

    const std::string dirTag("folder");
    const std::string fileTag("file");
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
    else
    {
        std::string message(res.data().data(), res.data().size());
        throw ArbiterError("Server responded with '" + message + "'");
    }

    return false;
}

void Dropbox::put(std::string rawPath, const std::vector<char>& data) const
{
    throw ArbiterError("PUT not yet supported for " + type());
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
    else
    {
        std::string message(res.data().data(), res.data().size());
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
        request["recursive"] = false;
        request["include_media_info"] = false;
        request["include_deleted"] = false;

        std::string f = toSanitizedString(request);

        std::vector<char> postData(f.begin(), f.end());
        HttpResponse res(http.post(listUrl, postData, headers));

        if (res.ok())
        {
            return std::string(res.data().data(), res.data().size());
        }
        else if (res.code() == 409)
        {
            return "";
        }
        else
        {
            std::string message(res.data().data(), res.data().size());
            throw ArbiterError("Server responded with '" + message + "'");
        }
    };

    bool more(false);
    std::string cursor("");

    auto processPath = [verbose, &results, &more, &cursor](std::string data)
    {
        if (data.empty()) return;

        if (verbose) std::cout << '.';

        Json::Value json;
        Json::Reader reader;
        reader.parse(data, json, false);

        const Json::Value& entries(json["entries"]);

        if (entries.isNull())
        {
            throw ArbiterError("Returned JSON from Dropbox was NULL");
        }
        if (!entries.isArray())
        {
            throw ArbiterError("Returned JSON from Dropbox was not an array");
        }

        more = json["has_more"].asBool();
        cursor = json["cursor"].asString();

        for (std::size_t i(0); i < entries.size(); ++i)
        {
            const Json::Value& v(entries[static_cast<Json::ArrayIndex>(i)]);
            const std::string tag(v[".tag"].asString());

            // Only insert files.
            if (std::equal(tag.begin(), tag.end(), fileTag.begin(), ins))
            {
                // Results already begin with a slash.
                results.push_back("dropbox:/" + v["path_lower"].asString());
            }
        }
    };

    processPath(listPath(path));

    if (more)
    {
        do
        {
            processPath(continueFileInfo(cursor));
        }
        while (more);
    }

    return results;
}

} // namespace drivers
} // namespace arbiter

