#include <algorithm>
#include <cstring>
#include <ios>
#include <iostream>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/curl.hpp>
#include <arbiter/util/http.hpp>
#include <arbiter/util/util.hpp>
#include <arbiter/util/json.hpp>


#ifdef ARBITER_ZLIB
#include <arbiter/third/gzip/decompress.hpp>
#endif

#endif

#ifdef ARBITER_CURL
#include <curl/curl.h>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace http
{

namespace
{
#ifdef ARBITER_CURL
    struct PutData
    {
        PutData(const std::vector<char>& data)
            : data(data)
            , offset(0)
        { }

        const std::vector<char>& data;
        std::size_t offset;
    };

    std::size_t getCb(
            const char* in,
            std::size_t size,
            std::size_t num,
            std::vector<char>* out)
    {
        const std::size_t fullBytes(size * num);
        const std::size_t startSize(out->size());

        out->resize(out->size() + fullBytes);
        std::memcpy(out->data() + startSize, in, fullBytes);

        return fullBytes;
    }

    std::size_t putCb(
            char* out,
            std::size_t size,
            std::size_t num,
            PutData* in)
    {
        const std::size_t fullBytes(
                (std::min)(
                    size * num,
                    in->data.size() - in->offset));
        std::memcpy(out, in->data.data() + in->offset, fullBytes);

        in->offset += fullBytes;
        return fullBytes;
    }

    std::size_t headerCb(
            const char *buffer,
            std::size_t size,
            std::size_t num,
            http::Headers* out)
    {
        const std::size_t fullBytes(size * num);

        std::string data(buffer, fullBytes);
        data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());
        data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());

        const std::size_t split(data.find_first_of(":"));

        // No colon means it isn't a header with data.
        if (split == std::string::npos) return fullBytes;

        const std::string key(data.substr(0, split));
        const std::string val(data.substr(split + 1, data.size()));

        (*out)[key] = val;

        return fullBytes;
    }

    std::size_t eatLogging(void *out, size_t size, size_t num, void *in)
    {
        return size * num;
    }

#else
    const std::string fail("Arbiter was built without curl");
#endif // ARBITER_CURL
} // unnamed namespace

Curl::Curl(const std::string s)
{
#ifdef ARBITER_CURL
    const json c(s.size() ? json::parse(s) : json::object());

    using namespace util;

    m_curl = curl_easy_init();

    // Configurable entries are:
    //      - timeout           (CURLOPT_LOW_SPEED_TIME)
    //      - followRedirect    (CURLOPT_FOLLOWLOCATION)
    //      - caBundle          (CURLOPT_CAPATH)
    //      - caInfo            (CURLOPT_CAINFO)
    //      - verifyPeer        (CURLOPT_SSL_VERIFYPEER)

    using Keys = std::vector<std::string>;
    auto find([](const Keys& keys)->std::unique_ptr<std::string>
    {
        for (const auto& key : keys)
        {
            if (auto e = util::env(key)) return makeUnique<std::string>(*e);
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

    if (auto v = find(verboseKeys)) m_verbose = !!std::stol(*v);
    if (auto v = find(timeoutKeys)) m_timeout = std::stol(*v);
    if (auto v = find(redirKeys)) m_followRedirect = !!std::stol(*v);
    if (auto v = find(verifyKeys)) m_verifyPeer = !!std::stol(*v);
    if (auto v = find(caPathKeys)) m_caPath = mk(*v);
    if (auto v = find(caInfoKeys)) m_caInfo = mk(*v);

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
            std::endl;
    }
#endif
}

Curl::~Curl()
{
#ifdef ARBITER_CURL
    curl_easy_cleanup(m_curl);
    curl_slist_free_all(m_headers);
    m_headers = nullptr;
#endif
}

void Curl::init(
        const std::string rawPath,
        const Headers& headers,
        const Query& query)
{
#ifdef ARBITER_CURL
    // Reset our curl instance and header list.
    curl_slist_free_all(m_headers);
    m_headers = nullptr;

    // Set path.
    const std::string path(rawPath + buildQueryString(query));
    curl_easy_setopt(m_curl, CURLOPT_URL, path.c_str());

    // Needed for multithreaded Curl usage.
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);

    // Substantially faster DNS lookups without IPv6.
    curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    // Don't wait forever.  Use the low-speed options instead of the timeout
    // option to make the timeout a sliding window instead of an absolute.
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, m_timeout);

    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    curl_easy_setopt(m_curl, CURLOPT_ACCEPTTIMEOUT_MS, 2000L);

    auto toLong([](bool b) { return b ? 1L : 0L; });

    // Configuration options.
    curl_easy_setopt(m_curl, CURLOPT_VERBOSE, toLong(m_verbose));
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, toLong(m_followRedirect));
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, toLong(m_verifyPeer));
    if (m_caPath) curl_easy_setopt(m_curl, CURLOPT_CAPATH, m_caPath->c_str());
    if (m_caInfo) curl_easy_setopt(m_curl, CURLOPT_CAINFO, m_caInfo->c_str());

    // Insert supplied headers.
    for (const auto& h : headers)
    {
        m_headers = curl_slist_append(
                m_headers,
                (h.first + ": " + h.second).c_str());
    }
