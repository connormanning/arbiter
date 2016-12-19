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

/** @brief Amazon %S3 driver. */
class S3 : public Http
{
public:
    class Auth;

    S3(
            http::Pool& pool,
            const Auth& auth,
            std::string region = "us-east-1",
            bool sse = false,
            bool precheck = false);

    /** Try to construct an S3 Driver.  Searches @p json primarily for the keys
     * `access` and `hidden` to construct an S3::Auth.  If not found, common
     * filesystem locations and then the environment will be searched (see
     * S3::Auth::find).
     *
     * Server-side encryption may be enabled by setting key `sse` to `true` in
     * @p json.
     */
    static std::unique_ptr<S3> create(
            http::Pool& pool,
            const Json::Value& json);

    static std::string extractProfile(const Json::Value& json);

    virtual std::string type() const override { return "s3"; }

    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    /** Inherited from Drivers::Http. */
    virtual void put(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers,
            http::Query query) const override;

    virtual void copy(std::string src, std::string dst) const override;

    /** @brief AWS authentication information. */
    class Auth
    {
    public:
        Auth(std::string access, std::string hidden);

        /** @brief Search for credentials in some common locations.
         *
         * See:
         * docs.aws.amazon.com/AWSJavaScriptSDK/guide/node-configuring.html
         *
         * Uses methods 2 and 3 of "Setting AWS Credentials":
         *      - Check for them in `~/.aws/credentials`.
         *      - If not found, try the environment settings.
         */
        static std::unique_ptr<Auth> find(std::string profile = "");

        std::string access() const;
        std::string hidden() const;

    private:
        std::string m_access;
        std::string m_hidden;
    };

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

    struct Resource
    {
        Resource(std::string baseUrl, std::string fullPath);

        std::string url() const;
        std::string host() const;
        std::string baseUrl() const { return m_baseUrl; }
        std::string bucket() const { return m_bucket; }
        std::string object() const;

    private:
        std::string m_baseUrl;
        std::string m_bucket;
        std::string m_object;
        bool m_virtualHosted;
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

    class ApiV4
    {
    public:
        ApiV4(
                std::string verb,
                const std::string& region,
                const Resource& resource,
                const S3::Auth& auth,
                const http::Query& query,
                const http::Headers& headers,
                const std::vector<char>& data);

        const http::Headers& headers() const { return m_headers; }
        const http::Query& query() const { return m_query; }

        const std::string& signedHeadersString() const
        {
            return m_signedHeadersString;
        }

    private:
        std::string buildCanonicalRequest(
                std::string verb,
                const Resource& resource,
                const http::Query& query,
                const std::vector<char>& data) const;

        std::string buildStringToSign(
                const std::string& canonicalRequest) const;

        std::string calculateSignature(
                const std::string& stringToSign) const;

        std::string getAuthHeader(
                const std::string& signedHeadersString,
                const std::string& signature) const;

        const S3::Auth& m_auth;
        const std::string m_region;
        const FormattedTime m_formattedTime;

        http::Headers m_headers;
        http::Query m_query;
        std::string m_canonicalHeadersString;
        std::string m_signedHeadersString;
    };

    Auth m_auth;

    std::string m_region;
    std::string m_baseUrl;
    http::Headers m_baseHeaders;
    bool m_precheck;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

