#pragma once

#include <string>
#include <vector>

namespace arbiter
{

class Driver;

class Endpoint
{
    // Only Arbiter may construct.
    friend class Arbiter;

public:
    std::string root() const;
    std::string type() const;
    bool isRemote() const;

    std::string getSubpath(std::string subpath) const;
    std::vector<char> getSubpathBinary(std::string subpath) const;

    void putSubpath(std::string subpath, const std::string& data) const;
    void putSubpath(std::string subpath, const std::vector<char>& data) const;

    std::string fullPath(const std::string& subpath) const;

private:
    Endpoint(const Driver& driver, std::string root);

    const Driver& m_driver;
    std::string m_root;
};

} // namespace arbiter

