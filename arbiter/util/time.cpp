#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/time.hpp>
#endif

#include <ctime>
#include <iomanip>
#include <iostream>
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
#ifndef ARBITER_WINDOWS
    // We'd prefer to use get_time, but it has poor compiler support.
    if (!strptime(s.c_str(), format.c_str(), &tm))
    {
        throw ArbiterError("Failed to parse " + s + " as time: " + format);
    }
#else
    std::istringstream ss(s);
    ss >> std::get_time(&tm, format.c_str());
    if (ss.fail())
    {
        throw ArbiterError("Failed to parse " + s + " as time: " + format);
    }
#endif
    tm.tm_sec -= utcOffset;
    m_time = std::mktime(&tm);
}

std::string Time::str(const std::string& format) const
{
    std::lock_guard<std::mutex> lock(mutex);
#ifndef ARBITER_WINDOWS
    // We'd prefer to use put_time, but it has poor compiler support.
    // We're already locked here for gmtime, so might as well make this static.
    static std::vector<char> s(256, 0);

    const std::size_t size =
        strftime(s.data(), s.size(), format.c_str(), std::gmtime(&m_time));

    return std::string(s.data(), s.data() + size);
#else
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&m_time), format.c_str());
    return ss.str();
#endif
}

int64_t Time::operator-(const Time& other) const
{
    return std::difftime(m_time, other.m_time);
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

