#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/http.hpp>
#include <arbiter/util/json.hpp>
#endif

#ifdef ARBITER_CURL
#include <curl/curl.h>
#endif

#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace http
{

std::string sanitize(const std::string path, const std::string excStr)
{
    static const std::set<char> unreserved = { '-', '.', '_', '~' };
    const std::set<char> exclusions(excStr.begin(), excStr.end());
    std::ostringstream result;
    result.fill('0');
    result << std::hex;

    for (const auto c : path)
    {
        if (std::isalnum(c) || unreserved.count(c) || exclusions.count(c))
        {
            result << c;
        }
        else
        {
            result << std::uppercase;
            result << '%' << std::setw(2) <<
                static_cast<int>(static_cast<uint8_t>(c));
            result << std::nouppercase;
        }
    }

    return result.str();
}

std::string buildQueryString(const Query& query)
{
    return std::accumulate(
            query.begin(),
            query.end(),
            std::string(),
            [](const std::string& out, const Query::value_type& keyVal)
            {
                const char sep(out.empty() ? '?' : '&');
                return out + sep + keyVal.first + '=' + keyVal.second;
            });
}

Resource::Resource(
        Pool& pool,
        Curl& curl,
        const std::size_t id,
        const std::size_t retry)
    : m_pool(pool)
    , m_curl(curl)
    , m_id(id)
    , m_retry(retry)
{ }

Resource::~Resource()
{
    m_pool.release(m_id);
}

Response Resource::get(
        const std::string path,
        const Headers headers,
        const Query query,
        const std::size_t reserve,
        const int retry)
{
    return exec([this, path, headers, query, reserve]()->Response
    {
        return m_curl.get(path, headers, query, reserve);
    }, retry);
}

Response Resource::head(
        const std::string path,
        const Headers headers,
        const Query query)
{
    return exec([this, path, headers, query]()->Response
    {
        return m_curl.head(path, headers, query);
    });
}

Response Resource::put(
        std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query,
        const int retry)
{
    return exec([this, path, &data, headers, query]()->Response
    {
        return m_curl.put(path, data, headers, query);
    }, retry);
}

Response Resource::post(
        std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query)
{
    return exec([this, path, &data, headers, query]()->Response
    {
        return m_curl.post(path, data, headers, query);
    });
}

Response Resource::exec(std::function<Response()> f, const int userRetry)
{
    Response res;
    std::size_t tries(0);

    const std::size_t retry = userRetry == -1
        ? m_retry
        : static_cast<std::size_t>(userRetry);

    do
    {
        if (tries)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds((int)std::pow(2, tries) * 500));
        }

        res = f();
    }
    while (res.serverError() && tries++ < retry);

    return res;
}

///////////////////////////////////////////////////////////////////////////////

Pool::Pool(
        const std::size_t concurrent,
        const std::size_t retry,
        const std::string s)
    : m_curls(concurrent)
    , m_available(concurrent)
    , m_retry(retry)
    , m_mutex()
    , m_cv()
{
#ifdef ARBITER_CURL
    curl_global_init(CURL_GLOBAL_ALL);

    const json config(s.size() ? json::parse(s) : json::object());

    for (std::size_t i(0); i < concurrent; ++i)
    {
        m_available[i] = i;
        m_curls[i].reset(new Curl(config.dump()));
    }
#endif
}

Pool::~Pool() { }

Resource Pool::acquire()
{
    if (m_curls.empty())
    {
        throw std::runtime_error("Cannot acquire from empty pool");
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return !m_available.empty(); });

    const std::size_t id(m_available.back());
    Curl& curl(*m_curls[id]);

    m_available.pop_back();

    return Resource(*this, curl, id, m_retry);
}

void Pool::release(const std::size_t id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_available.push_back(id);
    lock.unlock();

    m_cv.notify_one();
}

} // namepace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

