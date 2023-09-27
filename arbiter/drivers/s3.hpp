#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/time.hpp>
#include <arbiter/util/util.hpp>
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
    class Auth;
    class AuthFields;
    class Config;

public:
    S3(
            http::Pool& pool,
            std::string profile,
            std::unique_ptr<Auth> auth,
            std::unique_ptr<Config> config);

    /** Try to construct an S3 driver.  The configuration/credential discovery
     * follows, in order:
     *      - Environment settings.
     *      - JSON configuration
     *      - Well-known files or their environment overrides, like
     *          `~/.aws/credentials` or the file at AWS_CREDENTIAL_FILE.
     *      - EC2 instance profile.
     */
    static std::unique_ptr<S3> create(
        http::Pool& pool,
        std::string json,
        std::string profile = "default");

    // Overrides.
    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path,
            http::Headers headers,
            http::Query query = http::Query()) const override;

    /** Inherited from Drivers::Http. */
    virtual std::vector<char> put(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers,
            http::Query query) const override;

    virtual void copy(std::string src, std::string dst) const override;

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

    AuthFields authFields() const;

    class ApiV4;
    class Resource;

    std::unique_ptr<Auth> m_auth;
    std::unique_ptr<Config> m_config;
};

class S3::AuthFields
{
public:
    AuthFields(std::string access = "", std::string hidden = "", std::string token = "")
        : m_access(access), m_hidden(hidden), m_token(token)
    { }

    const std::string& access() const { return m_access; }
    const std::string& hidden() const { return m_hidden; }
    const std::string& token() const { return m_token; }

    explicit operator bool() const { return m_access.size() || m_hidden.size() || m_token.size(); }

private:
    std::string m_access;
    std::string m_hidden;
    std::string m_token;
};

class S3::Auth
{
public:
    Auth(std::string access, std::string hidden, std::string token = "")
        : m_access(access)
        , m_hidden(hidden)
        , m_token(token)
    { }

    Auth(std::string credUrl, bool imdsv2 = true)
        : m_credUrl(internal::makeUnique<std::string>(credUrl))
        , m_imdsv2(imdsv2)
    { }

    static std::unique_ptr<Auth> create(std::string profile, std::string s);

    AuthFields fields() const;

private:
    mutable std::string m_access;
    mutable std::string m_hidden;
    mutable std::string m_token;

    std::unique_ptr<std::string> m_credUrl;
    bool m_imdsv2 = true;
    mutable std::unique_ptr<Time> m_expiration;
    mutable std::mutex m_mutex;
};

class S3::Config
{
public:
    Config(std::string json, std::string profile);

    const std::string& region() const { return m_region; }
    const std::string& baseUrl() const { return m_baseUrl; }
    const http::Headers& baseHeaders() const { return m_baseHeaders; }
    bool precheck() const { return m_precheck; }

private:
    static std::string extractRegion(std::string json, std::string profile);
    static std::string extractBaseUrl(std::string json, std::string region);

    const std::string m_region;
    const std::string m_baseUrl;
    http::Headers m_baseHeaders;
    bool m_precheck;
};



class S3::Resource
{
public:
    Resource(std::string baseUrl, std::string fullPath);

    std::string url() const;
    std::string host() const;
    std::string baseUrl() const;
    std::string bucket() const;
    std::string object() const;
    std::string canonicalUri() const;

private:
    std::string m_baseUrl;
    std::string m_bucket;
    std::string m_object;
    bool m_virtualHosted;
};

class S3::ApiV4
{
public:
    ApiV4(
            std::string verb,
            const std::string& region,
            const Resource& resource,
            const S3::AuthFields authFields,
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

    const S3::AuthFields m_authFields;
    const std::string m_region;
    const Time m_time;

    http::Headers m_headers;
    http::Query m_query;
    std::string m_canonicalHeadersString;
    std::string m_signedHeadersString;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

