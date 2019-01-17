#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>

#include <arbiter/arbiter.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

std::string Driver::get(const std::string path) const
{
    const std::vector<char> data(getBinary(path));
    return std::string(data.begin(), data.end());
}

std::unique_ptr<std::string> Driver::tryGet(const std::string path) const
{
    std::unique_ptr<std::string> result;
    std::unique_ptr<std::vector<char>> data(tryGetBinary(path));
    if (data) result.reset(new std::string(data->begin(), data->end()));
    return result;
}

std::vector<char> Driver::getBinary(std::string path) const
{
    std::vector<char> data;
    if (!get(path, data)) throw ArbiterError("Could not read file " + path);
    return data;
}

std::unique_ptr<std::vector<char>> Driver::tryGetBinary(std::string path) const
{
    std::unique_ptr<std::vector<char>> data(new std::vector<char>());
    if (!get(path, *data)) data.reset();
    return data;
}

std::size_t Driver::getSize(const std::string path) const
{
    if (auto size = tryGetSize(path)) return *size;
    else throw ArbiterError("Could not get size of " + path);
}

void Driver::put(std::string path, const std::string& data) const
{
    put(path, std::vector<char>(data.begin(), data.end()));
}

void Driver::copy(std::string src, std::string dst) const
{
    put(dst, getBinary(src));
}

std::vector<std::string> Driver::resolve(
        std::string path,
        const bool verbose) const
{
    std::vector<std::string> results;

    if (path.size() > 1 && path.back() == '*')
    {
        if (verbose)
        {
            std::cout << "Resolving [" << type() << "]: " << path << " ..." <<
                std::flush;
        }

        results = glob(path, verbose);

        if (verbose)
        {
            std::cout << "\n\tResolved to " << results.size() <<
                " paths." << std::endl;
        }
    }
    else
    {
        if (isRemote()) path = type() + "://" + path;
        else path = expandTilde(path);

        results.push_back(path);
    }

    return results;
}

std::vector<std::string> Driver::glob(std::string path, bool verbose) const
{
    throw ArbiterError("Cannot glob driver for: " + path);
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

