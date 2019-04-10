#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/util.hpp>

#include <arbiter/arbiter.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <mutex>
#include <random>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    std::mutex randomMutex;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned long long> distribution;
}

uint64_t randomNumber()
{
    std::lock_guard<std::mutex> lock(randomMutex);
    return distribution(gen);
}

std::string stripPostfixing(const std::string path)
{
    std::string stripped(path);

    for (std::size_t i(0); i < 2; ++i)
    {
        // Pop trailing asterisk, or double-trailing-asterisks for both non- and
        // recursive globs.
        if (!stripped.empty() && stripped.back() == '*') stripped.pop_back();
    }

    // Pop trailing slash, in which case the result is the innermost directory.
    while (!stripped.empty() && isSlash(stripped.back())) stripped.pop_back();

    return stripped;
}

std::string getBasename(const std::string fullPath)
{
    std::string result(fullPath);

    const std::string stripped(stripPostfixing(Arbiter::stripType(fullPath)));

    // Now do the real slash searching.
    std::size_t pos(stripped.rfind('/'));

    // Maybe windows
    if (pos == std::string::npos)
        pos = stripped.rfind('\\');

    if (pos != std::string::npos)
    {
        const std::string sub(stripped.substr(pos + 1));
        if (!sub.empty()) result = sub;
    }

    return result;
}

std::string getNonBasename(const std::string fullPath)
{
    std::string result("");

    const std::string stripped(stripPostfixing(Arbiter::stripType(fullPath)));

    // Now do the real slash searching.
    const std::size_t pos(stripped.rfind('/'));

    if (pos != std::string::npos)
    {
        const std::string sub(stripped.substr(0, pos));
        result = sub;
    }

    const std::string type(Arbiter::getType(fullPath));
    if (type != "file") result = type + "://" + result;

    return result;
}

std::unique_ptr<std::string> env(const std::string& var)
{
    std::unique_ptr<std::string> result;

#ifndef ARBITER_WINDOWS
    if (const char* c = getenv(var.c_str())) result.reset(new std::string(c));
#else
    char* c(nullptr);
    std::size_t size(0);

    if (!_dupenv_s(&c, &size, var.c_str()))
    {
        if (c)
        {
            result.reset(new std::string(c));
            free(c);
        }
    }
#endif

    return result;
}

std::vector<std::string> split(const std::string& in, const char delimiter)
{
    std::size_t index(0);
    std::size_t pos(0);
    std::vector<std::string> lines;

    do
    {
        index = in.find(delimiter, pos);
        std::string line(in.substr(pos, index - pos));

        line.erase(
                std::remove_if(line.begin(), line.end(), ::isspace),
                line.end());

        lines.push_back(line);

        pos = index + 1;
    }
    while (index != std::string::npos);

    return lines;
}

std::string stripWhitespace(const std::string& in)
{
    std::string out(in);
    out.erase(
            std::remove_if(
                out.begin(),
                out.end(),
                [](char c) { return std::isspace(c); }),
            out.end());
    return out;
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

