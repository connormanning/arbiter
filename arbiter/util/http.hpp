#pragma once

#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION

#include <arbiter/util/curl.hpp>
#include <arbiter/util/types.hpp>

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
namespace http
{

/** Perform URI percent-encoding, without encoding characters included in
 * @p exclusions.
 */
std::string sanitize(std::string path, std::string exclusions = "/");

/** Build a query string from key-value pairs.  If @p query is empty, the
 * result is an empty string.  Otherwise, the result will start with the
 * '?' character.
 */
std::string buildQueryString(const http::Query& query);

/** @cond arbiter_internal */

class Pool;

class Resource
{
public:
    Resource(Pool& pool, Curl& curl, std::size_t id, std::size_t retry);
    ~Resource();

    http::Response get(
            std::string path,
            Headers headers = Headers(),
            Query query = Query(),
            std::size_t reserve = 0);

    http::Response head(
            std::string path,
            Headers headers = Headers(),
            Query query = Query());

    http::Response put(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query());

    http::Response post(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query());

private:
    Pool& m_pool;
    Curl& m_curl;
    std::size_t m_id;
    std::size_t m_retry;

    http::Response exec(std::function<http::Response()> f);
};

class Pool
{
    // Only HttpResource may release.
    friend class Resource;

public:
    Pool(
            std::size_t concurrent,
            std::size_t retry,
            const Json::Value& json);

    Resource acquire();

private:
    void release(std::size_t id);

    std::vector<std::unique_ptr<Curl>> m_curls;
    std::vector<std::size_t> m_available;
    std::size_t m_retry;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};

/** @endcond */

} // namespace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

