#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace arbiter
{

class Time
{
public:
    static const std::string iso8601;
    static const std::string iso8601NoSeparators;
    static const std::string dateNoSeparators;

    Time();
    Time(const std::string& s, const std::string& format = iso8601);

    std::string str(const std::string& format = iso8601) const;

    // Return value is in seconds.
    int64_t operator-(const Time& other) const;

private:
    std::time_t m_time;
};

} // namespace arbiter

