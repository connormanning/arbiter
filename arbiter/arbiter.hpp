#pragma once

#include <vector>
#include <string>

#if defined(_WIN32) || defined(WIN32) || defined(_MSC_VER)
#define ARBITER_WINDOWS
#endif

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#include <arbiter/endpoint.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/drivers/http.hpp>
#include <arbiter/drivers/s3.hpp>
#include <arbiter/drivers/dropbox.hpp>
#include <arbiter/third/json/json.hpp>
#endif

namespace arbiter
{

/** @brief Exception class for all internally thrown runtime errors. */
class ArbiterError : public std::runtime_error
{
public:
    ArbiterError(std::string msg) : std::runtime_error(msg) { }
};

/** @brief The primary interface for storage abstraction.
 *
 * The Arbiter is the primary layer of abstraction for all supported Driver
 * instances, allowing remote paths to be treated as a simple filesystem or
 * key-value store.  Routing to specialized drivers is based on a substring
 * comparison of an incoming path's delimited type, for example
 * `http://arbiter.io/data.txt` would route to the HTTP driver and
 * `s3://my-bucket/data.txt` would route to the S3 driver.  Paths containing
 * no prefixed type information are routed to the filesystem driver.
 *
 * All Arbiter operations are thread-safe except unless otherwise noted.
 */
class Arbiter
{
public:
    /** Construct a basic Arbiter with only drivers the don't require
     * external configuration parameters.
     */
    Arbiter();

    /** @brief Construct an Arbiter with driver configurations. */
    Arbiter(const Json::Value& json);

    /** @brief Add a custom driver for the supplied type.
     *
     * After this operation completes, future requests into arbiter beginning
     * with the prefix @p type followed by the delimiter `://` will be routed
     * to the supplied @p driver.  If a Driver of type @p type already exists,
     * the supplied @p driver will replace it.
     *
     * This operation will throw ArbiterError if @p driver is empty.
     *
     * @note This operation is not thread-safe.
     */
    void addDriver(std::string type, std::unique_ptr<Driver> driver);

    /** Get data or throw if inaccessible. */
    std::string get(std::string path) const;

    /** Get data if accessible. */
    std::unique_ptr<std::string> tryGet(std::string path) const;

    /** Get data in binary form or throw if inaccessible. */
    std::vector<char> getBinary(std::string path) const;

    /** Get data in binary form if accessible. */
    std::unique_ptr<std::vector<char>> tryGetBinary(std::string path) const;

    /** Write data to path. */
    void put(std::string path, const std::string& data) const;

    /** Write data to path. */
    void put(std::string path, const std::vector<char>& data) const;

    /** Returns true if this path is a remote path, or false if it is on the
     * local filesystem.
     */
    bool isRemote(std::string path) const;

    /** @brief Resolve a possibly globbed path.
     *
     * If @p path ends with `*`, then this operation will attempt to
     * non-recursively glob all files matching the given wildcard.  Directories
     * matching the wildcard and their contents are not returned.
     *
     * Examples:
     * @code
     * // Returns all files in `~/data`.  A directory `~/data/dir/` and its
     * // contents would not be returned since this is non-recursive.
     * a.resolve("~\data\*");
     *
     * // Returns all files matching `prefix-*` in bucket `my-bucket`.  An
     * // object `s3://my-bucket/prefix-123/hello.txt` would not be returned,
     * // but object `s3://my-bucket/prefix-456.txt` would.
     * a.resolve("s3://my-bucket/prefix-*");
     * @endcode
     *
     * @note Throws ArbiterError if the selected driver does not support
     * globbing, for example the HTTP driver.
     *
     * @param path Path to search, may be a directory or file.  Globbing
     * will only occur in the case of a directory.
     *
     * @param verbose If true, driver-specific status information may be
     * printed to STDOUT during globbing.
     *
     * @return  If @p path ends with `*`, the results are the contents of the
     * resulting non-recursive resolution of this path.  Otherwise, the results
     * are a vector of size one containing only @p path itself, unaltered.
     */
    std::vector<std::string> resolve(
            std::string path,
            bool verbose = false) const;

    /** @brief Get a reusable Endpoint for this root directory. */
    Endpoint getEndpoint(std::string root) const;

    /** Returns the Driver, if one can be found, for the given @p path.  The
     * driver type is determined from the @p path substring prior to the
     * delimiter `://`.  If this delimiter does not exist, then the filesystem
     * driver is returned.  If the delimiter exists but a corresponding driver
     * type is not found, ArbiterError is thrown.
     *
     * Optionally, filesystem paths may be explicitly prefixed with `fs://`.
     */
    const Driver& getDriver(std::string path) const;

    /** @brief Get a fs::LocalHandle to a possibly remote file.
     *
     * If @p path is remote (see Arbiter::isRemote), this operation will fetch
     * the file contents and write them to the local filesystem in the
     * directory represented by @p tmpEndpoint.  The contents of @p path are
     * not copied if @p path is already local.
     *
     * There are no filename guarantees if @p path is remote.  Use
     * LocalHandle::localPath to determine this.
     *
     * @note This operation will throw an ArbiterError if the path is remote
     * and @p tmpEndpoint is also remote.
     *
     * @param path Possibly remote path to fetch.
     *
     * @param tempEndpoint If @path is remote, the local copy will be created
     * at this Endpoint.
     *
     * @return A fs::LocalHandle for local access to the resulting file.
     */
    std::unique_ptr<fs::LocalHandle> getLocalHandle(
            std::string path,
            const Endpoint& tempEndpoint) const;

    /** Strip the type and delimiter `://`, if they exist. */
    static std::string stripType(std::string path);

    /** Fetch the common HTTP pool, which may be useful when dynamically
     * constructing adding a Driver via Arbiter::addDriver.
     */
    HttpPool& httpPool() { return m_pool; }

    /** If no delimiter of "://" is found, returns "fs".  Otherwise, returns
     * the substring prior to but not including this delimiter.
     */
    std::string getType(const std::string path) const;

private:
    // Registers all available default Driver instances.
    void init(const Json::Value& json);

    DriverMap m_drivers;
    HttpPool m_pool;
};

} // namespace arbiter

