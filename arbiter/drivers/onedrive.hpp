#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/http.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace drivers
{

class OneDrive : public Https
{
    class Auth;

public:
    OneDrive(http::Pool& pool, std::unique_ptr<Auth> auth);

    static std::unique_ptr<OneDrive> create(http::Pool& pool, std::string j);

    virtual std::string type() const override { return "od"; };
    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;
private:
    virtual bool get(
        std::string path,
        std::vector<char>& data,
        http::Headers headers,
        http::Query query) const override;

    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    std::vector<std::string> processList(std::string path, bool recursive) const;

    std::unique_ptr<Auth> m_auth;

};

class OneDrive::Auth
{
public:
    Auth(std::string s);
    static std::unique_ptr<Auth> create(std::string s);
    std::string getToken() { return m_token; };
    void refresh();
    http::Headers headers();

private:
    //auth variables necessary for refreshing token
    std::string m_refresh;
    std::string m_redirect;
    std::string m_id;
    std::string m_secret;
    std::string m_token;
    int64_t m_expiration = 0;

    mutable http::Headers m_headers;
    mutable std::mutex m_mutex;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif
