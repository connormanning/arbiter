#pragma once

#include <string>
#include <vector>
#include <memory>

#ifndef ARBITER_IS_AMALGAMATION

#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/exports.hpp>
#include <arbiter/util/types.hpp>

#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace drivers { class Http; }
namespace http { class Pool; }

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
class ARBITER_DLL Endpoint
{
    // Only Arbiter may construct.
    friend class Arbiter;

public:
    /** Returns root directory name without any type-prefixing, and will
     * always end with the character `/`.  For example `~/data/`, or
     * `my-bucket/nested-directory/`.
     */
    std::string root() const;

    /** Returns root directory name ending with the character `/`.  If
     * `isRemote` is `true`, then the path will be prefixed with
     * `type() + "://"`.
     */
    std::string prefixedRoot() const;

    // Driver passthroughs.

    /** Passthrough to Driver::type. */
    std::string type() const;

    /** Passthrough to Driver::isRemote. */
    bool isRemote() const;

    /** Negation of Endpoint::isRemote. */
    bool isLocal() const;

    /** See Arbiter::isHttpDerived. */
    bool isHttpDerived() const;

    /** See Arbiter::getLocalHandle. */
    LocalHandle getLocalHandle(
            std::string subpath,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

    /** Passthrough to Driver::get. */
    std::string get(std::string subpath) const;

    /** Passthrough to Driver::tryGet. */
    std::unique_ptr<std::string> tryGet(std::string subpath) const;

    /** Passthrough to Driver::getBinary. */
    std::vector<char> getBinary(std::string subpath) const;

    /** Passthrough to Driver::tryGetBinary. */
    std::unique_ptr<std::vector<char>> tryGetBinary(std::string subpath) const;

    /** Passthrough to Driver::getSize. */
    std::size_t getSize(std::string subpath) const;

    /** Passthrough to Driver::tryGetSize. */
    std::unique_ptr<std::size_t> tryGetSize(std::string subpath) const;

    /** Passthrough to Driver::put(std::string, const std::string&) const. */
    void put(std::string subpath, const std::string& data) const;

    /** Passthrough to
     * Driver::put(std::string, const std::vector<char>&) const.
     */
    void put(std::string subpath, const std::vector<char>& data) const;

    // HTTP-specific passthroughs.

    /** Passthrough to
     * drivers::Http::get(std::string, http::Headers, http::Query) const. */
    std::string get(
            std::string subpath,
            http::Headers headers,
            http::Query = http::Query()) const;

    /** Passthrough to
     * drivers::Http::tryGet(std::string, http::Headers, http::Query) const. */
    std::unique_ptr<std::string> tryGet(
            std::string subpath,
            http::Headers headers,
            http::Query = http::Query()) const;

    /** Passthrough to
     * drivers::Http::getBinary(std::string, http::Headers, http::Query) const.
     */
    std::vector<char> getBinary(
            std::string path,
            http::Headers headers,
            http::Query query = http::Query()) const;

    /** Passthrough to
     * drivers::Http::tryGetBinary(std::string, http::Headers, http::Query) const.
     */
    std::unique_ptr<std::vector<char>> tryGetBinary(
            std::string path,
            http::Headers headers,
            http::Query query = http::Query()) const;

    /** Passthrough to
     * drivers::Http::getSize(std::string, http::Headers, http::Query) const.
     */
    std::size_t getSize(
            std::string subpath,
            http::Headers headers,
            http::Query = http::Query()) const;

    /** Passthrough to
     * drivers::Http::tryGetSize(std::string, http::Headers, http::Query) const.
     */
    std::unique_ptr<std::size_t> tryGetSize(
            std::string subpath,
            http::Headers headers,
            http::Query = http::Query()) const;

    /** Passthrough to
     * drivers::Http::put(std::string, const std::string&, http::Headers, http::Query) const.
     */
    void put(
            std::string path,
            const std::string& data,
            http::Headers headers,
            http::Query query = http::Query()) const;

    /** Passthrough to
     * drivers::Http::put(std::string, const std::vector<char>&, http::Headers, http::Query) const.
     */
    void put(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers,
            http::Query query = http::Query()) const;

    // Endpoint specifics.

    /** Get the full path corresponding to this subpath.  The path will not
     * be prefixed with the driver type or the `://` delimiter.
     */
    std::string fullPath(const std::string& subpath) const;

    /** Get the full path corresponding to this subpath.  If `isRemote` is
     * `true`, then the path will be prefixed with `type() + "://"`.
     */
    std::string prefixedFullPath(const std::string& subpath) const;

    /** Get a further nested subpath relative to this Endpoint's root. */
    Endpoint getSubEndpoint(std::string subpath) const;

    http::Response httpGet(
            std::string path,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query(),
            std::size_t reserve = 0) const;

    http::Response httpPut(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

    http::Response httpHead(
            std::string path,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

    http::Response httpPost(
            std::string path,
            const std::vector<char>& data,
            http::Headers headers = http::Headers(),
            http::Query query = http::Query()) const;

private:
    Endpoint(const Driver& driver, std::string root);

    // If `isRemote()`, returns the type and delimiter, otherwise returns an
    // empty string.
    std::string softPrefix() const;

    const drivers::Http* tryGetHttpDriver() const;
    const drivers::Http& getHttpDriver() const;

    const Driver* m_driver;
    std::string m_root;
};

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

