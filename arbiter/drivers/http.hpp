#pragma once

#include <vector>
#include <memory>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#include <arbiter/util/http.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace drivers
{

/** @brief HTTP driver.  Intended as both a standalone driver as well as a base
 * for derived drivers build atop HTTP.
 *
 * Derivers should overload the HTTP-specific put/get methods that accept
 * headers and query parameters rather than Driver::put and Driver::get.
 *
 * Internal methods for derivers are provided as protected methods.
 */
class ARBITER_DLL Http : public Driver
{
public:
    Http(
            http::Pool& pool,
            std::string protocol = "http",
            std::string httpProtocol = "http",
            std::string profile = "");
    static std::unique_ptr<Http> create(http::Pool& pool);

    /** By default, performs a HEAD request and returns the contents of the
     * Content-Length header.
     */
    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    virtual void put(
            std::string path,
            const std::vector<char>& data) const final override
    {
        put(path, data, http::Headers(), http::Query());
    }

    /* HTTP-specific driver methods follow.  Since many drivers (S3, Dropbox,
     * etc.) are built atop HTTP, we'll provide HTTP-specific methods for
     * derived classes to use in addition to the generic PUT/GET combinations.
     *
     * Specifically, we'll add POST/HEAD calls, and allow headers and query
     * parameters to be passed as well.
     */

    /** Perform an HTTP GET request. */
    std::string get(
            std::string path,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

    /** Perform an HTTP GET request. */
    std::unique_ptr<std::string> tryGet(
            std::string path,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

    /* Perform an HTTP HEAD request. */
    std::size_t getSize(
            std::string path,
            http::Headers headers,
            http::Query query = http::Query()) const;

    /* Perform an HTTP HEAD request. */
    std::unique_ptr<std::size_t> tryGetSize(
            std::string path,
            http::Headers headers,
            http::Query query = http::Query()) const;

    /** Perform an HTTP GET request. */
    std::vector<char> getBinary(
            std::string path,
            http::Headers headers,
            http::Query query) const;

    /** Perform an HTTP GET request. */
    std::unique_ptr<std::vector<char>> tryGetBinary(
            std::string path,
            http::Headers headers,
            http::Query query) const;

    /** Perform an HTTP PUT request. */
    void put(
            std::string path,
            const std::string& data,
            http::Headers headers,
            http::Query query) const;

    /** HTTP-derived Drivers should override this version of PUT to allow for
     * custom headers and query parameters.
     */

    /** Perform an HTTP PUT request. */
    virtual void put(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers,
            http::Query query) const;

    void post(
            std::string path,
            const std::string& data,
            http::Headers headers,
            http::Query query) const;
    void post(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers,
            http::Query query) const;

    /* These operations are other HTTP-specific calls that derived drivers may
     * need for their underlying API use.
     */
    http::Response internalGet(
            std::string path,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query(),
            std::size_t reserve = 0) const;

    http::Response internalPut(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

    http::Response internalHead(
            std::string path,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

    http::Response internalPost(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

protected:
    /** HTTP-derived Drivers should override this version of GET to allow for
     * custom headers and query parameters.
     */
    virtual bool get(
            std::string path,
            std::vector<char>& data,
            http::Headers headers,
            http::Query query) const;

    http::Pool& m_pool;
    std::string m_httpProtocol;

private:
    virtual bool get(
            std::string path,
            std::vector<char>& data) const final override
    {
        return get(path, data, http::Headers(), http::Query());
    }

    std::string typedPath(const std::string& p) const;
};

/** @brief HTTPS driver.  Identical to the HTTP driver except for its type
 * string.
 */
class Https : public Http
{
public:
    Https(
            http::Pool& pool,
            std::string driverProtocol = "https",
            std::string httpProtocol = "https",
            std::string profile = "")
        : Http(pool, driverProtocol, httpProtocol, profile)
    { }

    static std::unique_ptr<Https> create(http::Pool& pool)
    {
        return std::unique_ptr<Https>(new Https(pool));
    }
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

