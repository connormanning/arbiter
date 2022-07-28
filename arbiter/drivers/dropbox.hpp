#pragma once

#include <memory>
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

/** @brief %Dropbox driver. */
class Dropbox : public Http
{
public:
    class Auth;
    Dropbox(http::Pool& pool, const Auth& auth, std::string profile);

    /** Try to construct a %Dropbox Driver.  @p j may be stringified JSON, in
     * which the key `token` will be used to construct the Dropbox auth, or
     * may simply be the token string itself.
     */
    static std::unique_ptr<Dropbox> create(
        http::Pool& pool,
        std::string j,
        std::string profile);

    virtual void put(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers,
            http::Query query = http::Query()) const override;

    /** @brief %Dropbox authentication information. */
    class Auth
    {
    public:
        explicit Auth(std::string token) : m_token(token) { }
        const std::string& token() const { return m_token; }

    private:
        std::string m_token;
    };

private:
    virtual bool get(
            std::string path,
            std::vector<char>& data,
            http::Headers headers,
            http::Query query) const override;

    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    std::string continueFileInfo(std::string cursor) const;

    http::Headers httpGetHeaders() const;
    http::Headers httpPostHeaders() const;

    Auth m_auth;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

