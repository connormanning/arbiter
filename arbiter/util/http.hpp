#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/curl.hpp>
#include <arbiter/util/exports.hpp>
#include <arbiter/util/types.hpp>
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
ARBITER_DLL std::string sanitize(const std::string& path, const std::string& exclusions = "/");

/** Build a query string from key-value pairs.  If @p query is empty, the
 * result is an empty string.  Otherwise, the result will start with the
 * '?' character.
 */
ARBITER_DLL std::string buildQueryString(const http::Query& query);

/** @cond arbiter_internal */

class ARBITER_DLL Pool;

class ARBITER_DLL Resource
{
public:
    Resource(Pool& pool, Curl& curl, std::size_t retry);
    ~Resource();

    http::Response get(
            std::string path,
            Headers headers = Headers(),
            Query query = Query(),
            std::size_t reserve = 0,
            int retry = -1,
            std::size_t timeout = 0);

    http::Response head(
            std::string path,
            Headers headers = Headers(),
            Query query = Query());

    http::Response put(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query(),
            int retry = -1,
            std::size_t timeout = 0);

    http::Response post(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query());

private:
    Pool& m_pool;
    Curl& m_curl;
    std::size_t m_retry;

    http::Response exec(std::function<http::Response()> f, int retry = -1);
};

class ARBITER_DLL Pool
{
    // Only HttpResource may release.
    friend class Resource;

public:
    Pool() : Pool(4, 4, "") { }
    Pool(std::size_t concurrent, std::size_t retry, const std::string& config);
    ~Pool();

    Resource acquire();
    void wakeup();
    void perform(Curl& curl);

private:
    void run();
    void release(Curl& curl);
    void handleReady();
    bool handleFailure();
    bool handleCompleted();

    CURL *m_multi;
    std::vector<Curl> m_curls;
    std::thread m_runner;
    std::size_t m_retry;
    std::atomic<bool> m_stop;

    std::mutex m_mutex;
    // Provide notification between a thread waiting on a Curl and one
    // making one available.
    std::condition_variable m_cv;
    // Provide notification between the run thread and those waiting.
    std::condition_variable m_poolCv;
};

/** @endcond */

} // namespace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

