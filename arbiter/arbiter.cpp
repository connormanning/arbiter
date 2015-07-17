#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>

#include <arbiter/driver.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/drivers/s3.hpp>
#endif

namespace arbiter
{

namespace
{
    const std::string delimiter("://");
}

Arbiter::Arbiter(AwsAuth* awsAuth)
    : m_drivers()
    , m_pool(32, 5)
{
    m_drivers["fs"] = std::make_shared<FsDriver>(FsDriver());
    m_drivers["http"] = std::make_shared<HttpDriver>(HttpDriver(m_pool));

    if (awsAuth)
    {
        m_drivers["s3"] =
            std::make_shared<S3Driver>(S3Driver(m_pool, *awsAuth));
    }
}

Arbiter::~Arbiter()
{ }

std::vector<char> Arbiter::get(const std::string path) const
{
    return getDriver(path).get(stripType(path));
}

std::string Arbiter::getAsString(const std::string path) const
{
    return getDriver(path).getAsString(stripType(path));
}

void Arbiter::put(const std::string path, const std::vector<char>& data)
{
    return getDriver(path).put(stripType(path), data);
}

void Arbiter::put(const std::string path, const std::string& data)
{
    return getDriver(path).put(stripType(path), data);
}

bool Arbiter::isRemote(const std::string path) const
{
    return getDriver(path).isRemote();
}

std::vector<std::string> Arbiter::resolve(
        const std::string path,
        const bool verbose) const
{
    return getDriver(path).resolve(path, verbose);
}

Driver& Arbiter::getDriver(const std::string path) const
{
    return *m_drivers.at(parseType(path));
}

std::string Arbiter::parseType(const std::string path) const
{
    std::string type("fs");
    const std::size_t pos(path.find(delimiter));

    if (pos != std::string::npos)
    {
        type = path.substr(0, pos);
    }

    return type;
}

std::string Arbiter::stripType(const std::string raw)
{
    std::string result(raw);
    const std::size_t pos(raw.find(delimiter));

    if (pos != std::string::npos)
    {
        result = raw.substr(pos + delimiter.size());
    }

    return result;
}

} // namespace arbiter

