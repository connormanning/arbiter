#pragma once

#include <memory>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
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

/** @brief AWS authentication information. */
class AwsAuth
{
public:
    AwsAuth(std::string access, std::string hidden);

    /** @brief Search for credentials in some common locations.
     *
     * See:
     * https://docs.aws.amazon.com/AWSJavaScriptSDK/guide/node-configuring.html
     *
     * Uses methods 2 and 3 of "Setting AWS Credentials":
     *      - Check for them in `~/.aws/credentials`.
     *      - If not found, try the environment settings.
     */
    static std::unique_ptr<AwsAuth> find(std::string profile = "");

    std::string access() const;
    std::string hidden() const;

private:
    std::string m_access;
    std::string m_hidden;
};

/** @brief Amazon %S3 driver. */
class S3 : public CustomHeaderDriver
{
public:
    S3(
            HttpPool& pool,
            AwsAuth awsAuth,
            std::string region = "us-east-1",
            bool sse = false);

    /** Try to construct an S3 Driver.  Searches @p json primarily for the keys
     * `access` and `hidden` to construct an AwsAuth.  If not found, common
     * filesystem locations and then the environment will be searched (see
     * AwsAuth::find).
     *
     * Server-side encryption may be enabled by setting key `sse` to `true` in
     * @p json.
     */
    static std::unique_ptr<S3> create(HttpPool& pool, const Json::Value& json);
    static std::string extractProfile(const Json::Value& json);

    virtual std::string type() const override { return "s3"; }

    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    virtual void put(
            std::string path,
            const std::vector<char>& data) const override;

    /** Inherited from CustomHeaderDriver. */
    virtual std::string get(std::string path, Headers headers) const override;

    /** Inherited from CustomHeaderDriver. */
    virtual std::vector<char> getBinary(
            std::string path,
            Headers headers) const override;

private:
    virtual bool get(std::string path, std::vector<char>& data) const override;
    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    bool get(
            std::string rawPath,
            const Query& query,
            const Headers& headers,
            std::vector<char>& data) const;

    struct Resource
    {
        Resource(std::string baseUrl, std::string fullPath);

        std::string buildPath(Query query = Query()) const;
        std::string host() const;

        std::string baseUrl;
        std::string bucket;
        std::string object;
    };

    class FormattedTime
    {
    public:
        FormattedTime();

        const std::string& date() const { return m_date; }
        const std::string& time() const { return m_time; }

        std::string amazonDate() const
        {
            return date() + 'T' + time() + 'Z';
        }

    private:
        std::string formatTime(const std::string& format) const;

        const std::string m_date;
        const std::string m_time;
    };

    class AuthV4
    {
    public:
        AuthV4(
                std::string verb,
                const std::string& region,
                const Resource& resource,
                const AwsAuth& auth,
                const Query& query,
                const Headers& headers,
                const std::vector<char>& data);

        const Headers& headers() const { return m_headers; }

        const std::string& signedHeadersString() const
        {
            return m_signedHeadersString;
        }

    private:
        std::string buildCanonicalRequest(
                std::string verb,
                const Resource& resource,
                const Query& query,
                const std::vector<char>& data) const;

        std::string buildStringToSign(
                const std::string& canonicalRequest) const;

        std::string calculateSignature(
                const std::string& stringToSign) const;

        std::string getAuthHeader(
                const std::string& signedHeadersString,
                const std::string& signature) const;

        const AwsAuth& m_auth;
        const std::string m_region;
        const FormattedTime m_formattedTime;

        Headers m_headers;
        std::string m_canonicalHeadersString;
        std::string m_signedHeadersString;
    };

    HttpPool& m_pool;
    AwsAuth m_auth;

    std::string m_region;
    std::string m_baseUrl;
    Headers m_baseHeaders;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

