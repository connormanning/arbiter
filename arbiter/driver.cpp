#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>

#include <arbiter/arbiter.hpp>
#endif

namespace arbiter
{

std::unique_ptr<std::vector<char>> Driver::tryGetBinary(std::string path) const
{
    std::unique_ptr<std::vector<char>> data(new std::vector<char>());

    if (!get(path, *data))
    {
        data.reset();
    }

    return data;
}

std::vector<char> Driver::getBinary(std::string path) const
{
    std::vector<char> data;

    if (!get(path, data))
    {
        throw std::runtime_error("Could not read file " + path);
    }

    return data;
}

std::unique_ptr<std::string> Driver::tryGet(std::string path) const
{
    std::unique_ptr<std::string> result;
    std::unique_ptr<std::vector<char>> data(tryGetBinary(path));
    if (data) result.reset(new std::string(data->begin(), data->end()));
    return result;
}

std::string Driver::get(const std::string path) const
{
    const std::vector<char> data(getBinary(path));
    return std::string(data.begin(), data.end());
}

void Driver::put(std::string path, const std::string& data) const
{
    put(path, std::vector<char>(data.begin(), data.end()));
}

std::vector<std::string> Driver::resolve(
        const std::string path,
        const bool verbose) const
{
    std::vector<std::string> results;

    if (
            path.size() > 2 &&
            path.back() == '*' &&
            (path[path.size() - 2] == '/' || path[path.size() - 2] == '\\'))
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
        results.push_back(path);
    }

    return results;
}

} // namespace arbiter

