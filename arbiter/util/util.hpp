#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

/** General utilities. */
namespace util
{
    /** Returns @p path, less any trailing glob indicators (one or two
     * asterisks) as well as any possible trailing slash.
     */
    std::string stripPostfixing(std::string path);

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
    std::string getBasename(std::string fullPath);

    /** Returns everything besides the basename, as determined by `getBasename`.
     * For file paths, this corresponds to the directory path above the file.
     * For directory paths, this corresponds to all directories above the
     * innermost directory.
     */
    std::string getNonBasename(std::string fullPath);

    /** @cond arbiter_internal */
    inline bool isSlash(char c) { return c == '/' || c == '\\'; }

    /** Returns true if the last character is an asterisk. */
    inline bool isGlob(std::string path)
    {
        return path.size() && path.back() == '*';
    }

    /** Returns true if the last character is a slash or an asterisk. */
    inline bool isDirectory(std::string path)
    {
        return (path.size() && isSlash(path.back())) || isGlob(path);
    }

    inline std::string joinImpl(bool first = false) { return std::string(); }

    template <typename ...Paths>
    inline std::string joinImpl(
            bool first,
            std::string current,
            Paths&&... paths)
    {
        const bool currentIsDir(current.size() && isSlash(current.back()));
        std::string next(joinImpl(false, std::forward<Paths>(paths)...));

        // Strip slashes from the front of our remainder.
        while (next.size() && isSlash(next.front())) next = next.substr(1);

        if (first)
        {
            // If this is the first component, strip a single trailing slash if
            // one exists - but do not strip a double trailing slash since we
            // want to retain Windows paths like "C://".
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

        std::string sep;

        if (next.size() && (current.empty() || !isSlash(current.back())))
        {
            // We are going to join current with a populated subpath, so make
            // sure they are separated by a slash.
            sep = "/";
        }
        else if (next.empty() && currentIsDir)
        {
            // We are at the end of the chain, and the last component was a
            // directory.  Retain its trailing slash.
            if (current.size() && !isSlash(current.back()))
            {
                sep = "/";
            }
        }

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

    /** @brief Extract an environment variable, if it exists, independent of
     * platform.
     */
    std::unique_ptr<std::string> env(const std::string& var);

    /** @brief Split a string on a token. */
    std::vector<std::string> split(const std::string& s, char delimiter = '\n');

    /** @brief Remove whitespace. */
    std::string stripWhitespace(const std::string& s);

    template<typename T, typename... Args>
    std::unique_ptr<T> makeUnique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    template<typename T>
    std::unique_ptr<T> clone(const T& t)
    {
        return makeUnique<T>(t);
    }

    template<typename T>
    std::unique_ptr<T> maybeClone(const T* t)
    {
        if (t) return makeUnique<T>(*t);
        else return std::unique_ptr<T>();
    }
} // namespace util

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

