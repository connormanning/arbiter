#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{
namespace http { class Pool; }


/** @brief Base class for interacting with a storage type.
 *
 * A Driver handles reading, writing, and possibly globbing from a storage
 * source.  It is intended to be overriden by specialized subclasses for each
 * supported storage mechanism.
 *
 * Derived classes must override Driver::type,
 * Driver::put(std::string, const std::vector<char>&) const, and
 * Driver::get(std::string, std::vector<char>&) const,
 * Driver::getSize(std::string) const - and may optionally
 * override Driver::glob if possible.
 *
 * HTTP-derived classes should override the PUT and GET versions that accept
 * http::Headers and http::Query parameters instead.
 */
class ARBITER_DLL Driver
{
public:
    Driver(std::string protocol, std::string profile = "")
        : m_profile(profile)
        , m_protocol(protocol)
    { }

    virtual ~Driver() { }

    static std::shared_ptr<Driver> create(
        http::Pool& pool,
        std::string protocol,
        std::string config);

    std::string profile() const { return m_profile; }
    std::string protocol() const { return m_protocol; }
    std::string profiledProtocol() const
    {
        return m_profile.size() ? m_profile + "@" + m_protocol : m_protocol;
    }

    /** Get string data. */
    std::string get(std::string path) const;

    /** Get string data, if available. */
    std::unique_ptr<std::string> tryGet(std::string path) const;

    /** Get binary data. */
    std::vector<char> getBinary(std::string path) const;

    /** Get binary data, if available. */
    std::unique_ptr<std::vector<char>> tryGetBinary(std::string path) const;

    /**
     * Write @p data to the given @p path.
     *
     * @note Derived classes must override.
     *
     * @param path Path with the type-specifying prefix information stripped.
     */
    virtual std::vector<char> put(
        std::string path, 
        const std::vector<char>& data) const = 0;

    /** True for remote paths, otherwise false.  If `true`, a fs::LocalHandle
     * request will download and write this file to the local filesystem.
     */
    virtual bool isRemote() const { return true; }

    /** Get the file size in bytes, if available. */
    virtual std::unique_ptr<std::size_t> tryGetSize(std::string path) const = 0;

    /** Get the file size in bytes, or throw if it does not exist. */
    std::size_t getSize(std::string path) const;

    /** Write string data. */
    std::vector<char> put(std::string path, const std::string& data) const;

    /** Copy a file, where @p src and @p dst must both be of this driver
     * type.  Type-prefixes must be stripped from the input parameters.
     */
    virtual void copy(std::string src, std::string dst) const;

    /** @brief Resolve a possibly globbed path.
     *
     * See Arbiter::resolve for details.
     */
    std::vector<std::string> resolve(
            std::string path,
            bool verbose = false) const;

protected:
    /** @brief Resolve a wildcard path.
     *
     * This operation should return a non-recursive resolution of the files
     * matching the given wildcard @p path (no directories).  With the exception
     * of the filesystem Driver, results should be prefixed with
     * `type() + "://"`.
     *
     * @note The default behavior is to throw ArbiterError, so derived classes
     * may optionally override if they can perform this behavior.
     *
     * @param path A string ending with the character `*`, and with all
     * type-specifying prefix information like `http://` or `s3://` stripped.
     *
     * @param verbose If true, this function may print out minimal information
     * to indicate that progress is occurring.
     */
    virtual std::vector<std::string> glob(std::string path, bool verbose) const;

    /**
     * @param path Path with the type-specifying prefix information stripped.
     * @param[out] data Empty vector in which to write resulting data.
     */
    virtual bool get(std::string path, std::vector<char>& data) const = 0;

    const std::string m_profile;
    const std::string m_protocol;
};

typedef std::map<std::string, std::shared_ptr<Driver>> DriverMap;

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

