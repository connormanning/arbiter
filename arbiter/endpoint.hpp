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

    std::string getSubpath(std::string subpath);
    std::vector<char> getSubpathBinary(std::string subpath);

    void putSubpath(std::string subpath, const std::string& data);
    void putSubpath(std::string subpath, const std::vector<char>& data);

    std::string fullPath(const std::string& subpath) const;

private:
    Endpoint(Driver& driver, std::string root);

    Driver& m_driver;
    std::string m_root;
};

} // namespace arbiter

