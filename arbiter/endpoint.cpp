#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/endpoint.hpp>

#include <arbiter/arbiter.hpp>
#include <arbiter/driver.hpp>
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
    , m_root(fs::expandTilde(postfixSlash(root)))
{ }

std::string Endpoint::root() const
{
    return m_root;
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

std::string Endpoint::getSubpath(const std::string subpath) const
{
    return m_driver.get(fullPath(subpath));
}

std::unique_ptr<std::string> Endpoint::tryGetSubpath(const std::string subpath)
    const
{
    return m_driver.tryGet(fullPath(subpath));
}

std::vector<char> Endpoint::getSubpathBinary(const std::string subpath) const
{
    return m_driver.getBinary(fullPath(subpath));
}

std::unique_ptr<std::vector<char>> Endpoint::tryGetSubpathBinary(
        const std::string subpath) const
{
    return m_driver.tryGetBinary(fullPath(subpath));
}

void Endpoint::putSubpath(
        const std::string subpath,
        const std::string& data) const
{
    m_driver.put(fullPath(subpath), data);
}

void Endpoint::putSubpath(
        const std::string subpath,
        const std::vector<char>& data) const
{
    m_driver.put(fullPath(subpath), data);
}

std::string Endpoint::fullPath(const std::string& subpath) const
{
    return m_root + subpath;
}

Endpoint Endpoint::getSubEndpoint(std::string subpath) const
{
    return Endpoint(m_driver, m_root + subpath);
}

} // namespace arbiter

