#include <algorithm>
#include <cstring>
#include <ios>
#include <iostream>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/curl.hpp>
#include <arbiter/util/http.hpp>
#include <arbiter/util/util.hpp>
#include <arbiter/util/json.hpp>
#endif

#include <curl/curl.h>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace http
{

Curl::Curl(std::size_t id, const std::string& s) : m_id(id)
{
    const json c(s.size() ? json::parse(s) : json::object());

    m_curl = curl_easy_init();

    // Configurable entries are:
    //      - timeout           (CURLOPT_LOW_SPEED_TIME)
    //      - followRedirect    (CURLOPT_FOLLOWLOCATION)
    //      - caBundle          (CURLOPT_CAPATH)
    //      - caInfo            (CURLOPT_CAINFO)
    //      - verifyPeer        (CURLOPT_SSL_VERIFYPEER)
    //      - proxy             (CURLOPT_PROXY)

    using Keys = std::vector<std::string>;
    auto find([](const Keys& keys)->std::unique_ptr<std::string>
    {
        for (const auto& key : keys)
        {
            if (auto e = env(key)) return makeUnique<std::string>(*e);
        }
        return std::unique_ptr<std::string>();
    });

    auto mk([](std::string s) { return makeUnique<std::string>(s); });

    if (!c.is_null())
    {
        m_verbose = c.value("verbose", false);
        const auto& h(c.value("http", json::object()));

        if (!h.is_null())
        {
            if (h.count("timeout"))
            {
                m_timeout = h["timeout"].get<long>();
            }

            if (h.count("followRedirect"))
            {
                m_followRedirect = h["followRedirect"].get<bool>();
            }

            if (h.count("caBundle"))
            {
                m_caPath = mk(h["caBundle"].get<std::string>());
            }
            else if (h.count("caPath"))
            {
                m_caPath = mk(h["caPath"].get<std::string>());
            }

            if (h.count("caInfo"))
            {
                m_caInfo = mk(h["caInfo"].get<std::string>());
            }

            if (h.count("Proxy"))
            {
                m_proxy = mk(h["Proxy"].get<std::string>());
            }

            if (h.count("verifyPeer"))
            {
                m_verifyPeer = h["verifyPeer"].get<bool>();
            }
        }
    }

    Keys verboseKeys{ "VERBOSE", "CURL_VERBOSE", "ARBITER_VERBOSE" };
    Keys timeoutKeys{ "CURL_TIMEOUT", "ARBITER_HTTP_TIMEOUT" };
    Keys redirKeys{
        "CURL_FOLLOWLOCATION",
        "CURL_FOLLOW_LOCATION",
        "ARBITER_FOLLOW_LOCATION"
        "ARBITER_FOLLOW_REDIRECT"
    };
    Keys verifyKeys{
        "CURL_SSL_VERIFYPEER",
        "CURL_VERIFY_PEER",
        "ARBITER_VERIFY_PEER"
    };
    Keys caPathKeys{ "CURL_CA_PATH", "CURL_CA_BUNDLE", "ARBITER_CA_PATH" };
    Keys caInfoKeys{ "CURL_CAINFO", "CURL_CA_INFO", "ARBITER_CA_INFO" };
    Keys ProxyKeys{ "CURL_PROXY", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "ARBITER_PROXY"};

    if (auto v = find(verboseKeys)) m_verbose = !!std::stol(*v);
    if (auto v = find(timeoutKeys)) m_timeout = std::stol(*v);
    if (auto v = find(redirKeys)) m_followRedirect = !!std::stol(*v);
    if (auto v = find(verifyKeys)) m_verifyPeer = !!std::stol(*v);
    if (auto v = find(caPathKeys)) m_caPath = mk(*v);
    if (auto v = find(caInfoKeys)) m_caInfo = mk(*v);
    if (auto v = find(ProxyKeys)) m_proxy = mk(*v);

    static bool logged(false);
    if (m_verbose && !logged)
    {
        logged = true;
        std::cout << "Curl config:" << std::boolalpha <<
            "\n\ttimeout: " << m_timeout << "s" <<
            "\n\tfollowRedirect: " << m_followRedirect <<
            "\n\tverifyPeer: " << m_verifyPeer <<
            "\n\tcaBundle: " << (m_caPath ? *m_caPath : "(default)") <<
            "\n\tcaInfo: " << (m_caInfo ? *m_caInfo : "(default)") <<
            "\n\tProxy: " << (m_proxy ? *m_proxy : "(default)") <<
            std::endl;
    }
}

Curl::~Curl()
{
    if (m_curl)
        curl_easy_cleanup(m_curl);
    if (m_headers)
        curl_slist_free_all(m_headers);
}

// The only time this should be invoked is when things are moving in a vector of Curls.
Curl::Curl(Curl&& other)
{
    m_curl = other.m_curl; other.m_curl = nullptr;
    m_headers = other.m_headers; other.m_headers = nullptr;
    m_id = other.m_id;
    m_code = other.m_code;
    m_state = other.m_state; other.m_state = State::UNUSED;
    m_verbose = other.m_verbose;
    m_timeout = other.m_timeout;
    m_followRedirect = other.m_followRedirect;
    m_verifyPeer = other.m_verifyPeer;
    m_caPath = std::move(other.m_caPath);
    m_caInfo = std::move(other.m_caInfo);
    m_proxy = std::move(other.m_proxy);
    m_response = std::move(other.m_response);
    m_putData = std::move(other.m_putData);
}

void Curl::init(
        const std::string& rawPath,
        const Headers& headers,
        const Query& query)
{
    // Reset our curl instance and header list.
    curl_slist_free_all(m_headers);
    m_headers = nullptr;

    // Set path.
    const std::string path(rawPath + buildQueryString(query));
    curl_easy_setopt(m_curl, CURLOPT_URL, path.c_str());

    curl_version_info_data* versioninfo = curl_version_info(CURLVERSION_NOW);
    if (!(versioninfo->features & CURL_VERSION_ASYNCHDNS))
    {
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    }

    // Substantially faster DNS lookups without IPv6.
    curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    // See https://curl.se/libcurl/c/CURLOPT_ACCEPT_ENCODING.html
    //
    // To aid applications not having to bother about what specific algorithms
    // this particular libcurl build supports, libcurl allows a zero-length
    // string to be set ("") to ask for an Accept-Encoding: header to be used
    // that contains all built-in supported encodings.
    curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, "");

    // Don't wait forever.  Use the low-speed options instead of the timeout
    // option to make the timeout a sliding window instead of an absolute.
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, m_timeout);

    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, 1000L);
    curl_easy_setopt(m_curl, CURLOPT_ACCEPTTIMEOUT_MS, 1000L);

    auto toLong([](bool b) { return b ? 1L : 0L; });

    // Configuration options.
    curl_easy_setopt(m_curl, CURLOPT_VERBOSE, toLong(m_verbose));
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, toLong(m_followRedirect));
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, toLong(m_verifyPeer));
    if (m_caPath) curl_easy_setopt(m_curl, CURLOPT_CAPATH, m_caPath->c_str());
    if (m_caInfo) curl_easy_setopt(m_curl, CURLOPT_CAINFO, m_caInfo->c_str());
    if (m_proxy) curl_easy_setopt(m_curl, CURLOPT_PROXY, m_proxy->c_str());

    // Insert supplied headers.
    for (const auto& h : headers)
    {
        m_headers = curl_slist_append(
                m_headers,
                (h.first + ": " + h.second).c_str());
    }
}

