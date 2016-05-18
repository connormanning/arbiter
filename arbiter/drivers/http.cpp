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

namespace arbiter
{

namespace drivers
{

using namespace http;

Http::Http(Pool& pool) : m_pool(pool) { }

std::unique_ptr<Http> Http::create(Pool& pool, const Json::Value&)
{
    return std::unique_ptr<Http>(new Http(pool));
}

std::unique_ptr<std::size_t> Http::tryGetSize(std::string path) const
{
    std::unique_ptr<std::size_t> size;

    auto http(m_pool.acquire());
    http::Response res(http.head(path));

    if (res.ok() && res.headers().count("Content-Length"))
    {
        const std::string& str(res.headers().at("Content-Length"));
        size.reset(new std::size_t(std::stoul(str)));
    }

    return size;
}

std::string Http::get(
        std::string path,
        http::Headers headers,
        http::Query query) const
{
    const auto data(getBinary(path, headers, query));
    return std::string(data.begin(), data.end());
}

std::unique_ptr<std::string> Http::tryGet(
        std::string path,
        http::Headers headers,
        http::Query query) const
{
    std::unique_ptr<std::string> result;
    auto data(tryGetBinary(path, headers, query));
    if (data) result.reset(new std::string(data->begin(), data->end()));
    return result;
}

std::vector<char> Http::getBinary(
        std::string path,
        http::Headers headers,
        http::Query query) const
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
        http::Headers headers,
        http::Query query) const
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
    http::Response res(http.get(path, headers, query));

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

    if (!http.put(path, data, headers, query).ok())
    {
        throw ArbiterError("Couldn't HTTP PUT to " + path);
    }
}

http::Response Http::internalGet(
        const std::string path,
        const Headers headers,
        const Query query) const
{
    return m_pool.acquire().get(path, headers, query);
}

http::Response Http::internalPut(
        const std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query) const
{
    return m_pool.acquire().put(path, data, headers, query);
}

http::Response Http::internalHead(
        const std::string path,
        const Headers headers,
        const Query query) const
{
    return m_pool.acquire().head(path, headers, query);
}

http::Response Http::internalPost(
        const std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query) const
{
    return m_pool.acquire().post(path, data, headers, query);
}

} // namespace drivers

} // namespace arbiter

