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

//     std::string get(std::string path, Headers headers) const;
//     std::vector<char> getBinary(std::string path, Headers headers) const;

private:
    virtual bool get(std::string path, std::vector<char>& data) const override;
    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

//     std::vector<char> get(std::string path, const Query& query) const;
//     bool get(
//             std::string rawPath,
//             const Query& query,
//             std::vector<char>& data,
//             Headers = Headers()) const;
//
    Headers httpGetHeaders(std::string filePath) const;
//     Headers httpPutHeaders(std::string filePath) const;
//
//     std::string getHttpDate() const;
//
//     std::string getSignedEncodedString(
//             std::string command,
//             std::string file,
//             std::string httpDate,
//             std::string contentType = "") const;
//
//     std::string getStringToSign(
//             std::string command,
//             std::string file,
//             std::string httpDate,
//             std::string contentType) const;
//
//     std::vector<char> signString(std::string input) const;
//     std::string encodeBase64(std::vector<char> input) const;
//
    HttpPool& m_pool;
    DropboxAuth m_auth;
};

} // namespace drivers
} // namespace arbiter

