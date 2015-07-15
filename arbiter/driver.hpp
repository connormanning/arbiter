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
    virtual std::vector<char> get(std::string path) = 0;
    virtual void put(std::string path, const std::vector<char>& data) = 0;

    // True for filesystem paths, otherwise false.  Derived classes other than
    // the filesystem driver should not overload.
    virtual bool isRemote() const { return true; }



    // Convenience overloads.
    std::string getAsString(std::string path);
    void put(std::string path, const std::string& data);

    // Resolve a possibly globbed path.
    std::vector<std::string> resolve(std::string path, bool verbose = false);

private:
    // This operation expects a path ending with the characters "/*", and
    // without any type-specifying information (i.e. "http://", "s3://", or any
    // other "<type>://" information is stripped).
    virtual std::vector<std::string> glob(std::string path, bool verbose)
    {
        throw std::runtime_error("Cannot glob driver for: " + path);
    }
};

typedef std::map<std::string, std::shared_ptr<Driver>> DriverMap;

} // namespace arbiter

