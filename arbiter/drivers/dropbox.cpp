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
#include <arbiter/util/json.hpp>
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
    const std::string baseDropboxUrl("https://content.dropboxapi.com/");
    const std::string getUrl(baseDropboxUrl + "2/files/download");
    const std::string putUrl(baseDropboxUrl + "2/files/upload");

    const std::string listUrl("https://api.dropboxapi.com/2/files/list_folder");
    const std::string metaUrl("https://api.dropboxapi.com/2/files/get_metadata");
    const std::string continueListUrl(listUrl + "/continue");

    const auto ins([](unsigned char lhs, unsigned char rhs)
    {
        return std::tolower(lhs) == std::tolower(rhs);
    });

    const std::string dirTag("folder");
    const std::string fileTag("file");
}

namespace drivers
{

using namespace http;

Dropbox::Dropbox(
        Pool& pool,
        const Dropbox::Auth& auth,
        const std::string profile)
    : Http(pool, "dbx", profile)
    , m_auth(auth)
{ }

std::unique_ptr<Dropbox> Dropbox::create(
    Pool& pool,
    const std::string s,
    const std::string profile)
{
    const json j(json::parse(s));
    if (!j.is_null())
    {
        if (j.is_object() && j.count("token"))
        {
            return makeUnique<Dropbox>(
                    pool,
                    Auth(j.at("token").get<std::string>()),
                    profile);
        }
        else if (j.is_string())
        {
            return makeUnique<Dropbox>(
                    pool,
                    Auth(j.get<std::string>()),
                    profile);
        }
    }

    return std::unique_ptr<Dropbox>();
}

Headers Dropbox::httpGetHeaders() const
{
    Headers headers;

    headers["Authorization"] = "Bearer " + m_auth.token();

    headers["Transfer-Encoding"] = "";
    headers["Expect"] = "";

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

std::unique_ptr<std::size_t> Dropbox::tryGetSize(const std::string path) const
{
    std::unique_ptr<std::size_t> result;

    Headers headers(httpPostHeaders());

    json tx { { "path", "/" + path } };
    const std::string f(tx.dump());
    const std::vector<char> postData(f.begin(), f.end());

    Response res(Http::internalPost(metaUrl, postData, headers));

    if (res.ok())
    {
        const auto data(res.data());

        json rx(json::parse(std::string(data.data(), data.size())));
        if (rx.count("size"))
        {
            result = makeUnique<std::size_t>(rx.at("size").get<uint64_t>());
        }
    }

    return result;
}

bool Dropbox::get(
        const std::string path,
        std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    Headers headers(httpGetHeaders());

    headers["Dropbox-API-Arg"] = json{{ "path", "/" + path }}.dump();
    headers.insert(userHeaders.begin(), userHeaders.end());

    const Response res(Http::internalGet(getUrl, headers, query));

    if (res.ok())
    {
        if (!userHeaders.count("Range"))
        {
            if (!res.headers().count("dropbox-api-result"))
            {
                std::cout << "No dropbox-api-result header found" << std::endl;
                return false;
            }

            json rx;
            try { rx = json::parse(res.headers().at("dropbox-api-result")); }
            catch (...) { std::cout << "Failed to parse result" << std::endl; }

            if (!rx.is_null())
            {
                if (!rx.count("size"))
                {
                    std::cout << "No size found in API result" << std::endl;
                    return false;
                }

                const std::size_t size(rx.at("size").get<std::size_t>());
                data = res.data();

                if (size == data.size()) return true;
                else
                {
                    std::cout <<
                            "Data size check failed - got " <<
                            size << " of " << data.size() << " bytes." <<
                            std::endl;
                }
            }
        }
        else
        {
            data = res.data();
            return true;
        }
    }
    else
    {
        const auto data(res.data());
        std::string message(data.data(), data.size());

        std::cout <<
                "Server response: " << res.code() << " - '" << message << "'" <<
                std::endl;
    }

    return false;
}

void Dropbox::put(
        const std::string path,
        const std::vector<char>& data,
        const Headers userHeaders,
        const Query query) const
{
    Headers headers(httpGetHeaders());
    headers["Dropbox-API-Arg"] = json{{ "path", "/" + path }}.dump();
    headers["Content-Type"] = "application/octet-stream";

    headers.insert(userHeaders.begin(), userHeaders.end());

    const Response res(Http::internalPost(putUrl, data, headers, query));

    if (!res.ok()) throw ArbiterError(res.str());
}

std::string Dropbox::continueFileInfo(std::string cursor) const
{
    Headers headers(httpPostHeaders());

    const std::string f(json{{ "cursor", cursor }}.dump());
    std::vector<char> postData(f.begin(), f.end());
    Response res(Http::internalPost(continueListUrl, postData, headers));

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

std::vector<std::string> Dropbox::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;

    path.pop_back();
    const bool recursive(path.back() == '*');
    if (recursive) path.pop_back();
    if (path.back() == '/') path.pop_back();

    auto listPath = [this, recursive](std::string path)->std::string
    {
        Headers headers(httpPostHeaders());

        const json request {
            { "path", "/" + path },
            { "recursive", recursive },
            { "include_media_info", false },
            { "include_deleted", false }
        };
        const std::string f(request.dump());
        std::vector<char> postData(f.begin(), f.end());

        // Can't fully qualify this protected method within the lambda due to a
        // GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61148
        Response res(internalPost(listUrl, postData, headers));

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

    auto processPath = [this, verbose, &results, &more, &cursor](std::string d)
    {
        if (d.empty()) return;
        if (verbose) std::cout << '.';

        const json j(json::parse(d));
        if (!j.count("entries"))
        {
            throw ArbiterError("Returned JSON from Dropbox was null");
        }
        const json& entries(j.at("entries"));
        if (!entries.is_array())
        {
            throw ArbiterError("Returned JSON from Dropbox was not an array");
        }

        more = j.value("has_more", false);
        cursor = j.value("cursor", "");

        for (std::size_t i(0); i < entries.size(); ++i)
        {
            const json& v(entries[i]);
            const std::string tag(v.value(".tag", ""));

            // Only insert files.
            if (std::equal(tag.begin(), tag.end(), fileTag.begin(), ins))
            {
                // Results already begin with a slash.
                results.push_back(
                        profiledProtocol() +
                        ":/" +
                        v.at("path_lower").get<std::string>());
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

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