#else
    throw ArbiterError(fail);
#endif
}

int Curl::perform()
{
#ifdef ARBITER_CURL
    long httpCode(0);

    const auto code(curl_easy_perform(m_curl));
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_reset(m_curl);

    if (code != CURLE_OK) httpCode = 500;

    return httpCode;
#else
    throw ArbiterError(fail);
#endif
}

Response Curl::get(
        std::string path,
        Headers headers,
        Query query,
        const std::size_t reserve)
{
#ifdef ARBITER_CURL
    std::vector<char> data;

    if (reserve) data.reserve(reserve);

    init(path, headers, query);

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &data);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    Headers receivedHeaders;
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &receivedHeaders);

    // Run the command.
    const int httpCode(perform());

    for (auto& h : receivedHeaders)
    {
        std::string& v(h.second);
        while (v.size() && v.front() == ' ') v = v.substr(1);
        while (v.size() && v.back() == ' ') v.pop_back();
    }

    if (receivedHeaders["Content-Encoding"] == "gzip")
    {
#ifdef ARBITER_ZLIB
        std::string s(gzip::decompress(data.data(), data.size()));
        data.assign(s.begin(), s.end());
#else
        throw ArbiterError("Cannot decompress zlib");
#endif
    }

    return Response(httpCode, data, receivedHeaders);
#else
    throw ArbiterError(fail);
#endif
}

Response Curl::head(std::string path, Headers headers, Query query)
{
#ifdef ARBITER_CURL
    std::vector<char> data;

    init(path, headers, query);

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &data);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    Headers receivedHeaders;
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &receivedHeaders);

    // Specify a HEAD request.
    curl_easy_setopt(m_curl, CURLOPT_NOBODY, 1L);

    // Run the command.
    const int httpCode(perform());
    return Response(httpCode, data, receivedHeaders);
#else
    throw ArbiterError(fail);
#endif
}

Response Curl::put(
        std::string path,
        const std::vector<char>& data,
        Headers headers,
        Query query)
{
#ifdef ARBITER_CURL
    init(path, headers, query);

    std::unique_ptr<PutData> putData(new PutData(data));

    // Register callback function and data pointer to create the request.
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, putCb);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, putData.get());

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Specify that this is a PUT request.
    curl_easy_setopt(m_curl, CURLOPT_PUT, 1L);

    // Must use this for binary data, otherwise curl will use strlen(), which
    // will likely be incorrect.
    curl_easy_setopt(
            m_curl,
            CURLOPT_INFILESIZE_LARGE,
            static_cast<curl_off_t>(data.size()));

    // Hide Curl's habit of printing things to console even with verbose set
    // to false.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, eatLogging);

    // Run the command.
    const int httpCode(perform());
    return Response(httpCode);
#else
    throw ArbiterError(fail);
#endif
}

Response Curl::post(
        std::string path,
        const std::vector<char>& data,
        Headers headers,
        Query query)
{
#ifdef ARBITER_CURL
    init(path, headers, query);

    std::unique_ptr<PutData> putData(new PutData(data));
    std::vector<char> writeData;

    // Register callback function and data pointer to create the request.
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, putCb);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, putData.get());

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &writeData);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    Headers receivedHeaders;
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &receivedHeaders);

    // Specify that this is a POST request.
    curl_easy_setopt(m_curl, CURLOPT_POST, 1L);

    // Must use this for binary data, otherwise curl will use strlen(), which
    // will likely be incorrect.
    curl_easy_setopt(
            m_curl,
            CURLOPT_INFILESIZE_LARGE,
            static_cast<curl_off_t>(data.size()));

    // Run the command.
    const int httpCode(perform());
    return Response(httpCode, writeData, receivedHeaders);
#else
    throw ArbiterError(fail);
#endif
}

} // namepace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

