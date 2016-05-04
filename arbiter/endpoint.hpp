#pragma once

#include <string>
#include <vector>
#include <memory>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

class Driver;

/** @brief A utility class to drive usage from a common root directory.
 *
 * Acts as a reusable Driver based on a single root directory.  The interface
 * is the same as a Driver, although @p path parameters represent subpaths
 * which will be appended to the value from Endpoint::root to form a full
 * path.
 *
 * An Endpoint may be created using Arbiter::getEndpoint.
 */
class Endpoint
{
    // Only Arbiter may construct.
    friend class Arbiter;

public:
    /** Returns root directory name without any type-prefixing, and will
     * always end with the character `/`.  For example `~/data/`, or
     * `my-bucket/nested-directory/`.
     */
    std::string root() const;

    /** Passthrough to Driver::type. */
    std::string type() const;

    /** Passthrough to Driver::isRemote. */
    bool isRemote() const;

    /** Negation of Endpoint::isRemote. */
    bool isLocal() const;

    /** Passthrough to Driver::get. */
    std::string getSubpath(std::string subpath) const;

    /** Passthrough to Driver::tryGet. */
    std::unique_ptr<std::string> tryGetSubpath(std::string subpath) const;

    /** Passthrough to Driver::getBinary. */
    std::vector<char> getSubpathBinary(std::string subpath) const;

    /** Passthrough to Driver::tryGetBinary. */
    std::unique_ptr<std::vector<char>> tryGetSubpathBinary(
            std::string subpath) const;

    /** Passthrough to Driver::put(std::string, const std::string&) const. */
    void putSubpath(std::string subpath, const std::string& data) const;

    /** Passthrough to
     * Driver::put(std::string, const std::vector<char>&) const.
     */
    void putSubpath(std::string subpath, const std::vector<char>& data) const;

    /** Get the full path corresponding to this subpath.  The path will not
     * be prefixed with the driver type or the `://` delimiter.
     */
    std::string fullPath(const std::string& subpath) const;

    /** Get a further nested subpath relative to this Endpoint's root. */
    Endpoint getSubEndpoint(std::string subpath) const;

private:
    Endpoint(const Driver& driver, std::string root);

    const Driver& m_driver;
    std::string m_root;
};

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

