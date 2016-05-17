#pragma once

#include <memory>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/http.hpp>
#endif

namespace arbiter
{

namespace drivers
{

/** @brief %Dropbox authentication information. */
class DropboxAuth
{
public:
    explicit DropboxAuth(std::string token) : m_token(token) { }
    std::string token() const { return m_token; }

private:
    std::string m_token;
};

/** @brief %Dropbox driver. */
class Dropbox : public Http
{
public:
    Dropbox(HttpPool& pool, DropboxAuth auth);

    /** Try to construct a %Dropbox Driver.  Searches @p json for the key
     * `token` to construct a DropboxAuth.
     */
    static std::unique_ptr<Dropbox> create(
            HttpPool& pool,
            const Json::Value& json);

    virtual std::string type() const override { return "dropbox"; }
    virtual void put(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query = Query()) const override;

private:
    virtual bool get(
            std::string path,
            std::vector<char>& data,
            Headers headers,
            Query query = Query()) const override;

    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    std::string continueFileInfo(std::string cursor) const;

    Headers httpGetHeaders() const;
    Headers httpPostHeaders() const;

    DropboxAuth m_auth;
};

} // namespace drivers
} // namespace arbiter

