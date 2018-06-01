#pragma once

#include <cstdint>
#include <ctime>
#include <string>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#endif



#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

class ARBITER_DLL Time
{
public:
    static const std::string iso8601;
    static const std::string iso8601NoSeparators;
    static const std::string dateNoSeparators;

    Time();
    Time(const std::string& s, const std::string& format = "%Y-%m-%dT%H:%M:%SZ");

    std::string str(const std::string& format = "%Y-%m-%dT%H:%M:%SZ") const;

    // Return value is in seconds.
    int64_t operator-(const Time& other) const;
    int64_t asUnix() const;

private:
    std::time_t m_time;
};

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

