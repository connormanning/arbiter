#include <algorithm>
#include <cstring>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/curl.hpp>
#include <arbiter/util/http.hpp>
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
                std::min(
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

    const bool followRedirect(true);
#else
    const std::string fail("Arbiter was built without curl");
#endif // ARBITER_CURL
} // unnamed namespace

Curl::Curl(bool verbose, std::size_t timeout)
    : m_curl(nullptr)
    , m_headers(nullptr)
    , m_verbose(verbose)
    , m_timeout(timeout)
    , m_data()
{
#ifdef ARBITER_CURL
    m_curl = curl_easy_init();
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

    // Configuration options.
    if (followRedirect) curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);

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
    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

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
    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

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
    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

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
    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

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

