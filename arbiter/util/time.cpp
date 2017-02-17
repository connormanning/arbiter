#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/time.hpp>
#endif

#include <iomanip>
#include <mutex>
#include <sstream>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/types.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    std::mutex mutex;

    int64_t utcOffsetSeconds()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::time_t now(std::time(nullptr));
        std::tm utc(*std::gmtime(&now));
        std::tm loc(*std::localtime(&now));
        return std::difftime(std::mktime(&utc), std::mktime(&loc));
    }

    std::tm getTm()
    {
        std::tm tm;
        tm.tm_sec = 0;
        tm.tm_min = 0;
        tm.tm_hour = 0;
        tm.tm_mday = 0;
        tm.tm_mon = 0;
        tm.tm_year = 0;
        tm.tm_wday = 0;
        tm.tm_yday = 0;
        tm.tm_isdst = 0;
        return tm;
    }
}

const std::string Time::iso8601 = "%Y-%m-%dT%H:%M:%SZ";
const std::string Time::iso8601NoSeparators = "%Y%m%dT%H%M%SZ";
const std::string Time::dateNoSeparators = "%Y%m%d";

Time::Time()
{
    m_time = std::time(nullptr);
}

Time::Time(const std::string& s, const std::string& format)
{
    static const int64_t utcOffset(utcOffsetSeconds());

    auto tm(getTm());
    std::istringstream ss(s);
    ss >> std::get_time(&tm, format.c_str());
    if (ss.fail())
    {
        throw ArbiterError("Failed to parse " + s + " as time: " + format);
    }
    tm.tm_sec -= utcOffset;
    m_time = std::mktime(&tm);
}

std::string Time::str(const std::string& format) const
{
    std::ostringstream ss;
    std::lock_guard<std::mutex> lock(mutex);
    ss << std::put_time(std::gmtime(&m_time), format.c_str());
    return ss.str();
}

int64_t Time::operator-(const Time& other) const
{
    return std::difftime(m_time, other.m_time);
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

