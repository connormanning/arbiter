#pragma once

#include <string>

namespace arbiter
{

/** General utilities. */
namespace util
{
    /** Returns the portion of @p fullPath following the last instance of the
     * character `/`, if any instances exist aside from possibly the delimiter
     * `://`.  If there are no other instances of `/`, then @p fullPath itself
     * will be returned.
     *
     * If @p fullPath ends with a trailing `/` or a glob indication (i.e. is a
     * directory), these trailing characters will be stripped prior to the
     * logic above, thus the innermost directory in the full path will be
     * returned.
     */
    std::string getBasename(const std::string fullPath);

    /** @cond arbiter_internal */
    inline bool isSlash(char c) { return c == '/' || c == '\\'; }
    inline std::string joinImpl(bool first = false) { return std::string(); }

    template <typename ...Paths>
    inline std::string joinImpl(
            bool first,
            std::string current,
            Paths&&... paths)
    {
        std::string next(joinImpl(false, std::forward<Paths>(paths)...));
        while (next.size() && isSlash(next.front())) next = next.substr(1);

        if (first)
        {
            if (
                    current.size() > 1 &&
                    isSlash(current.back()) &&
                    !isSlash(current.at(current.size() - 2)))
            {
                current.pop_back();
            }
        }
        else
        {
            while (current.size() && isSlash(current.back()))
            {
                current.pop_back();
            }
            if (current.empty()) return next;
        }

        const std::string sep(
                next.size() && (current.empty() || !isSlash(current.back())) ?
                    "/" : "");

        return current + sep + next;
    }
    /** @endcond */

    /** @brief Join one or more path components "intelligently".
     *
     * The result is the concatenation of @p path and any members of @p paths
     * with exactly one slash preceding each non-empty portion of @p path or
     * @p paths.  Portions of @p paths will be stripped of leading slashes prior
     * to processing, so portions containing only slashes are considered empty.
     *
     * If @p path contains a single trailing slash preceded by a non-slash
     * character, then that slash will be stripped prior to processing.
     *
     * @code
     * join("")                                 // ""
     * join("/")                                // "/"
     * join("/var", "log", "arbiter.log")       // "/var/log/arbiter.log"
     * join("/var/", "log", "arbiter.log")      // "/var/log/arbiter.log"
     * join("", "var", "log", "arbiter.log")    // "/var/log/arbiter.log"
     * join("/", "/var", "log", "arbiter.log")  // "/var/log/arbiter.log"
     * join("", "/var", "log", "arbiter.log")   // "/var/log/arbiter.log"
     * join("~", "code", "", "test.cpp", "/")   // "~/code/test.cpp"
     * join("C:\\", "My Documents")             // "C:/My Documents"
     * join("s3://", "bucket", "object.txt")    // "s3://bucket/object.txt"
     * @endcode
     */
    template <typename ...Paths>
    inline std::string join(std::string path, Paths&&... paths)
    {
        return joinImpl(true, path, std::forward<Paths>(paths)...);
    }

} // namespace util

} // namespace arbiter

