#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace arbiter
{

using Headers = std::map<std::string, std::string>;
using Query = std::map<std::string, std::string>;

class HttpPool;

/** @brief Base class for interacting with a storage type.
 *
 * A Driver handles reading, writing, and possibly globbing from a storage
 * source.  It is intended to be overriden by specialized subclasses for each
 * supported storage mechanism.
 *
 * Derived classes must override Driver::type,
 * Driver::put(std::string, const std::vector<char>&) const, and
 * Driver::get(std::string, std::vector<char>&) const,
 * Driver::size(std::string) const - and may optionally
 * override Driver::glob if possible.
 */
class Driver
{
public:
    virtual ~Driver() { }

    /**
     * Returns a string identifying this driver type, which should be unique
     * among all other drivers.  Paths that begin with the substring
     * `<type>://` will be routed to this driver.  For example, `fs`, `s3`, or
     * `http`.
     *
     * @note Derived classes must override.
     */
    virtual std::string type() const = 0;

    /** Get binary data, if available. */
    std::unique_ptr<std::vector<char>> tryGetBinary(std::string path) const;

    /** Get binary data. */
    std::vector<char> getBinary(std::string path) const;

    /**
     * Write @p data to the given @p path.
     *
     * @note Derived classes must override.
     *
     * @param path Path with the type-specifying prefix information stripped.
     */
    virtual void put(std::string path, const std::vector<char>& data) const = 0;

    /** True for remote paths, otherwise false.  If `true`, a fs::LocalHandle
     * request will download and write this file to the local filesystem.
     */
    virtual bool isRemote() const { return true; }

    /** Get string data, if available. */
    std::unique_ptr<std::string> tryGet(std::string path) const;

    /** Get string data. */
    std::string get(std::string path) const;

    /** Get the file size in bytes, if available. */
    virtual std::unique_ptr<std::size_t> tryGetSize(std::string path) const = 0;

    /** Get the file size in bytes, or throw if it does not exist. */
    std::size_t getSize(std::string path) const;

    /** Write string data. */
    void put(std::string path, const std::string& data) const;

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
     * matching the given wildcard @p path (no directories).
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
};

typedef std::map<std::string, std::unique_ptr<Driver>> DriverMap;

} // namespace arbiter

