#pragma once

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/http.hpp>
#endif

#include <mutex>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace drivers
{

class Google : public Https
{
    class Auth;
public:
    Google(http::Pool& pool, std::unique_ptr<Auth> auth);

    static std::unique_ptr<Google> create(http::Pool& pool, std::string j);

    // Overrides.
    virtual std::string type() const override { return "gs"; }  // Match gsutil.

    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    /** Inherited from Drivers::Http. */
    virtual void put(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers,
            http::Query query) const override;

private:
    /** Inherited from Drivers::Http. */
    virtual bool get(
            std::string path,
            std::vector<char>& data,
            http::Headers headers,
            http::Query query) const override;

    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    std::unique_ptr<Auth> m_auth;
};

class Google::Auth
{
public:
    Auth(std::string s);
    static std::unique_ptr<Auth> create(std::string s);

    http::Headers headers() const;

private:
    void maybeRefresh() const;
    std::string sign(std::string data, std::string privateKey) const;

    const std::string m_clientEmail;
    const std::string m_privateKey;
    mutable int64_t m_expiration = 0;   // Unix time.
    mutable http::Headers m_headers;

    mutable std::mutex m_mutex;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

