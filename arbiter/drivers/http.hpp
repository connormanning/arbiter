#pragma once

#include <vector>
#include <string>
#include <thread>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <map>

#include <curl/curl.h>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#endif

namespace arbiter
{

/** @cond arbiter_internal */
class HttpResponse
{
public:
    HttpResponse(int code = 0)
        : m_code(code)
        , m_data()
    { }

    HttpResponse(int code, std::vector<char> data)
        : m_code(code)
        , m_data(data)
    { }

    ~HttpResponse() { }

    bool ok() const     { return m_code / 100 == 2; }
    bool retry() const  { return m_code / 100 == 5; }   // Only server errors.
    int code() const    { return m_code; }

    std::vector<char> data() const { return m_data; }

private:
    int m_code;
    std::vector<char> m_data;
};
/** @endcond */

typedef std::vector<std::string> Headers;

///////////////////////////////////////////////////////////////////////////////

class HttpPool;

namespace drivers
{

/** @brief HTTP driver. */
class Http : public Driver
{
public:
    Http(HttpPool& pool);

    virtual std::string type() const override { return "http"; }
    virtual void put(
            std::string path,
            const std::vector<char>& data) const override;

    static std::string sanitize(std::string path);

private:
    virtual bool get(
            std::string path,
            std::vector<char>& data) const override;

    HttpPool& m_pool;
};

} // namespace drivers

/** @cond arbiter_internal */
class Curl
{
    // Only HttpPool may create.
    friend class HttpPool;

public:
    ~Curl();

    HttpResponse get(std::string path, Headers headers);
    HttpResponse put(
            std::string path,
            const std::vector<char>& data,
            Headers headers);

private:
    Curl();

    void init(std::string path, const Headers& headers);

    Curl(const Curl&);
    Curl& operator=(const Curl&);

    CURL* m_curl;
    curl_slist* m_headers;

    std::vector<char> m_data;
};

class HttpResource
{
public:
    HttpResource(HttpPool& pool, Curl& curl, std::size_t id, std::size_t retry);
    ~HttpResource();

    HttpResponse get(
            std::string path,
            Headers headers = Headers());

    HttpResponse put(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers());

private:
    HttpPool& m_pool;
    Curl& m_curl;
    std::size_t m_id;
    std::size_t m_retry;

    HttpResponse exec(std::function<HttpResponse()> f);
};

class HttpPool
{
    // Only HttpResource may release.
    friend class HttpResource;

public:
    HttpPool(std::size_t concurrent, std::size_t retry);

    HttpResource acquire();

private:
    void release(std::size_t id);

    std::vector<std::unique_ptr<Curl>> m_curls;
    std::vector<std::size_t> m_available;
    std::size_t m_retry;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};
/** @endcond */

} // namespace arbiter

