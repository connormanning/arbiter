#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#include <arbiter/util/types.hpp>
#endif

#include <curl/curl.h>

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

    enum class State
    {
        UNUSED,     // Waiting to be used for a request.
        ACQUIRED,   // Acquired by a Resource.
        READY,      // Initialization complete. Ready to be run.
        RUNNING,    // Running.
        DONE        // Operation completed
    };

public:
    Curl(std::size_t id, const std::string& config);
    Curl(Curl&& curl);
    ~Curl();

    void prepareGet(
            std::string path,
            Headers headers,
            Query query,
            std::size_t reserve,
            std::size_t timeout = 0);

    void prepareHead(
            std::string path,
            Headers headers,
            Query query,
            std::size_t timeout = 0);

    void preparePut(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query,
            std::size_t timeout = 0);

    void preparePost(
            std::string path,
            const std::vector<char>& data,
            Headers headers,
            Query query,
            std::size_t timeout = 0);

    // Note that the response is *moved*.
    Response response()
    {
        m_response.setCode(m_code);
        return std::move(m_response);
    }

    std::size_t id() const
    { return m_id; }

private:
    void init(const std::string& path, const Headers& headers, const Query& query);

    CURL* m_curl = nullptr;
    curl_slist* m_headers = nullptr;

    std::size_t m_id;
    State m_state = State::UNUSED;
    bool m_verbose = false;
    long m_timeout = defaultHttpTimeout;
    bool m_followRedirect = true;
    bool m_verifyPeer = true;
    int m_code = 0;
    std::unique_ptr<std::string> m_caPath;
    std::unique_ptr<std::string> m_caInfo;
    std::unique_ptr<std::string> m_proxy;
    Response m_response;
    PutData m_putData;
};

/** @endcond */

} // namespace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

