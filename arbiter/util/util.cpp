#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/util.hpp>

#include <arbiter/arbiter.hpp>
#endif

namespace arbiter
{
namespace util
{

std::string getBasename(const std::string fullPath)
{
    std::string result(fullPath);

    std::string stripped(Arbiter::stripType(fullPath));

    for (std::size_t i(0); i < 2; ++i)
    {
        // Pop trailing asterisk, or double-trailing-asterisks for both non- and
        // recursive globs.
        if (!stripped.empty() && stripped.back() == '*') stripped.pop_back();
    }

    // Pop trailing slash, in which case the result is the innermost directory.
    while (!stripped.empty() && isSlash(stripped.back())) stripped.pop_back();

    // Now do the real slash searching.
    const std::size_t pos(stripped.rfind('/'));

    if (pos != std::string::npos)
    {
        const std::string sub(stripped.substr(pos + 1));
        if (!sub.empty()) result = sub;
    }

    return result;
}

} // namespace util
} // namespace arbiter
