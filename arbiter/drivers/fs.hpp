#pragma once

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#endif

namespace arbiter
{

namespace fs
{
    // Returns true if created, false if already existed.
    bool mkdirp(std::string dir);

    // Returns true if removed, otherwise false.
    bool remove(std::string filename);

    // Performs tilde expansion to a fully-qualified path, if possible.
    std::string expandTilde(std::string path);

    // RAII class for local temporary versions of remote files.  No-op if the
    // original file is already local.
    class LocalHandle
    {
    public:
        LocalHandle(std::string localPath, bool isRemote);
        ~LocalHandle();

        std::string localPath() const { return m_localPath; }

    private:
        const std::string m_localPath;
        const bool m_isRemote;
    };
}

class FsDriver : public Driver
{
public:
    virtual std::string type() const { return "fs"; }
    virtual void put(std::string path, const std::vector<char>& data) const;

    virtual std::vector<std::string> glob(std::string path, bool verbose) const;

    virtual bool isRemote() const { return false; }

protected:
    virtual bool get(std::string path, std::vector<char>& data) const;
};

} // namespace arbiter