void Curl::prepareGet(
        std::string path,
        Headers headers,
        Query query,
        const std::size_t reserve,
        const std::size_t timeout)
{
    m_response.init(reserve);

    init(path, headers, query);
    if (timeout) curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, timeout);

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Response::getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_response);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, Response::headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &m_response);

void Curl::prepareHead(
    std::string path,
    Headers headers,
    Query query,
    const std::size_t timeout)
{
    m_response.init();

    init(path, headers, query);
    if (timeout)
        curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, timeout);

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Response::getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_response);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, Response::headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &m_response);

    // Specify a HEAD request.
    curl_easy_setopt(m_curl, CURLOPT_NOBODY, 1L);
}

void Curl::preparePut(
        std::string path,
        const std::vector<char>& data,
        Headers headers,
        Query query,
        const std::size_t timeout)
{
    m_response.init();
    m_putData.init(data);
    init(path, headers, query);
    if (timeout) curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, timeout);


    // Register callback function and data pointer to create the request.
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, PutData::putCb);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, &m_putData);

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Response::getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_response);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, Response::headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &m_response);

    // Specify that this is a PUT request.
    curl_easy_setopt(m_curl, CURLOPT_PUT, 1L);

    // Must use this for binary data, otherwise curl will use strlen(), which
    // will likely be incorrect.
    curl_easy_setopt(
            m_curl,
            CURLOPT_INFILESIZE_LARGE,
            static_cast<curl_off_t>(data.size()));
}

void Curl::preparePost(
        std::string path,
        const std::vector<char>& data,
        Headers headers,
        Query query,
        const std::size_t timeout)
{
    m_response.init();
    m_putData.init(data);
    init(path, headers, query);
    if (timeout) curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, timeout);

    // Register callback function and data pointer to create the request.
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, PutData::putCb);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, &m_putData);

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Response::getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_response);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, Response::headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &m_response);

    // Specify that this is a POST request.
    curl_easy_setopt(m_curl, CURLOPT_POST, 1L);

    // Must use this for binary data, otherwise curl will use strlen(), which
    // will likely be incorrect.
    curl_easy_setopt(
            m_curl,
            CURLOPT_INFILESIZE_LARGE,
            static_cast<curl_off_t>(data.size()));
}

} // namepace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

