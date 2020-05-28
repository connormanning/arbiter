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
    class Resource;

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
    // static std::string getToken(std::string s);
    std::string getRefreshUrl() { return m_authUrl + "/common/oauth2/v2.0/token"; }
    void refresh();
    http::Headers headers();

private:
    //auth variables necessary for refreshing token
    std::string m_refresh;
    std::string m_redirect;
    std::string m_id;
    std::string m_secret;
    std::string m_token;
    int64_t m_expiration;

    mutable http::Headers m_headers;
    mutable std::mutex m_mutex;
    const std::string m_authUrl = "https://login.microsoftonline.com/";
};

class OneDrive::Resource
{
public:
    Resource(std::string path) {
        m_path = path;
        m_baseUrl = hostUrl + path;
    }
    const std::string endpoint() const { return std::string(m_baseUrl); }
    const std::string binaryEndpoint() const { return std::string(m_baseUrl + ":/content"); }
    const std::string childrenEndpoint() const { return std::string(m_baseUrl + ":/children"); }

private:
    std::string m_baseUrl;
    std::string m_path;
    const std::string hostUrl = "https://graph.microsoft.com/v1.0/me/drive/root:/";

};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif
