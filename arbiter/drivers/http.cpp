#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/http.hpp>
#include <arbiter/util/util.hpp>
#endif

#ifdef ARBITER_WINDOWS
#undef min
#undef max
#endif

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace drivers
{

using namespace http;

Http::Http(
        Pool& pool,
        const std::string driverProtocol,
        const std::string httpProtocol,
        const std::string profile)
    : Driver(driverProtocol, profile)
    , m_pool(pool)
    , m_httpProtocol(httpProtocol)
{
#ifndef ARBITER_CURL
    throw ArbiterError("Cannot create HTTP driver - no curl support was built");
#endif
}

std::unique_ptr<Http> Http::create(Pool& pool)
{
    return std::unique_ptr<Http>(new Http(pool));
}

std::unique_ptr<std::size_t> Http::tryGetSize(std::string path) const
{
    return tryGetSize(path, http::Headers());
}

std::size_t Http::getSize(
        std::string path,
        Headers headers,
        Query query) const
{
    auto s = tryGetSize(path, headers, query);
    if (!s)
    {
        throw ArbiterError("Could not get size from " + path);
    }
    return *s;
}

std::unique_ptr<std::size_t> Http::tryGetSize(
        std::string path,
        Headers headers,
        Query query) const
{
    auto http(m_pool.acquire());
    Response res(http.head(typedPath(path), headers, query));

    if (res.ok())
    {
        const auto cl = findHeader(res.headers(), "Content-Length");
        if (cl) return makeUnique<std::size_t>(std::stoull(*cl));
    }

    return std::unique_ptr<std::size_t>();
}

std::string Http::get(
        std::string path,
        Headers headers,
        Query query) const
{
    const auto data(getBinary(path, headers, query));
    return std::string(data.begin(), data.end());
}

std::unique_ptr<std::string> Http::tryGet(
        std::string path,
        Headers headers,
        Query query) const
{
    std::unique_ptr<std::string> result;
    auto data(tryGetBinary(path, headers, query));
    if (data) result.reset(new std::string(data->begin(), data->end()));
    return result;
}

std::vector<char> Http::getBinary(
        std::string path,
        Headers headers,
        Query query) const
{
    std::vector<char> data;
    if (!get(path, data, headers, query))
    {
        std::stringstream oss;
        oss << "Could not read from '" << path << "'.";

        if (data.size())
            oss << " Response message returned '" << std::string(data.data()) << "'";
        throw ArbiterError(oss.str());
    }
    return data;
}

std::unique_ptr<std::vector<char>> Http::tryGetBinary(
        std::string path,
        Headers headers,
        Query query) const
{
    std::unique_ptr<std::vector<char>> data(new std::vector<char>());
    if (!get(path, *data, headers, query)) data.reset();
    return data;
}

std::vector<char> Http::put(
        std::string path,
        const std::string& data,
        const Headers headers,
        const Query query) const
{
    return put(
        path,
        std::vector<char>(data.begin(), data.end()),
        headers,
        query);
}

bool Http::get(
        std::string path,
        std::vector<char>& data,
        const Headers headers,
        const Query query) const
{
    bool good(false);

    auto http(m_pool.acquire());
    Response res(http.get(typedPath(path), headers, query));


    data = res.data();
    if (res.ok())
        good = true;

    return good;
}

std::vector<char> Http::put(
        const std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query) const
{
    auto http(m_pool.acquire());
    auto res(http.put(typedPath(path), data, headers, query));

    if (!res.ok())
    {
        throw ArbiterError("Couldn't HTTP PUT to " + path);
    }

    return res.data();
}

void Http::post(
        const std::string path,
        const std::string& data,
        const Headers h,
        const Query q) const
{
    return post(path, std::vector<char>(data.begin(), data.end()), h, q);
}

void Http::post(
        const std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query) const
{
    auto http(m_pool.acquire());
    auto res(http.post(typedPath(path), data, headers, query));

    if (!res.ok())
    {
        throw ArbiterError("Couldn't HTTP POST to " + path);
    }
}

Response Http::internalGet(
        const std::string path,
        const Headers headers,
        const Query query,
        const std::size_t reserve,
        const int retry,
        const std::size_t timeout) const
{
    return m_pool.acquire().get(
        typedPath(path),
        headers,
        query,
        reserve,
        retry,
        timeout);
}

Response Http::internalPut(
        const std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query,
        const int retry,
        const std::size_t timeout) const
{
    return m_pool.acquire().put(
        typedPath(path),
        data,
        headers,
        query,
        retry,
        timeout);
}

Response Http::internalHead(
        const std::string path,
        const Headers headers,
        const Query query) const
{
    return m_pool.acquire().head(typedPath(path), headers, query);
}

Response Http::internalPost(
        const std::string path,
        const std::vector<char>& data,
        Headers headers,
        const Query query) const
{
    if (!findHeader(headers, "Content-Length"))
    {
        headers["Content-Length"] = std::to_string(data.size());
    }
    return m_pool.acquire().post(typedPath(path), data, headers, query);
}

std::string Http::typedPath(const std::string& p) const
{
    if (getProtocol(p) != "file") return p;
    else return m_httpProtocol + "://" + p;
}

} // namespace drivers

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

