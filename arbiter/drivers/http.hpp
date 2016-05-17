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

    HttpResponse(
            int code,
            const std::vector<char>& data,
            const Headers& headers)
        : m_code(code)
        , m_data(data)
        , m_headers(headers)
    { }

    ~HttpResponse() { }

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
/** @endcond */

///////////////////////////////////////////////////////////////////////////////

class HttpPool;

namespace drivers
{

/** @brief HTTP driver.  Intended as both a standalone driver as well as a base
 * for derived drivers build atop HTTP.  Derivers should overload the
 * HTTP-specific put/get methods that accept headers and query parameters
 * rather than Driver::put and Driver::get, which are overridden as `final`
 * here as they will be routed to the more specific methods.
 */
class Http : public Driver
{
public:
    Http(HttpPool& pool);
    static std::unique_ptr<Http> create(
            HttpPool& pool,
            const Json::Value& json);

    // Inherited from Driver.
    virtual std::string type() const override { return "http"; }

    /** By default, performs a HEAD request and returns the contents of the
     * Content-Length header.
     */
    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    virtual void put(
            std::string path,
            const std::vector<char>& data) const final override
    {
        put(path, data, Headers(), Query());
    }




    /* HTTP-specific driver methods follow.  Since many drivers (S3, Dropbox,
     * etc.) are built atop HTTP, we'll provide HTTP-specific methods for
     * derived classes to use in addition to the generic PUT/GET combinations.
     *
     * Specifically, we'll add POST/HEAD calls, and allow headers and query
     * parameters to be passed as well.
     */
    std::string get(
            std::string path,
            Headers headers,
            Query query = Query()) const;

    std::vector<char> getBinary(
            std::string path,
            Headers headers,
            Query query = Query()) const;

    // Utility functions.

    /** Perform URI percent-encoding, without encoding characters included in
     * @p exclusions.
     */
    static std::string sanitize(std::string path, std::string exclusions = "/");

    /** Build a query string from key-value pairs.  If @p query is empty, the
     * result is an empty string.  Otherwise, the result will start with the
     * '?' character.
     */
    static std::string buildQueryString(const Query& query);

    /** HTTP-derived Drivers should override this version of PUT to allow for
     * custom headers and query parameters.
     */
    virtual void put(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query = Query()) const;

protected:
    /** HTTP-derived Drivers should override this version of GET to allow for
     * custom headers and query parameters.
     */
    virtual bool get(
            std::string path,
            std::vector<char>& data,
            Headers headers,
            Query query) const;

    /* These operations are other HTTP-specific calls that derived drivers may
     * need for their underlying API use.
     */
    HttpResponse internalGet(
            std::string path,
            Headers headers = Headers(),
            Query query = Query()) const;

    HttpResponse internalPut(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query()) const;

    HttpResponse internalHead(
            std::string path,
            Headers headers = Headers(),
            Query query = Query()) const;

    HttpResponse internalPost(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query()) const;

private:
    virtual bool get(
            std::string path,
            std::vector<char>& data) const final override
    {
        return get(path, data, Headers(), Query());
    }

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

    HttpResponse get(std::string path, Headers headers, Query query);
    HttpResponse head(std::string path, Headers headers, Query query);
    HttpResponse put(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query);
    HttpResponse post(
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

class HttpResource
{
public:
    HttpResource(HttpPool& pool, Curl& curl, std::size_t id, std::size_t retry);
    ~HttpResource();

    HttpResponse get(
            std::string path,
            Headers headers = Headers(),
            Query query = Query());

    HttpResponse head(
            std::string path,
            Headers headers = Headers(),
            Query query = Query());

    HttpResponse put(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query());

    HttpResponse post(
            std::string path,
            const std::vector<char>& data,
            Headers headers = Headers(),
            Query query = Query());

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
    HttpPool(
            std::size_t concurrent,
            std::size_t retry,
            const Json::Value& json);

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

