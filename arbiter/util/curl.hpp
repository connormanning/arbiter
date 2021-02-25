#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#include <arbiter/util/types.hpp>
#endif

#ifdef ARBITER_CURL
#include <curl/curl.h>
#else
typedef void CURL;
#endif

struct curl_slist;

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace http
{

/** @cond arbiter_internal */

class Pool;

class ARBITER_DLL Curl
{
    friend class Pool;

    static constexpr std::size_t defaultHttpTimeout = 5;

public:
    ~Curl();

    http::Response get(
            std::string path,
            Headers headers,
            Query query,
            std::size_t reserve);

    http::Response head(std::string path, Headers headers, Query query);

    http::Response put(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query);

    http::Response post(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query);

private:
    Curl(std::string j);

    void init(std::string path, const Headers& headers, const Query& query);

    // Returns HTTP status code.
    int perform();

    Curl(const Curl&);
    Curl& operator=(const Curl&);

    CURL* m_curl = nullptr;
    curl_slist* m_headers = nullptr;

    bool m_verbose = false;
    long m_timeout = defaultHttpTimeout;
    bool m_followRedirect = true;
    bool m_verifyPeer = true;
    std::unique_ptr<std::string> m_caPath;
    std::unique_ptr<std::string> m_caInfo;
    std::unique_ptr<std::string> m_proxy;

    std::vector<char> m_data;
};

/** @endcond */

} // namespace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

