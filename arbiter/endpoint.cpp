#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/endpoint.hpp>

#include <arbiter/arbiter.hpp>
#include <arbiter/driver.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/sha256.hpp>
#include <arbiter/util/transforms.hpp>
#include <arbiter/util/util.hpp>
#endif
#include<fstream>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    std::string postfixSlash(std::string path)
    {
        if (path.empty()) throw ArbiterError("Invalid root path");
        if (path.back() != '/') path.push_back('/');
        return path;
    }
}

Endpoint::Endpoint(const Driver& driver, const std::string root)
    : m_driver(driver)
    , m_root(expandTilde(postfixSlash(root)))
{ }

std::string Endpoint::root() const
{
    return m_root;
}

std::string Endpoint::prefixedRoot() const
{
    return softPrefix() + root();
}

std::string Endpoint::type() const
{
    return m_driver.type();
}

bool Endpoint::isRemote() const
{
    return m_driver.isRemote();
}

bool Endpoint::isLocal() const
{
    return !isRemote();
}

bool Endpoint::isHttpDerived() const
{
    return tryGetHttpDriver() != nullptr;
}

std::unique_ptr<LocalHandle> Endpoint::getLocalHandle(
        const std::string subpath) const
{
    std::unique_ptr<LocalHandle> handle;

    if (isRemote())
    {
        const std::string tmp(getTempPath());
        const auto ext(Arbiter::getExtension(subpath));
        const std::string basename(
                                std::to_string(randomNumber()) +
                                (ext.size() ? "." + ext : ""));

        const std::string local(tmp + basename);
        if (isHttpDerived())
        {
            std::size_t fileSize = getSize(subpath);
            uint32_t chunkSize = 100 * 1000 * 1000; // 100mb
            uint32_t numChunks = ((fileSize % chunkSize) == 0)
                                     ? (fileSize / chunkSize)
                                     : (fileSize / chunkSize) + 1;
            int32_t start;
            std::ofstream stream(local, std::ofstream::binary |
                                        std::ofstream::out |
                                        std::ofstream::app);
            stream.good()
                ? start = -1
                : throw ArbiterError("Unable to create local handle.");
            while (numChunks > 0)
            {
                http::Headers headers;
                std::string range("bytes=" + std::to_string(start + 1) + "-" +
                                  std::to_string((start + chunkSize)>fileSize?fileSize:(start + chunkSize)));
                headers.insert(std::pair<std::string, std::string>("Range", range));
                std::vector<char> data =getBinary(subpath, headers, http::Query());
                stream.write(data.data(), data.size());
                stream.good()
                    ? start += chunkSize
                    : throw ArbiterError("Unable to write to local handle.");
                --numChunks;
            }
            stream.close();
        }
        else
        {
            drivers::Fs fs;
            fs.put(local, getBinary(subpath));
        }
        handle.reset(new LocalHandle(local, true));
    }
    else
    {
        handle.reset(new LocalHandle(expandTilde(fullPath(subpath)), false));
    }

    return handle;
}

std::string Endpoint::get(const std::string subpath) const
{
    return m_driver.get(fullPath(subpath));
}

std::unique_ptr<std::string> Endpoint::tryGet(const std::string subpath)
    const
{
    return m_driver.tryGet(fullPath(subpath));
}

std::vector<char> Endpoint::getBinary(const std::string subpath) const
{
    return m_driver.getBinary(fullPath(subpath));
}

std::unique_ptr<std::vector<char>> Endpoint::tryGetBinary(
        const std::string subpath) const
{
    return m_driver.tryGetBinary(fullPath(subpath));
}

std::size_t Endpoint::getSize(const std::string subpath) const
{
    return m_driver.getSize(fullPath(subpath));
}

std::unique_ptr<std::size_t> Endpoint::tryGetSize(
        const std::string subpath) const
{
    return m_driver.tryGetSize(fullPath(subpath));
}

void Endpoint::put(const std::string subpath, const std::string& data) const
{
    m_driver.put(fullPath(subpath), data);
}

void Endpoint::put(
        const std::string subpath,
        const std::vector<char>& data) const
{
    m_driver.put(fullPath(subpath), data);
}

std::string Endpoint::get(
        const std::string subpath,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver().get(fullPath(subpath), headers, query);
}

std::unique_ptr<std::string> Endpoint::tryGet(
        const std::string subpath,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver().tryGet(fullPath(subpath), headers, query);
}

std::vector<char> Endpoint::getBinary(
        const std::string subpath,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver().getBinary(fullPath(subpath), headers, query);
}

std::unique_ptr<std::vector<char>> Endpoint::tryGetBinary(
        const std::string subpath,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver().tryGetBinary(fullPath(subpath), headers, query);
}

void Endpoint::put(
        const std::string path,
        const std::string& data,
        const http::Headers headers,
        const http::Query query) const
{
    getHttpDriver().put(path, data, headers, query);
}

void Endpoint::put(
        const std::string path,
        const std::vector<char>& data,
        const http::Headers headers,
        const http::Query query) const
{
    getHttpDriver().put(path, data, headers, query);
}

http::Response Endpoint::httpGet(
        std::string path,
        http::Headers headers,
        http::Query query,
        const std::size_t reserve) const
{
    return getHttpDriver().internalGet(fullPath(path), headers, query, reserve);
}

http::Response Endpoint::httpPut(
        std::string path,
        const std::vector<char>& data,
        http::Headers headers,
        http::Query query) const
{
    return getHttpDriver().internalPut(fullPath(path), data, headers, query);
}

http::Response Endpoint::httpHead(
        std::string path,
        http::Headers headers,
        http::Query query) const
{
    return getHttpDriver().internalHead(fullPath(path), headers, query);
}

http::Response Endpoint::httpPost(
        std::string path,
        const std::vector<char>& data,
        http::Headers headers,
        http::Query query) const
{
    return getHttpDriver().internalPost(fullPath(path), data, headers, query);
}

std::string Endpoint::fullPath(const std::string& subpath) const
{
    return m_root + subpath;
}

std::string Endpoint::prefixedFullPath(const std::string& subpath) const
{
     return softPrefix() + fullPath(subpath);
}

Endpoint Endpoint::getSubEndpoint(std::string subpath) const
{
    return Endpoint(m_driver, m_root + subpath);
}

std::string Endpoint::softPrefix() const
{
    return isRemote() ? type() + "://" : "";
}

const drivers::Http* Endpoint::tryGetHttpDriver() const
{
    return dynamic_cast<const drivers::Http*>(&m_driver);
}

const drivers::Http& Endpoint::getHttpDriver() const
{
    if (auto d = tryGetHttpDriver()) return *d;
    else throw ArbiterError("Cannot get driver of type " + type() + " as HTTP");
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

