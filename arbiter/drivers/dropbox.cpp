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

#ifndef ARBITER_EXTERNAL_JSON
#include <arbiter/third/json/json.hpp>
#endif

#endif



#ifdef ARBITER_EXTERNAL_JSON
#include <json/json.h>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    const std::string baseGetUrl("https://content.dropboxapi.com/");
    const std::string getUrlV1(baseGetUrl + "1/files/auto/");
    const std::string getUrlV2(baseGetUrl + "2/files/download");

    // We still need to use API V1 for GET requests since V2 is poorly
    // documented and doesn't correctly support the Range header.  Hopefully
    // we can switch to V2 at some point.
    const bool legacy(true);

    const std::string listUrl("https://api.dropboxapi.com/2/files/list_folder");
    const std::string metaUrl("https://api.dropboxapi.com/2/files/get_metadata");
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

    if (!json.isNull() && json.isMember("token"))
    {
        dropbox.reset(new Dropbox(pool, DropboxAuth(json["token"].asString())));
    }

    return dropbox;
}

Headers Dropbox::httpGetHeaders() const
{
    Headers headers;

    headers["Authorization"] = "Bearer " + m_auth.token();

    if (!legacy)
    {
        headers["Transfer-Encoding"] = "";
        headers["Expect"] = "";
    }

    return headers;
}

Headers Dropbox::httpPostHeaders() const
{
    Headers headers;

    headers["Authorization"] = "Bearer " + m_auth.token();
    headers["Transfer-Encoding"] = "chunked";
    headers["Expect"] = "100-continue";
    headers["Content-Type"] = "application/json";

    return headers;
}

bool Dropbox::get(const std::string rawPath, std::vector<char>& data) const
{
    return buildRequestAndGet(rawPath, data);
}

std::unique_ptr<std::size_t> Dropbox::tryGetSize(
        const std::string rawPath) const
{
    std::unique_ptr<std::size_t> result;

    Headers headers(httpPostHeaders());

    Json::Value json;
    json["path"] = std::string("/" + Http::sanitize(rawPath));
    const auto f(toSanitizedString(json));
    const std::vector<char> postData(f.begin(), f.end());

    auto http(m_pool.acquire());
    HttpResponse res(http.post(metaUrl, postData, headers));

    if (res.ok())
    {
        const auto data(res.data());

        Json::Value json;
        Json::Reader reader;
        reader.parse(std::string(data.data(), data.size()), json, false);

        if (json.isMember("size"))
        {
            result.reset(new std::size_t(json["size"].asUInt64()));
        }
    }

    return result;
}

bool Dropbox::buildRequestAndGet(
        const std::string rawPath,
        std::vector<char>& data,
        const Headers userHeaders) const
{
    const std::string path(Http::sanitize(rawPath));

    Headers headers(httpGetHeaders());

    if (!legacy)
    {
        Json::Value json;
        json["path"] = std::string("/" + path);
        headers["Dropbox-API-Arg"] = toSanitizedString(json);
    }

    headers.insert(userHeaders.begin(), userHeaders.end());

    auto http(m_pool.acquire());

    HttpResponse res(
            legacy ?
                http.get(getUrlV1 + path, headers) :
                http.get(getUrlV2, headers));

    if (res.ok())
    {
        if (
                (legacy && !res.headers().count("Content-Length")) ||
                (!legacy && !res.headers().count("original-content-length")))
        {
            return false;
        }

        const std::size_t size(
                std::stol(
                    legacy ?
                        res.headers().at("Content-Length") :
                        res.headers().at("original-content-length")));

        data = res.data();

        if (size == res.data().size())
        {
            return true;
        }
        else
        {
            throw ArbiterError(
                    "Data size check failed - got " + std::to_string(size) +
                    " of " + std::to_string(res.data().size()) + " bytes.");
        }
    }
    else
    {
        std::string message(res.data().data(), res.data().size());
        throw ArbiterError(
                "Server response: " + std::to_string(res.code()) + " - '" +
                message + "'");
    }

    return false;
}

void Dropbox::put(std::string rawPath, const std::vector<char>& data) const
{
    throw ArbiterError("PUT not yet supported for " + type());
}

std::string Dropbox::continueFileInfo(std::string cursor) const
{
    Headers headers(httpPostHeaders());

    auto http(m_pool.acquire());

    Json::Value json;
    json["cursor"] = cursor;
    const std::string f(toSanitizedString(json));

    std::vector<char> postData(f.begin(), f.end());
    HttpResponse res(http.post(continueListUrl, postData, headers));

    if (res.ok())
    {
        return std::string(res.data().data(), res.data().size());
    }
    else
    {
        std::string message(res.data().data(), res.data().size());
        throw ArbiterError(
                "Server response: " + std::to_string(res.code()) + " - '" +
                message + "'");
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
        Headers headers(httpPostHeaders());

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
            throw ArbiterError(
                    "Server response: " + std::to_string(res.code()) + " - '" +
                    message + "'");
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



// These functions allow a caller to directly pass additional headers into
// their GET request.  This is only applicable when using the Dropbox driver
// directly, as these are not available through the Arbiter.

std::vector<char> Dropbox::getBinary(std::string rawPath, Headers headers) const
{
    std::vector<char> data;
    const std::string stripped(Arbiter::stripType(rawPath));
    if (!buildRequestAndGet(stripped, data, headers))
    {
        throw ArbiterError("Couldn't Dropbox GET " + rawPath);
    }

    return data;
}

std::string Dropbox::get(std::string rawPath, Headers headers) const
{
    std::vector<char> data(getBinary(rawPath, headers));
    return std::string(data.begin(), data.end());
}

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

