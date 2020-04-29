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

    //return type of driver
    virtual std::string type() const override { return "od"; };
    //return size of the file
    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;
private:
    virtual bool get(
        std::string path,
        std::vector<char>& data,
        http::Headers headers,
        http::Query query) const override;


    std::unique_ptr<Auth> m_auth;

};

class OneDrive::Auth
{
public:
    Auth(std::string s);
    static std::unique_ptr<Auth> create(std::string s);
    http::Headers headers() const;

private:
    const std::string m_token;
    mutable http::Headers m_headers;
    mutable std::mutex m_mutex;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif
