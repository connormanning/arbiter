#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace arbiter
{

class Driver
{
public:
    virtual ~Driver() { }

    // Return a unique string identifying this driver type.  Paths that begin
    // with the substring "<type>://" will be routed to this driver by the
    // Arbiter.
    virtual std::string type() const = 0;

    // Read/write data.
    std::unique_ptr<std::vector<char>> tryGetBinary(std::string path) const;
    std::vector<char> getBinary(std::string path) const;

    virtual void put(std::string path, const std::vector<char>& data) const = 0;

    // True for filesystem paths, otherwise false.  Derived classes other than
    // the filesystem driver should not overload.
    virtual bool isRemote() const { return true; }



    // Convenience overloads.
    std::unique_ptr<std::string> tryGet(std::string path) const;
    std::string get(std::string path) const;
    void put(std::string path, const std::string& data) const;

    // Resolve a possibly globbed path.
    std::vector<std::string> resolve(
            std::string path,
            bool verbose = false) const;

protected:
    // This operation expects a path ending with the characters "/*", and
    // without any type-specifying information (i.e. "http://", "s3://", or any
    // other "<type>://" information is stripped).
    virtual std::vector<std::string> glob(std::string path, bool verbose) const;
    virtual bool get(std::string path, std::vector<char>& data) const = 0;
};

typedef std::map<std::string, std::shared_ptr<Driver>> DriverMap;

} // namespace arbiter

