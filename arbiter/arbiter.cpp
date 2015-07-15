#include <arbiter/arbiter.hpp>

#include <arbiter/driver.hpp>
#include <arbiter/drivers/fs.hpp>

namespace arbiter
{

namespace
{
    const std::string delimiter("://");
}

Arbiter::Arbiter(DriverMap drivers)
    : m_drivers{{ "fs", std::make_shared<FsDriver>(FsDriver()) }}
{
    m_drivers.insert(drivers.begin(), drivers.end());
}

Arbiter::~Arbiter()
{ }

void Arbiter::add(std::shared_ptr<Driver> driver)
{
    m_drivers[driver->type()] = driver;
}

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
    return getDriver(path).put(path, data);
}

void Arbiter::put(const std::string path, const std::string& data)
{
    return getDriver(path).put(path, data);
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

