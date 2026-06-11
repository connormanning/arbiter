#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/http.hpp>
#include <arbiter/util/json.hpp>
#endif

#include <curl/curl.h>

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

std::string sanitize(const std::string& path, const std::string& excStr)
{
    auto ispunct = [](char c) -> bool
    {
        return c == '-' || c == '.' || c == '_' || c == '~';
    };

    auto isexclusion = [&excStr](char c) -> bool
    {
        return excStr.find(c) != std::string::npos;
    };

    std::ostringstream result;
    result.fill('0');
    result << std::hex;

    for (char c : path)
    {
        if (std::isalnum(c) || ispunct(c) || isexclusion(c))
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
    if (query.empty())
        return std::string();

    std::string out;
    for (auto &[key, val] : query)
    {
        const char sep(out.empty() ? '?' : '&');
        out += sep + key + '=' + val;
    }
    return out;
}

Resource::Resource(
        Pool& pool,
        Curl& curl,
        const std::size_t retry)
    : m_pool(pool)
    , m_curl(curl)
    , m_retry(retry)
{ }

Resource::~Resource()
{
    m_pool.release(m_curl);
}

Response Resource::get(
        const std::string path,
        const Headers headers,
        const Query query,
        const std::size_t reserve,
        const int retry,
        const std::size_t timeout)
{
    return exec([this, path, headers, query, reserve, timeout]()->Response
    {
        m_curl.prepareGet(path, headers, query, reserve, timeout);
        m_pool.perform(m_curl);
        return m_curl.response();
    }, retry);
}

Response Resource::head(
        const std::string path,
        const Headers headers,
        const Query query)
{
    return exec([this, path, headers, query]()->Response
    {
        m_curl.prepareHead(path, headers, query);
        m_pool.perform(m_curl);
        return m_curl.response();
    });
}

Response Resource::put(
        std::string path,
        const std::vector<char>& data,
        const Headers headers,
        const Query query,
        const int retry,
        const std::size_t timeout)
{
    return exec([this, path, &data, headers, query, timeout]()->Response
    {
        m_curl.preparePut(path, data, headers, query, timeout);
        m_pool.perform(m_curl);
        return m_curl.response();
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
        m_curl.preparePost(path, data, headers, query);
        m_pool.perform(m_curl);
        return m_curl.response();
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
        const std::string& config)
    : m_retry(retry)
{
    curl_global_init(CURL_GLOBAL_ALL);
    for (std::size_t i = 0; i < concurrent; ++i)
        m_curls.emplace_back(i, config);
    m_multi = curl_multi_init();
    m_runner = std::thread(&Pool::run, this);
}

Pool::~Pool()
{
    m_stop = true;
    wakeup();
    m_runner.join();

    std::lock_guard l(m_mutex);
    for (size_t i = 0; i < m_curls.size(); ++i)
        curl_multi_remove_handle(m_multi, m_curls[i].m_curl);

    // This deletes all the curl objects and does curl_easy_cleanup.
    m_curls.clear();
    curl_multi_cleanup(m_multi);
}

// Thread that performs the curl activity. Runs until told to stop.
void Pool::run()
{
    while (true)
    {
        if (m_stop)
            break;

        // Add ready transfers to the multi handle to run. If there is nothing to run, wait
        // for the 1 sec. timeout. If a new handle is added, the poll will break before
        // the 1 sec. timeout (see wakeup())
        if (handleReady() == 0)
        {
            curl_multi_poll(m_multi, NULL, 0, 1000, NULL);
            continue;
        }

        int stillRunning;
        CURLMcode result = curl_multi_perform(m_multi, &stillRunning);
        if (result == CURLM_OK && stillRunning)
            result = curl_multi_poll(m_multi, NULL, 0, 200, NULL);

        bool notify;
        if (result != CURLM_OK)
            notify = handleFailure();
        else
            notify = handleCompleted();

        // If any threads have completed, notify waiters.
        if (notify)
            m_poolCv.notify_all();
    }
}

// For any any transfers that are ready, set the state to running, clear the code and
// add the handle.
int Pool::handleReady()
{
    std::lock_guard l(m_mutex);

    int runningCount = 0;
    for (Curl& curl : m_curls)
    {
        if (curl.m_state == Curl::State::READY)
        {
            curl.m_state = Curl::State::RUNNING;
            curl.m_code = 0;
            curl_multi_add_handle(m_multi, curl.m_curl);
        }
        runningCount += (curl.m_state == Curl::State::RUNNING);
    }
    return runningCount;
}

// See if any curl requests completed. If so, mark the state as DONE.
bool Pool::handleCompleted()
{
    bool notify = false;
    while (true)
    {
        int msgCnt;
        CURLMsg *m = curl_multi_info_read(m_multi, &msgCnt);
        if (!m)
            break;
        if (m->msg != CURLMSG_DONE)
            continue;

        // If we have a completed transfer, set the state to done, update the http code
        // and say we should notify.
        curl_multi_remove_handle(m_multi, m->easy_handle);
        std::lock_guard l(m_mutex);
        for (Curl& curl : m_curls)
            if (curl.m_curl == m->easy_handle)
            {
                curl.m_state = Curl::State::DONE;
                curl_easy_getinfo(curl.m_curl, CURLINFO_RESPONSE_CODE, &curl.m_code);
                notify = true;
            }
    }
    return notify;
}

// Abort all the running transfers as curl has failed internally. Remove the handle.
// Set the state to done. Update the http code and return the notification state.
bool Pool::handleFailure()
{
    bool notify = false;

    std::lock_guard l(m_mutex);
    for (Curl& curl : m_curls)
        if (curl.m_state == Curl::State::RUNNING)
        {
            curl_multi_remove_handle(m_multi, curl.m_curl);
            curl.m_state = Curl::State::DONE;
            curl.m_code = 550;  // Made-up error code.
            notify = true;
        }
    return notify;
}

// Wakeup the run thread.
void Pool::wakeup()
{
    curl_multi_wakeup(m_multi);
}

// Acquire a resource (a curl easy handle) from the pool.
Resource Pool::acquire()
{
    if (m_curls.empty())
        throw std::runtime_error("Cannot acquire from empty pool");

    Curl *foundCurl = nullptr;

    // Wait until we find an unused Curl object. If we find one, mark
    // it acquired.
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this, &foundCurl]()
    {
        for (Curl& curl : m_curls)
        {
            if (curl.m_state == Curl::State::UNUSED)
            {
                curl.m_state = Curl::State::ACQUIRED;
                foundCurl = &curl;
                return true;
            }
        }
        return false;
     });

    // Return the resource with the acquired Curl.
    return Resource(*this, *foundCurl, m_retry);
}

// Release a curl handle.
void Pool::release(Curl& curl)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_curls[curl.id()].m_state = Curl::State::UNUSED;
    }

    m_cv.notify_one();
}

void Pool::perform(Curl& curl)
{
    std::unique_lock l(m_mutex);
    curl.m_state = Curl::State::READY;
    wakeup();
    // Wait until the operation is done or a non-recoverable error occurs.
    m_poolCv.wait(l, [&curl]()
    {
        return curl.m_state == Curl::State::DONE;
    });
}

} // namepace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

