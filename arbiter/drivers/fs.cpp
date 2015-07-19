#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/fs.hpp>
#endif

#ifndef WINDOWS
#include <glob.h>
#include <sys/stat.h>
#endif

#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace arbiter
{

namespace
{
    // Binary output, overwriting any existing file with a conflicting name.
    const std::ios_base::openmode binaryTruncMode(
            std::ofstream::binary |
            std::ofstream::out |
            std::ofstream::trunc);

    const std::string home("HOME");

    std::string expandTilde(std::string in)
    {
        std::string out(in);

        if (!in.empty() && in.front() == '~')
        {
            out = getenv(home.c_str()) + in.substr(1);
        }

        return out;
    }
}

std::vector<char> FsDriver::getBinary(std::string path) const
{
    path = expandTilde(path);
    std::ifstream stream(path, std::ios::in | std::ios::binary);

    if (!stream.good())
    {
        throw std::runtime_error("Could not read file " + path);
    }

    stream.seekg(0, std::ios::end);
    std::vector<char> data(static_cast<std::size_t>(stream.tellg()));
    stream.seekg(0, std::ios::beg);
    stream.read(data.data(), data.size());
    stream.close();

    return data;
}

void FsDriver::put(std::string path, const std::vector<char>& data) const
{
    path = expandTilde(path);
    std::ofstream stream(path, binaryTruncMode);

    if (!stream.good())
    {
        throw std::runtime_error("Could not open " + path + " for writing");
    }

    stream.write(data.data(), data.size());

    if (!stream.good())
    {
        throw std::runtime_error("Error occurred while writing " + path);
    }
}

std::vector<std::string> FsDriver::glob(std::string path, bool) const
{
    std::vector<std::string> results;

#ifndef WINDOWS
    path = expandTilde(path);

    glob_t buffer;
    struct stat info;

    ::glob(path.c_str(), GLOB_NOSORT | GLOB_TILDE, 0, &buffer);

    for (std::size_t i(0); i < buffer.gl_pathc; ++i)
    {
        const std::string val(buffer.gl_pathv[i]);

        if (stat(val.c_str(), &info) == 0)
        {
            if (S_ISREG(info.st_mode))
            {
                results.push_back(val);
            }
        }
        else
        {
            throw std::runtime_error("Error globbing - POSIX stat failed");
        }
    }

    globfree(&buffer);
#else

#endif

    return results;
}

} // namespace arbiter

