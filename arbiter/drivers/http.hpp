#pragma once

#include <vector>
#include <string>
#include <thread>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <map>

#include <curl/curl.h>

#include <arbiter/driver.hpp>

namespace arbiter
{

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

typedef std::vector<std::string> Headers;

///////////////////////////////////////////////////////////////////////////////

class HttpPool;

class HttpDriver : public Driver
{
public:
    HttpDriver(HttpPool& pool, std::size_t retry = 0);

    virtual std::string type() const { return "http"; }
    virtual std::vector<char> get(std::string path);
    virtual void put(std::string path, const std::vector<char>& data);

private:
    HttpPool& m_pool;
    std::size_t m_retry;
};

class Curl
{
    // Only HttpPool may create.
    friend class HttpPool;

public:
    ~Curl();

    HttpResponse get(std::string path, std::vector<std::string> headers);
    HttpResponse put(
            std::string path,
            const std::vector<char>& data,
            std::vector<std::string> headers);

private:
    Curl();

    void init(std::string path, const std::vector<std::string>& headers);

    Curl(const Curl&);
    Curl& operator=(const Curl&);

    CURL* m_curl;
    curl_slist* m_headers;

    std::vector<char> m_data;
};

class HttpResource
{
public:
    HttpResource(HttpPool& pool, Curl& curl, std::size_t id);
    ~HttpResource();

    HttpResponse get(
            std::string path,
            std::size_t retry,
            Headers headers = Headers());

    HttpResponse put(
            std::string path,
            const std::vector<char>& data,
            std::size_t retry,
            Headers headers = Headers());

private:
    HttpPool& m_pool;
    Curl& m_curl;
    std::size_t m_id;

    HttpResponse exec(std::function<HttpResponse()> f, std::size_t retry);
};

class HttpPool
{
    // Only HttpResource may release.
    friend class HttpResource;

public:
    HttpPool(std::size_t concurrent);

    HttpResource acquire();

private:
    void release(std::size_t id);

    std::vector<std::unique_ptr<Curl>> m_curls;
    std::vector<std::size_t> m_available;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};

} // namespace arbiter

