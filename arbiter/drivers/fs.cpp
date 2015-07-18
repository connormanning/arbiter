#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/fs.hpp>
#endif

#include <fstream>
#include <glob.h>
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
}

std::vector<char> FsDriver::getBinary(const std::string path) const
{
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

void FsDriver::put(const std::string path, const std::vector<char>& data) const
{
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

std::vector<std::string> FsDriver::glob(const std::string path, bool) const
{
    // TODO Platform dependent.
    std::vector<std::string> results;

    glob_t buffer;

    ::glob(path.c_str(), GLOB_NOSORT | GLOB_TILDE, 0, &buffer);

    for (std::size_t i(0); i < buffer.gl_pathc; ++i)
    {
        results.push_back(buffer.gl_pathv[i]);
    }

    globfree(&buffer);

    return results;
}

} // namespace arbiter

