#pragma once

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#include <arbiter/third/json/json.hpp>
#endif

namespace arbiter
{

class Arbiter;

namespace fs
{
    // Returns true if created, false if already existed.
    bool mkdirp(std::string dir);

    // Returns true if removed, otherwise false.
    bool remove(std::string filename);

    // Performs tilde expansion to a fully-qualified path, if possible.
    std::string expandTilde(std::string path);

    /** @brief A scoped local filehandle for a possibly remote path.
     *
     * This is an RAII style pseudo-filehandle.  It manages the scope of a
     * local temporary version of a file, where that file may have been copied
     * from a remote storage location.
     *
     * See Arbiter::getLocalHandle for details about construction.
     */
    class LocalHandle
    {
        friend class arbiter::Arbiter;

    public:
        /** @brief Deletes the local path if the data was copied from a remote
         * source.
         *
         * This is a no-op if the path was already local and not copied.
         */
        ~LocalHandle();

        /** @brief Get the path of the locally stored file.
         *
         * @return A local filesystem absolute path containing the data
         * requested in Arbiter::getLocalHandle.
         */
        std::string localPath() const { return m_localPath; }

    private:
        LocalHandle(std::string localPath, bool isRemote);

        const std::string m_localPath;
        const bool m_isRemote;
    };
}

namespace drivers
{

/** @brief Local filesystem driver. */
class Fs : public Driver
{
public:
    static std::unique_ptr<Fs> create(HttpPool& pool, const Json::Value& json);

    virtual std::string type() const override { return "fs"; }
    virtual void put(
            std::string path,
            const std::vector<char>& data) const override;

    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    virtual bool isRemote() const override { return false; }

protected:
    virtual bool get(std::string path, std::vector<char>& data) const override;
};

} // namespace drivers

} // namespace arbiter

