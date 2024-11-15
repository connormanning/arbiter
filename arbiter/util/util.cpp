#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/util.hpp>

#include <arbiter/arbiter.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <mutex>
#include <random>
#include <string>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace
{
    const std::string protocolDelimiter("://");

    std::mutex randomMutex;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned long long> distribution;

    bool iequals(const std::string& s, const std::string& s2)
    {
        if (s.length() != s2.length())
            return false;
        for (std::size_t i = 0; i < s.length(); ++i)
        {
            if (std::tolower(s[i]) != std::tolower(s2[i])) return false;
        }
        return true;
    }
}

std::unique_ptr<std::string> findHeader(
        const http::Headers& headers,
        const std::string key)
{
    for (const auto& p : headers)
    {
        if (iequals(p.first, key))
        {
            return makeUnique<std::string>(p.second);
        }
    }
    return std::unique_ptr<std::string>();
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
    std::string result(stripProtocol(fullPath));

    const std::string stripped(stripPostfixing(stripProtocol(fullPath)));

    // Now do the real slash searching.
    std::size_t pos(stripped.find_last_of("/\\"));

    if (pos != std::string::npos)
    {
        const std::string sub(stripped.substr(pos + 1));
        if (!sub.empty()) result = sub;
    }

    return result;
}

std::string getDirname(const std::string fullPath)
{
    std::string result("");

    const std::string stripped(stripPostfixing(stripProtocol(fullPath)));

    // Now do the real slash searching.
    std::size_t pos(stripped.find_last_of("/\\"));

    if (pos != std::string::npos)
    {
        const std::string sub(stripped.substr(0, pos));
        result = sub;
    }

    const std::string protocol(getProtocol(fullPath));
    if (protocol != "file") result = protocol + "://" + result;

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

bool parseBoolFromEnv(const std::string& var, bool defaultValue)
{
    auto value = env(var);
    if (!value)
    {
        // env var is not set
        return defaultValue;
    }
    if (value->empty())
    {
        // env var is set to the empty string; interpret as false
        return false;
    }

    const char firstChar = std::tolower((*value)[0]);
    if (firstChar == 't' || firstChar == 'T' || firstChar == '1')
        return true;
    else if (firstChar == 'f' || firstChar == 'F' || firstChar == '0')
        return false;
    else
        return defaultValue;
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

std::string getProtocol(const std::string path)
{
    std::string type("file");
    const std::size_t pos(path.find(protocolDelimiter));

    if (pos != std::string::npos)
    {
        type = path.substr(0, pos);
    }

    return type;
}

std::string stripProtocol(const std::string raw)
{
    std::string result(raw);
    const std::size_t pos(raw.find(protocolDelimiter));

    if (pos != std::string::npos)
    {
        result = raw.substr(pos + protocolDelimiter.size());
    }

    return result;
}

std::string getExtension(std::string path)
{
    path = getBasename(path);
    const std::size_t pos(path.find_last_of('.'));

    if (pos != std::string::npos) return path.substr(pos + 1);
    else return std::string();
}

std::string stripExtension(const std::string path)
{
    const std::size_t pos(path.find_last_of('.'));
    return path.substr(0, pos);
}

std::string getProfile(std::string protocol)
{
    const std::size_t pos(protocol.find_last_of('@'));

    if (pos != std::string::npos) return protocol.substr(0, pos);
    return "";
}

std::string stripProfile(std::string protocol)
{
    const std::string profile = getProfile(protocol);
    if (profile.size() && protocol.size() >= profile.size() + 1)
    {
        return protocol.substr(profile.size() + 1);
    }
    return protocol;
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

