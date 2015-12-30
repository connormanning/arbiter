#pragma once

#include <memory>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#include <arbiter/drivers/http.hpp>
#endif

namespace arbiter
{

namespace drivers
{

/** @brief Dropbox authentication information. */
class DropboxAuth
{
public:
    DropboxAuth(std::string token);

    static std::unique_ptr<DropboxAuth> find(std::string user);

    std::string token() const;

private:
    std::string m_token;
};

typedef std::map<std::string, std::string> Query;

/** @brief Dropbox driver. */
class Dropbox : public Driver
{
public:
    Dropbox(HttpPool& pool, DropboxAuth auth);

    virtual std::string type() const override { return "dropbox"; }
    virtual void put(
            std::string path,
            const std::vector<char>& data) const override;


private:
    virtual bool get(std::string path, std::vector<char>& data) const override;
    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    Headers httpGetHeaders(std::string filePath) const;
    HttpPool& m_pool;
    DropboxAuth m_auth;
};

} // namespace drivers
} // namespace arbiter

