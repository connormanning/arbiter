#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>

#include <arbiter/arbiter.hpp>
#endif

namespace arbiter
{

std::string Driver::get(std::string path)
{
    const std::vector<char> data(getBinary(path));
    return std::string(data.begin(), data.end());
}

void Driver::put(std::string path, const std::string& data)
{
    put(path, std::vector<char>(data.begin(), data.end()));
}

std::vector<std::string> Driver::resolve(std::string path, bool verbose)
{
    std::vector<std::string> results;

    if (path.size() > 2 && path.substr(path.size() - 2) == "/*")
    {
        if (verbose)
        {
            std::cout << "Resolving " << path << " ..." << std::flush;
        }

        results = glob(Arbiter::stripType(path), verbose);

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

