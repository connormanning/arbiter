#pragma once

#include <vector>
#include <string>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#include <arbiter/endpoint.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/drivers/http.hpp>
#include <arbiter/drivers/s3.hpp>
#endif

namespace arbiter
{

class Arbiter
{
public:
    Arbiter(AwsAuth* awsAuth = 0);
    ~Arbiter();

    // Read/write operations.  Each may throw std::runtime_error if the
    // path is inacessible for the requested operation.
    std::string get(std::string path) const;
    std::vector<char> getBinary(std::string path) const;

    void put(std::string path, const std::string& data);
    void put(std::string path, const std::vector<char>& data);

    // Returns true if this path is a filesystem path, otherwise false.
    bool isRemote(std::string path) const;

    // If a path ends with "/*", this operation will attempt to glob the
    // preceding directory, returning all resolved paths.  Otherwise a vector
    // of size one containing only _path_, unaltered, is returned.
    //
    // Globbed resolution is non-recursive.
    //
    // Will throw std::runtime_error if the selected driver does not support
    // globbing (e.g. HTTP).
    //
    // If _verbose_ is true, the driver may print status information during
    // the globbing process.
    std::vector<std::string> resolve(
            std::string path,
            bool verbose = false) const;

    // If many subpaths from a common root will be addressed, an Endpoint may
    // be used to simplify paths.
    Endpoint getEndpoint(std::string root) const;

    // The substring prior to the delimiter "://" denotes the driver type, if
    // this delimiter exists - otherwise the path is assumed to refer to the
    // local filesystem, which is guaranteed to have a driver.
    //
    // Will throw std::out_of_range if the delimiter exists but a driver for
    // its type does not exist.
    Driver& getDriver(std::string path) const;

    static std::string stripType(const std::string path);

private:
    // If no delimiter of "://" is found, returns "fs".  Otherwise, returns
    // the substring prior to but not including this delimiter.
    std::string parseType(const std::string path) const;

    DriverMap m_drivers;
    HttpPool m_pool;
};

} // namespace arbiter

