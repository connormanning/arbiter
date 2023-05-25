#pragma once

#include <memory>
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

/** @brief Microsoft %Azure blob driver. */
class AZ : public Http
{
    class AuthFields;
    class Config;

public:
    AZ(
            http::Pool& pool,
            std::string profile,
            std::unique_ptr<Config> config);

    /** Try to construct an Azure blob driver.  The configuration/credential
     * discovery follows, in order:
     *      - Environment settings.
     *      - JSON configuration.
     */
    static std::unique_ptr<AZ> create(
            http::Pool& pool,
            std::string j,
            std::string profile);

    // Overrides.
    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

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

    class ApiV1;
    class Resource;

    std::unique_ptr<Config> m_config;
};

class AZ::AuthFields
{
public:
    AuthFields(std::string account, std::string key="")
        : m_storageAccount(account), m_storageAccessKey(key)
    { }

    const std::string& account() const { return m_storageAccount; }
    const std::string& key() const { return m_storageAccessKey; }

private:
    std::string m_storageAccount;
    std::string m_storageAccessKey;
};

class AZ::Config
{

public:
    Config(std::string j);

    const http::Query& sasToken() const { return m_sasToken; }
    bool hasSasToken() const { return m_sasToken.size() > 0; }
    const std::string& service() const { return m_service; }
    const std::string& storageAccount() const { return m_storageAccount; }
    const std::string& endpoint() const { return m_endpoint; }
    const std::string& baseUrl() const { return m_baseUrl; }
    const http::Headers& baseHeaders() const { return m_baseHeaders; }
    bool precheck() const { return m_precheck; }

    AuthFields authFields() const { return AuthFields(m_storageAccount, m_storageAccessKey); }

private:
    static std::string extractService(std::string j);
    static std::string extractEndpoint(std::string j);
    static std::string extractBaseUrl(std::string j, std::string endpoint, std::string service, std::string account);
    static std::string extractStorageAccount(std::string j);
    static std::string extractStorageAccessKey(std::string j);
    static std::string extractSasToken(std::string j);

    http::Query m_sasToken;
    const std::string m_service;
    const std::string m_storageAccount;
    const std::string m_storageAccessKey;
    const std::string m_endpoint;
    const std::string m_baseUrl;
    http::Headers m_baseHeaders;
    bool m_precheck;
};



class AZ::Resource
{
public:
    Resource(std::string baseUrl, std::string fullPath);

    std::string url() const;
    std::string host() const;
    std::string baseUrl() const;
    std::string bucket() const;
    std::string object() const;
    std::string blob() const;
    std::string storageAccount() const;

private:
    std::string m_baseUrl;
    std::string m_bucket;
    std::string m_object;
    std::string m_storageAccount;
};

class AZ::ApiV1
{
public:
    ApiV1(
            std::string verb,
            const Resource& resource,
            const AZ::AuthFields authFields,
            const http::Query& query,
            const http::Headers& headers,
            const std::vector<char>& data);

    const http::Headers& headers() const { return m_headers; }
    const http::Query& query() const { return m_query; }

private:
    std::string buildCanonicalResource(
            const Resource& resource,
            const http::Query& query) const;

    std::string buildCanonicalHeader(
            http::Headers & msHeaders,
            const http::Headers & existingHeaders) const;

    std::string buildStringToSign(
            const std::string& verb,
            const http::Headers& headers,
            const std::string& canonicalHeaders,
            const std::string& canonicalRequest) const;

    std::string calculateSignature(
            const std::string& stringToSign) const;

    std::string getAuthHeader(
            const std::string& signedHeadersString) const;

    const AZ::AuthFields m_authFields;
    const Time m_time;

    http::Headers m_headers;
    http::Query m_query;
};

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

