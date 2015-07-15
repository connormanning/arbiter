#pragma once

#include <memory>
#include <string>
#include <vector>

#include <arbiter/driver.hpp>
#include <arbiter/drivers/http.hpp>

namespace arbiter
{

class AwsAuth
{
public:
    AwsAuth(std::string access, std::string hidden);

    std::string access() const;
    std::string hidden() const;

private:
    std::string m_access;
    std::string m_hidden;
};

class S3Driver : public Driver
{
public:
    S3Driver(const AwsAuth& awsAuth);
    ~S3Driver();

    virtual std::string type() const { return "s3"; }
    virtual std::vector<char> get(std::string path);
    virtual void put(std::string path, const std::vector<char>& data);

private:
    virtual std::vector<std::string> glob(std::string path, bool verbose);

    // TODO Move somewhere else.
    HttpResponse httpExec(std::function<HttpResponse()> f, std::size_t tries);

    typedef std::map<std::string, std::string> Query;

    HttpResponse tryGet(
            std::string bucket,
            std::string object,
            Query query = Query());

    HttpResponse tryPut(std::string file, const std::vector<char>& data);

    std::vector<std::string> httpGetHeaders(std::string filePath) const;
    std::vector<std::string> httpPutHeaders(std::string filePath) const;

    std::string getHttpDate() const;

    std::string getSignedEncodedString(
            std::string command,
            std::string file,
            std::string httpDate,
            std::string contentType = "") const;

    std::string getStringToSign(
            std::string command,
            std::string file,
            std::string httpDate,
            std::string contentType) const;

    std::vector<char> signString(std::string input) const;
    std::string encodeBase64(std::vector<char> input) const;

    AwsAuth m_auth;
    std::shared_ptr<CurlBatch> m_curlBatch;
};

} // namespace arbiter

