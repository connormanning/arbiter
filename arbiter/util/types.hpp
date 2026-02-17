#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

/** @brief Exception class for all internally thrown runtime errors. */
class ArbiterError : public std::runtime_error
{
public:
    ArbiterError(std::string msg) : std::runtime_error(msg) { }
};

namespace http
{

/** HTTP header fields. */
using Headers = std::map<std::string, std::string>;

/** HTTP query parameters. */
using Query = std::map<std::string, std::string>;

/** @cond arbiter_internal */

class PutData
{
public:
    void init(const std::vector<char>& data)
    {
        m_data = data;
        m_offset = 0;
    }

    void init(std::vector<char>&& data)
    {
        m_data = std::move(data);
        m_offset = 0;
    }

    static size_t putCb(char* out, std::size_t size, std::size_t num, void *cbData)
    {
        PutData& putData = *static_cast<PutData *>(cbData);

        size *= num;  // Size is really size * num;
        return putData.extract(out, size);
    }

private:
    size_t extract(char *out, size_t size)
    {
        size_t remaining = m_data.size() - m_offset;
        size_t extractCount = (std::min)(size, remaining);

        std::memcpy(out, m_data.data() + m_offset, extractCount);
        m_offset += extractCount;
        return extractCount;
    }

    std::vector<char> m_data;
    size_t m_offset = 0;
};

class Response
{
public:
    void init(std::size_t reserve)
    {
        m_data.reserve(reserve);
        init();
    }

    void init()
    {
        m_code = 0;
        m_data.clear();
        m_headers.clear();
    }

    bool ok() const             { return m_code / 100 == 2; }
    bool clientError() const    { return m_code / 100 == 4; }
    bool serverError() const    { return m_code / 100 == 5; }
    int code() const            { return m_code; }
    void setCode(long code)     { m_code = code; }

    // We move data out of the response, so only call once.
    std::vector<char>&& data() { return std::move(m_data); }
    Headers headers() { return std::move(m_headers); }
    std::string str()
    {
        std::string s(m_data.data(), m_data.size());
        // Clear data for consistency with data().
        m_data.clear();
        return s;
    }

    static size_t getCb(const char *in, size_t size, size_t num, void *cbData)
    {
        Response& response = *static_cast<Response *>(cbData);

        size *= num;  // Size is really size * num.
        response.append(in, size);
        return size;
    }

    static size_t headerCb(const char *in, std::size_t size, std::size_t num, void *cbData)
    {
        Response& response = *static_cast<Response *>(cbData);

        size *= num;  // Size is really size * num.
        response.addHeaders(in, size);
        return size;
    }

private:
    void append(const char *in, size_t size)
    {
        std::size_t startSize = m_data.size();
        m_data.resize(startSize + size);
        std::memcpy(m_data.data() + startSize, in, size);
    }

    void addHeaders(const char *in, size_t size)
    {
        // Not sure the case where we'd have \r\n where we'd also have a valid
        // header, but older code handled this.
        // Remove leading \r or \n
        while (size && (*in == '\n' || *in == '\r'))
        {
            in++;
            size--;
        }

        // Remove trailing \r or \n
        const char *end = in + size;
        while (size && (*end == '\n' || *end == '\r'))
        {
            end--;
            size--;
        }

        std::string_view data(in, size);

        const std::size_t split(data.find_first_of(":"));

        // No colon means it isn't a header with data.
        if (split != std::string::npos)
        {
            std::string_view key(data.substr(0, split));
            std::string_view val(data.substr(split + 1));
            m_headers.emplace(key, val);
        }
    }

    long m_code;
    std::vector<char> m_data;
    Headers m_headers;
};

/** @endcond */

} // namespace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

