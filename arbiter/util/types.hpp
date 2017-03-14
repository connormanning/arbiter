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

class Response
{
public:
    Response(int code = 0)
        : m_code(code)
        , m_data()
    { }

    Response(int code, std::vector<char> data)
        : m_code(code)
        , m_data(data)
    { }

    Response(
            int code,
            const std::vector<char>& data,
            const Headers& headers)
        : m_code(code)
        , m_data(data)
        , m_headers(headers)
    { }

    ~Response() { }

    bool ok() const             { return m_code / 100 == 2; }
    bool clientError() const    { return m_code / 100 == 4; }
    bool serverError() const    { return m_code / 100 == 5; }
    int code() const            { return m_code; }

    std::vector<char> data() const { return m_data; }
    const Headers& headers() const { return m_headers; }

private:
    int m_code;
    std::vector<char> m_data;
    Headers m_headers;
};

/** @endcond */

} // namespace http
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

