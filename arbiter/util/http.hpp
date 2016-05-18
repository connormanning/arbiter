#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <curl/curl.h>

#ifndef ARBITER_IS_AMALGAMATION
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

/** HTTP header fields. */
using Headers = std::map<std::string, std::string>;

/** HTTP query parameters. */
using Query = std::map<std::string, std::string>;

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

class Response
{
public:
    Response(int code = 0)
        : m_code(code)
        , m_data()
    { }

    Response(int code, std::vector<char> data)
        : m_code(code)
        , m_data(data)
    { }

    Response(
            int code,
            const std::vector<char>& data,
            const Headers& headers)
        : m_code(code)
        , m_data(data)
        , m_headers(headers)
    { }

    ~Response() { }

    bool ok() const             { return m_code / 100 == 2; }
    bool clientError() const    { return m_code / 100 == 4; }
    bool serverError() const    { return m_code / 100 == 5; }
    int code() const            { return m_code; }

    std::vector<char> data() const { return m_data; }
    const Headers& headers() const { return m_headers; }

private:
    int m_code;
    std::vector<char> m_data;
    Headers m_headers;
};

class Pool;

class Curl
{
    friend class Pool;

public:
    ~Curl();

    http::Response get(std::string path, Headers headers, Query query);
    http::Response head(std::string path, Headers headers, Query query);
    http::Response put(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query);
    http::Response post(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query);

private:
    Curl(bool verbose, std::size_t timeout);

    void init(std::string path, const Headers& headers, const Query& query);

    Curl(const Curl&);
    Curl& operator=(const Curl&);

    CURL* m_curl;
    curl_slist* m_headers;
    const bool m_verbose;
    const std::size_t m_timeout;

    std::vector<char> m_data;
};

class Resource
{
public:
    Resource(Pool& pool, Curl& curl, std::size_t id, std::size_t retry);
    ~Resource();

    http::Response get(
            std::string path,
            Headers headers = Headers(),
            Query query = Query());

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

