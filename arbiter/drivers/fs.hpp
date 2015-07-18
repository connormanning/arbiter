#pragma once

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#endif

namespace arbiter
{

class FsDriver : public Driver
{
public:
    virtual std::string type() const { return "fs"; }
    virtual std::vector<char> getBinary(std::string path) const;
    virtual void put(std::string path, const std::vector<char>& data) const;

    virtual std::vector<std::string> glob(std::string path, bool verbose) const;

    virtual bool isRemote() const { return false; }
};

} // namespace arbiter

