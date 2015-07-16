#include <arbiter/drivers/http.hpp>

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

    const bool followRedirect(true);
    const bool verbose(false);
}

namespace arbiter
{

HttpDriver::HttpDriver(HttpPool& pool, const std::size_t retry)
    : m_pool(pool)
    , m_retry(retry)
{ }

std::vector<char> HttpDriver::get(const std::string path)
{
    auto http(m_pool.acquire());
    HttpResponse res(http.get(path, m_retry));

    if (res.ok()) return res.data();
    else throw std::runtime_error("Couldn't HTTP GET " + path);
}

void HttpDriver::put(const std::string path, const std::vector<char>& data)
{
    auto http(m_pool.acquire());

    if (!http.put(path, data, m_retry).ok())
    {
        throw std::runtime_error("Couldn't HTTP GET " + path);
    }
}

Curl::Curl()
    : m_curl(0)
    , m_headers(0)
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

void Curl::init(std::string path, const std::vector<std::string>& headers)
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
    if (verbose)        curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
    if (followRedirect) curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Insert supplied headers.
    for (std::size_t i(0); i < headers.size(); ++i)
    {
        m_headers = curl_slist_append(m_headers, headers[i].c_str());
    }
}

HttpResponse Curl::get(std::string path, std::vector<std::string> headers)
{
    int httpCode(0);
    std::vector<char> data;

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
        std::vector<std::string> headers)
{
    init(path, headers);

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
    curl_easy_setopt(m_curl, CURLOPT_INFILESIZE_LARGE, data.size());

    // Run the command.
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_reset(m_curl);
    return HttpResponse(httpCode);
}

///////////////////////////////////////////////////////////////////////////////

HttpResource::HttpResource(HttpPool& pool, Curl& curl, const std::size_t id)
    : m_pool(pool)
    , m_curl(curl)
    , m_id(id)
{ }

HttpResource::~HttpResource()
{
    m_pool.release(m_id);
}

HttpResponse HttpResource::get(
        const std::string path,
        const std::size_t retry,
        const Headers headers)
{
    auto f([this, path, headers]()->HttpResponse
    {
        return m_curl.get(path, headers);
    });

    return exec(f, retry);
}

HttpResponse HttpResource::put(
        std::string path,
        const std::vector<char>& data,
        const std::size_t retry,
        Headers headers)
{
    auto f([this, path, &data, headers]()->HttpResponse
    {
        return m_curl.put(path, data, headers);
    });

    return exec(f, retry);
}

HttpResponse HttpResource::exec(
        std::function<HttpResponse()> f,
        const std::size_t retry)
{
    HttpResponse res;
    std::size_t tries(0);

    do
    {
        res = f();
    }
    while (res.retry() && tries++ < retry);

    return res;
}

///////////////////////////////////////////////////////////////////////////////

HttpPool::HttpPool(std::size_t concurrent)
    : m_curls(concurrent)
    , m_available(concurrent)
    , m_mutex()
    , m_cv()
{
    for (std::size_t i(0); i < concurrent; ++i)
    {
        m_available[i] = i;
        m_curls[i].reset(new Curl());
    }
}

HttpResource HttpPool::acquire()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return !m_available.empty(); });

    const std::size_t id(m_available.back());
    Curl& curl(*m_curls[id]);

    m_available.pop_back();

    return HttpResource(*this, curl, id);
}

void HttpPool::release(const std::size_t id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_available.push_back(id);
    lock.unlock();

    m_cv.notify_one();
}

} // namespace arbiter

