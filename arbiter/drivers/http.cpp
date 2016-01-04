#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/http.hpp>
#endif

#ifdef ARBITER_WINDOWS
#undef min
#undef max
#endif

#include <algorithm>
#include <cstring>
#include <iostream>

namespace
{
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
            arbiter::Headers* out)
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

    size_t eatLogging(void *out, size_t size, size_t num, void *in)
    {
        return size * num;
    }

    const bool followRedirect(true);

    const auto baseSleepTime(std::chrono::milliseconds(1));
    const auto maxSleepTime (std::chrono::milliseconds(4096));

    const std::map<char, std::string> sanitizers
    {
        { ' ', "%20" },
        { '!', "%21" },
        { '"', "%22" },
        { '#', "%23" },
        { '$', "%24" },
        { '\'', "%27" },
        { '(', "%28" },
        { ')', "%29" },
        { '*', "%2A" },
        { '+', "%2B" },
        { ',', "%2C" },
        { ';', "%3B" },
        { '<', "%3C" },
        { '>', "%3E" },
        { '@', "%40" },
        { '[', "%5B" },
        { '\\', "%5C" },
        { ']', "%5D" },
        { '^', "%5E" },
        { '`', "%60" },
        { '{', "%7B" },
        { '|', "%7C" },
        { '}', "%7D" },
        { '~', "%7E" }
    };
}

namespace arbiter
{
namespace drivers
{

Http::Http(HttpPool& pool) : m_pool(pool) { }

std::unique_ptr<Http> Http::create(HttpPool& pool, const Json::Value&)
{
    return std::unique_ptr<Http>(new Http(pool));
}

bool Http::get(std::string path, std::vector<char>& data) const
{
    bool good(false);

    auto http(m_pool.acquire());
    HttpResponse res(http.get(path));

    if (res.ok())
    {
        data = res.data();
        good = true;
    }

    return good;
}

void Http::put(std::string path, const std::vector<char>& data) const
{
    auto http(m_pool.acquire());

    if (!http.put(path, data).ok())
    {
        throw ArbiterError("Couldn't HTTP PUT to " + path);
    }
}

std::string Http::sanitize(std::string path)
{
    std::string result;

    for (const auto c : path)
    {
        auto it(sanitizers.find(c));

        if (it == sanitizers.end())
        {
            result += c;
        }
        else
        {
            result += it->second;
        }
    }

    return result;
}

} // namespace drivers

Curl::Curl(bool verbose)
    : m_curl(0)
    , m_headers(0)
    , m_verbose(verbose)
    , m_data()
{
    m_curl = curl_easy_init();
}

Curl::~Curl()
{
    curl_easy_cleanup(m_curl);
    curl_slist_free_all(m_headers);
    m_headers = 0;
}

void Curl::init(std::string path, const Headers& headers)
{
    // Reset our curl instance and header list.
    curl_slist_free_all(m_headers);
    m_headers = 0;

    // Set path.
    curl_easy_setopt(m_curl, CURLOPT_URL, path.c_str());

    // Needed for multithreaded Curl usage.
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);

    // Substantially faster DNS lookups without IPv6.
    curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    // Don't wait forever.
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 120);

    // Configuration options.
    if (followRedirect) curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Insert supplied headers.
    for (const auto& h : headers)
    {
        m_headers = curl_slist_append(
                m_headers,
                (h.first + ": " + h.second).c_str());
    }
}

HttpResponse Curl::get(std::string path, Headers headers)
{
    int httpCode(0);
    std::vector<char> data;

    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    path = drivers::Http::sanitize(path);
    init(path, headers);

    // Register callback function and date pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &data);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Run the command.
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_reset(m_curl);
    return HttpResponse(httpCode, data);
}

HttpResponse Curl::put(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    path = drivers::Http::sanitize(path);
    init(path, headers);

    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    int httpCode(0);

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
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_reset(m_curl);
    return HttpResponse(httpCode);
}

HttpResponse Curl::post(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    path = drivers::Http::sanitize(path);
    init(path, headers);
    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    int httpCode(0);

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
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_reset(m_curl);
    HttpResponse response(httpCode, writeData, receivedHeaders);
    return response;
}


///////////////////////////////////////////////////////////////////////////////

HttpResource::HttpResource(
        HttpPool& pool,
        Curl& curl,
        const std::size_t id,
        const std::size_t retry)
    : m_pool(pool)
    , m_curl(curl)
    , m_id(id)
    , m_retry(retry)
{ }

HttpResource::~HttpResource()
{
    m_pool.release(m_id);
}

HttpResponse HttpResource::get(
        const std::string path,
        const Headers headers)
{
    auto f([this, path, headers]()->HttpResponse
    {
        return m_curl.get(path, headers);
    });

    return exec(f);
}

HttpResponse HttpResource::put(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    auto f([this, path, &data, headers]()->HttpResponse
    {
        return m_curl.put(path, data, headers);
    });

    return exec(f);
}

HttpResponse HttpResource::post(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    auto f([this, path, &data, headers]()->HttpResponse
    {
        return m_curl.post(path, data, headers);
    });

    return exec(f);
}

HttpResponse HttpResource::exec(std::function<HttpResponse()> f)
{
    HttpResponse res;
    std::size_t tries(0);

    do
    {
        res = f();
    }
    while (res.serverError() && tries++ < m_retry);

    return res;
}

///////////////////////////////////////////////////////////////////////////////

HttpPool::HttpPool(std::size_t concurrent, std::size_t retry, bool verbose)
    : m_curls(concurrent)
    , m_available(concurrent)
    , m_retry(retry)
    , m_mutex()
    , m_cv()
{
    for (std::size_t i(0); i < concurrent; ++i)
    {
        m_available[i] = i;
        m_curls[i].reset(new Curl(verbose));
    }
}

HttpResource HttpPool::acquire()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return !m_available.empty(); });

    const std::size_t id(m_available.back());
    Curl& curl(*m_curls[id]);

    m_available.pop_back();

    return HttpResource(*this, curl, id, m_retry);
}

void HttpPool::release(const std::size_t id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_available.push_back(id);
    lock.unlock();

    m_cv.notify_one();
}

} // namespace arbiter

