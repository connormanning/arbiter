#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/onedrive.hpp>
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/transforms.hpp>
#include <arbiter/util/json.hpp>
#include <chrono>
#include <thread>
#endif

#include <vector>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace {


const std::string hostUrl = "https://graph.microsoft.com/v1.0/me/drive/root:/";

std::string getBaseEndpoint(const std::string path)
{
    return hostUrl + path;
}

std::string getBinaryEndpoint(const std::string path)
{
    return path + ":/content";
}

std::string getChildrenEndpoint(const std::string path)
{
    return path + ":/children";
}

std::string getRefreshUrl()
{
    return "https://login.microsoftonline.com/common/oauth2/v2.0/token";
}

std::vector<char> buildBody(const http::Query& query)
{
    const std::string body(http::buildQueryString(query));
    return std::vector<char>(body.begin(), body.end());
}

}//unnamed

namespace drivers
{

OneDrive::OneDrive(http::Pool& pool, std::unique_ptr<Auth> auth)
    : Https(pool)
    , m_auth(std::move(auth))
{ }

std::unique_ptr<OneDrive> OneDrive::create(http::Pool& pool, const std::string s)
{
    if (auto auth = Auth::create(s))
    {
        return makeUnique<OneDrive>(pool, std::move(auth));
    }

    return std::unique_ptr<OneDrive>();
}

std::unique_ptr<std::size_t> OneDrive::tryGetSize(const std::string path) const
{
    const std::string endpoint(getBaseEndpoint(path));
    http::Headers headers(m_auth->headers());
    headers["Content-Type"] = "application/x-www-form-urlencoded";
    drivers::Https https(m_pool);

    const auto res(https.internalGet(endpoint, headers));

    if (!res.ok())
    {
        std::cout <<
            "Failed get - " << res.code() << ": " << res.str() << std::endl;
        return std::unique_ptr<std::size_t>();
    }

    //parse the data into a json object and extract size key value
    const auto obj = json::parse(res.data());
    if (obj.count("size"))
    {
        return makeUnique<std::size_t>(obj.at("size").get<std::size_t>());
    }

    return std::unique_ptr<std::size_t>();
}

std::vector<std::string> OneDrive::processList(std::string path, bool recursive) const
{
    const std::string endpoint(getBaseEndpoint(path));
    std::vector<std::string> result;

    std::string pageUrl(getChildrenEndpoint(endpoint));

    http::Headers headers(m_auth->headers());
    headers["Content-Type"] = "application/json";
    drivers::Https https(m_pool);

    //iterate through files and folders located in path parent
    //limit to 200 items per list, @odata.nextLink will be provided as url for
    //next set of items
    do
    {
        const http::Query queries(http::getQueries(pageUrl));
        auto res(https.internalGet(getChildrenEndpoint(endpoint), headers, queries));

        const json obj(json::parse(res.data()));

        if (!obj.contains("value") || !res.ok())
        {
            std::cout << "Could not get OneDrive item  " << path
                << " with response code " << res.code() << ":" << res.str() << std::endl;
            return std::vector<std::string>();
        }

        //parse list
        const json list(obj.at("value"));
        for (auto& i: list.items())
        {
            const auto data(i.value());
            const std::string fileName(data.at("name").get<std::string>());
            const std::string filePath(path + "/" + fileName);

            result.push_back(filePath);
            if (data.contains("folder") && recursive)
            {
                //restart process with new file head
                const auto children(processList(filePath, recursive));

                //add result of children processes to the parent
                result.insert(result.end(), children.begin(), children.end());
            }
        }

        //check for link to next set
        if (obj.contains("@odata.nextLink"))
            pageUrl = obj.at("@odata.nextLink");
        else
            break;

    } while (true);

    return result;
}

std::vector<std::string> OneDrive::glob(std::string path, bool verbose) const
{
    path.pop_back();
    bool recursive(path.back() == '*');
    if (recursive)
        path.pop_back();

    if (path.back() == '/')
        path.pop_back();

    return processList(path, recursive);
}

bool OneDrive::get(
        const std::string path,
        std::vector<char>& data,
        const http::Headers userHeaders,
        const http::Query query) const
{
    const std::string endpoint(getBaseEndpoint(path));
    http::Headers headers(m_auth->headers());
    headers["Content-Type"] = "application/octet-stream";
    headers.insert(userHeaders.begin(), userHeaders.end());

    drivers::Https https(m_pool);
    const auto res(https.internalGet(getBinaryEndpoint(endpoint), headers));

    if (!res.ok()) {
        std::cout <<
            "Failed get - " << res.code() << ": " << res.str() << std::endl;
        return false;
    }

    data = res.data();
    return true;
}

OneDrive::Auth::Auth(std::string s) {
    const json config = json::parse(s);
    m_token = config.at("access_token").get<std::string>();
    m_refresh = config.at("refresh_token").get<std::string>();
    m_redirect = config.at("redirect_uri").get<std::string>();
    m_id = config.at("client_id").get<std::string>();
    m_secret = config.at("client_secret").get<std::string>();
}

std::unique_ptr<OneDrive::Auth> OneDrive::Auth::create(const std::string s)
{
    return makeUnique<Auth>(s);
}

void OneDrive::Auth::refresh()
{
    //only refresh if we get within 2 minutes of the token ending
    const auto now(Time().asUnix());
    if (m_expiration - now > 120)
        return;

    http::Pool pool;
    drivers::Https https(pool);
    const http::Headers headers({
        { "Accept", "application/json" },
        { "Content-Type", "application/x-www-form-urlencoded" }
    });

    const auto encoded = buildBody({
        { "access_token", m_token },
        { "refresh_token", m_refresh },
        { "client_id", m_id },
        { "client_secret", m_secret },
        { "scope", "offline_access+files.read.all+user.read.all" },
        { "grant_type", "refresh_token" }
    });

    const auto res(https.internalPost(getRefreshUrl(), encoded, headers));
    const auto response(json::parse(res.str()));
    if (res.code() != 200)
    {
        std::cout << res.code() << ": Failed to refresh token" << res.str() << std::endl;
        throw new ArbiterError("Failed to refresh token. " + res.str());
    }

    //reset the token, refresh, and expiration time
    m_token = response.at("access_token").get<std::string>();
    m_refresh = response.at("refresh_token").get<std::string>();
    m_expiration = now + response.at("expires_in").get<int64_t>();
}

http::Headers OneDrive::Auth::headers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    refresh();
    m_headers["Accept"] = "application/json";
    m_headers["Authorization"] = "Bearer " + getToken();
    return m_headers;
}

}//drivers

}//arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
};
#endif