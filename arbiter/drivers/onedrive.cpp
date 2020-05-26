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

#ifdef ARBITER_OPENSSL
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

// See: https://www.openssl.org/docs/manmaster/man3/OPENSSL_VERSION_NUMBER.html
#   if OPENSSL_VERSION_NUMBER >= 0x010100000
#   define ARBITER_OPENSSL_ATLEAST_1_1
#   endif

#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace {

static std::string getQueries(const std::string url)
{
    json result;

    //find position of queries in url
    std::string::size_type pos(url.find("?"));
    if (pos == std::string::npos)
    {
        return result.dump();
    }

    std::string queries(url.substr(pos + 1));

    do
    {
        //get positional values
        //find next query
        std::string::size_type nextQueryPos(queries.find_first_of("&"));
        //if it doesn't exist, go to the end of the string
        if (nextQueryPos == std::string::npos)
            nextQueryPos = queries.size();
        const std::string::size_type separator(queries.find("="));

        //create key-value pair for json
        const std::string key(queries.substr(0, separator));
        const std::string value(queries.substr(separator + 1, nextQueryPos - (separator + 1)));

        result.push_back(json::object_t::value_type(key, value));

        /*BREAK HERE*/
        //was this the last one? if yes, break. if no, resize queries and continue
        if (nextQueryPos == queries.size())
            break;

        queries = queries.substr(nextQueryPos + 1);

    } while (true);

    return result.dump();
}

}

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
    Resource resource(path);
    http::Headers headers(m_auth->headers());
    headers["Authorization"] = m_auth->getToken();
    headers["Content-Type"] = "application/x-www-form-urlencoded";
    headers["Accept"] = "application/json";
    drivers::Https https(m_pool);

    const auto res(https.internalGet(resource.endpoint(), headers));

    if (!res.ok())
    {
        std::cout <<
            "Failed get - " << res.code() << ": " << res.str() << std::endl;
        return std::unique_ptr<std::size_t>();
    }

    //parse the data into a json object and extract size key value
    const auto obj = json::parse(res.data());
    if (obj.find("size") != obj.end())
    {
        return makeUnique<std::size_t>(obj.at("size").get<std::size_t>());
    }

    return std::unique_ptr<std::size_t>();
}

std::vector<std::string> OneDrive::processList(std::string path, bool recursive) const
{
    std::vector<std::string> result;

    Resource resource(path);
    std::string pageUrl(resource.childrenEndpoint());

    http::Headers headers(m_auth->headers());
    headers["Content-Type"] = "application/json";
    drivers::Https https(m_pool);

    //iterate through files and folders located in path parent
    //limit to 200 items per list, @odata.nextLink will be provided as url for
    //next set of items
    do
    {
        http::Query queries;
        auto res(https.internalGet(resource.childrenEndpoint(), headers, queries));
        const json obj(json::parse(res.data()));
        const json parsedQueries(json::parse(getQueries(pageUrl)));
        for (auto& it: parsedQueries.items())
            queries[it.key()] = it.value();

        if (!obj.contains("value") || !res.ok())
        {
            std::cout << "Could not get OneDrive item  " << resource.childrenEndpoint()
                << " with response code " << res.code() << std::endl;
            return std::vector<std::string>();
        }

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
                //TODO: look into more efficient way to do this
                result.insert(result.end(), children.begin(), children.end());
            }
        }

        if (obj.contains("@odata.nextLink"))
            pageUrl = obj.at("@odata.nextLink");
        else
            pageUrl = "";

    } while (pageUrl.size() > 0);

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
    Resource resource(path);
    http::Headers headers(m_auth->headers());
    headers["Content-Type"] = "application/octet-stream";
    headers.insert(userHeaders.begin(), userHeaders.end());

    drivers::Https https(m_pool);
    const auto res(https.internalGet(resource.binaryEndpoint(), headers));

    if (!res.ok()) {
        std::cout <<
            "Failed get - " << res.code() << ": " << res.str() << std::endl;
        return false;
    }

    data = res.data();
    return true;
}

OneDrive::Auth::Auth(std::string s)
    : m_token(json::parse(s).at("access_token").get<std::string>())
    , m_refresh(json::parse(s).at("refresh_token").get<std::string>())
    , m_redirect(json::parse(s).at("redirect_uri").get<std::string>())
    , m_id(json::parse(s).at("client_id").get<std::string>())
    , m_secret(json::parse(s).at("client_secret").get<std::string>())
    , m_tenant(json::parse(s).at("tenant_id").get<std::string>())
    , m_expiration(Time().asUnix())
    { }

std::unique_ptr<OneDrive::Auth> OneDrive::Auth::create(const std::string s)
{
    return makeUnique<Auth>(s);
}

void OneDrive::Auth::refresh()
{
    const auto now(Time().asUnix());
    if (m_expiration - now > 120)
        return;

    http::Pool pool;
    drivers::Https https(pool);
    http::Headers headers({ });

    headers["Content-Type"] = "application/x-www-form-urlencoded";
    headers["Accept"] = "application/json";

    std::string scope = "offline_access+files.readwrite.all+user.read+user.readwrite+user.readbasic.all+user.read.all+directory.read.all+directory.accessasuser.all";
    json bodyJson = {
        { "access_token", m_token },
        { "refresh_token", m_refresh },
        { "client_id", m_id },
        { "client_secret", m_secret },
        { "scope", scope },
        { "grant_type", "refresh_token" }
    };
    std::string body = "";

    for (auto& i: bodyJson.items())
        body += i.key() + "=" + i.value().get<std::string>() + "&";
    body.pop_back();
    std::vector<char> content(body.begin(), body.end());

    const auto res(https.internalPost(this->getRefreshUrl(), content, headers));
    const auto response(json::parse(res.str()));
    std::cout << "response, " << res.str() << std::endl;

    m_token = response.at("access_token").get<std::string>();
    m_refresh = response.at("refresh_token").get<std::string>();

    m_expiration = now + 3599;

}

http::Headers OneDrive::Auth::headers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    refresh();
    m_headers["Authorization"] = "Bearer " + getToken();
    return m_headers;
}

}//drivers

}//arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
};
#endif