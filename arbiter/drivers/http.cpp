#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/http.hpp>
#endif

#ifdef ARBITER_WINDOWS
#undef min
#undef max
#endif

#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace drivers
{

using namespace http;

Http::Http(Pool& pool)
    : m_pool(pool)
{
#ifndef ARBITER_CURL
    throw ArbiterError("Cannot create HTTP driver - no curl support was built");
#endif
}

std::unique_ptr<Http> Http::create(Pool& pool, const Json::Value&)
{
    return std::unique_ptr<Http>(new Http(pool));
}

std::unique_ptr<std::size_t> Http::tryGetSize(std::string path) const
{
    std::unique_ptr<std::size_t> size;

    auto http(m_pool.acquire());
    Response res(http.head(typedPath(path)));

    if (res.ok() && res.headers().count("Content-Length"))
    {
        const std::string& str(res.headers().at("Content-Length"));
        size.reset(new std::size_t(std::stoul(str)));
    }

    return size;
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
        throw ArbiterError("Could not read from " + path);
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

void Http::put(
        std::string path,
        const std::string& data,
        const Headers headers,
        const Query query) const
{
    put(path, std::vector<char>(data.begin(), data.end()), headers, query);
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

    if (res.ok())
    {
        data = res.data();
        good = true;
    }

    return good;
}

void Http::put(
        const std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query) const
{
    auto http(m_pool.acquire());

    if (!http.put(typedPath(path), data, headers, query).ok())
    {
        throw ArbiterError("Couldn't HTTP PUT to " + path);
    }
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
        std::cout << res.str() << std::endl;
        throw ArbiterError("Couldn't HTTP POST to " + path);
    }
}

Response Http::internalGet(
        const std::string path,
        const Headers headers,
        const Query query,
        const std::size_t reserve) const
{
    return m_pool.acquire().get(typedPath(path), headers, query, reserve);
}

Response Http::internalPut(
        const std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query) const
{
    return m_pool.acquire().put(typedPath(path), data, headers, query);
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
    if (!headers.count("Content-Length"))
    {
        headers["Content-Length"] = std::to_string(data.size());
    }
    return m_pool.acquire().post(typedPath(path), data, headers, query);
}

} // namespace drivers

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

