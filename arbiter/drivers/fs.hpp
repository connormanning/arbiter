#pragma once

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>

#ifndef ARBITER_EXTERNAL_JSON
#include <arbiter/third/json/json.hpp>
#endif

#endif


#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/exports.hpp>
#endif

#ifdef ARBITER_EXTERNAL_JSON
#include <json/json.h>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

class Arbiter;

/**
 * \addtogroup fs
 * @{
 */

/** Filesystem utilities. */
namespace fs
{
    /** @brief Returns true if created, false if already existed. */
    ARBITER_DLL bool mkdirp(std::string dir);

    /** @brief Returns true if removed, otherwise false. */
    ARBITER_DLL bool remove(std::string filename);

    /** @brief Performs tilde expansion to a fully-qualified path, if possible.
     */
    ARBITER_DLL std::string expandTilde(std::string path);

    /** @brief Get temporary path from environment. */
    ARBITER_DLL std::string getTempPath();

    /** @brief Resolve a possible wildcard path. */
    ARBITER_DLL std::vector<std::string> glob(std::string path);

    /** @brief A scoped local filehandle for a possibly remote path.
     *
     * This is an RAII style pseudo-filehandle.  It manages the scope of a
     * local temporary version of a file, where that file may have been copied
     * from a remote storage location.
     *
     * See Arbiter::getLocalHandle for details about construction.
     */
    class ARBITER_DLL LocalHandle
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

        /** @brief Release the managed local path and return the path from
         * LocalHandle::localPath.
         *
         * After this call, destruction of the LocalHandle will not erase the
         * temporary file that may have been created.
         */
        std::string release()
        {
            m_erase = false;
            return localPath();
        }

    private:
        LocalHandle(std::string localPath, bool isRemote);

        const std::string m_localPath;
        bool m_erase;
    };
}
/** @} */

namespace drivers
{

/** @brief Local filesystem driver. */
class ARBITER_DLL Fs : public Driver
{
public:
    Fs() { }

    using Driver::get;

    static std::unique_ptr<Fs> create(const Json::Value& json);

    virtual std::string type() const override { return "file"; }

    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    virtual void put(
            std::string path,
            const std::vector<char>& data) const override;

    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    virtual bool isRemote() const override { return false; }

    virtual void copy(std::string src, std::string dst) const override;

protected:
    virtual bool get(std::string path, std::vector<char>& data) const override;
};

} // namespace drivers

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

