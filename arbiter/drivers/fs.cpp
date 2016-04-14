#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#endif

#ifndef ARBITER_WINDOWS
#include <glob.h>
#include <sys/stat.h>
#else
#define UNICODE
#include <locale>
#include <codecvt>
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

    void noHome()
    {
        throw ArbiterError("No home directory found");
    }
}

namespace drivers
{

std::unique_ptr<Fs> Fs::create(HttpPool&, const Json::Value&)
{
    return std::unique_ptr<Fs>(new Fs());
}

std::unique_ptr<std::size_t> Fs::tryGetSize(std::string path) const
{
    std::unique_ptr<std::size_t> size;

    path = fs::expandTilde(path);

    std::ifstream stream(path, std::ios::in | std::ios::binary);

    if (stream.good())
    {
        stream.seekg(0, std::ios::end);
        size.reset(new std::size_t(stream.tellg()));
    }

    return size;
}

bool Fs::get(std::string path, std::vector<char>& data) const
{
    bool good(false);

    path = fs::expandTilde(path);
    std::ifstream stream(path, std::ios::in | std::ios::binary);

    if (stream.good())
    {
        stream.seekg(0, std::ios::end);
        data.resize(static_cast<std::size_t>(stream.tellg()));
        stream.seekg(0, std::ios::beg);
        stream.read(data.data(), data.size());
        stream.close();
        good = true;
    }

    return good;
}

void Fs::put(std::string path, const std::vector<char>& data) const
{
    path = fs::expandTilde(path);
    std::ofstream stream(path, binaryTruncMode);

    if (!stream.good())
    {
        throw ArbiterError("Could not open " + path + " for writing");
    }

    stream.write(data.data(), data.size());

    if (!stream.good())
    {
        throw ArbiterError("Error occurred while writing " + path);
    }
}

std::vector<std::string> Fs::glob(std::string path, bool) const
{
    std::vector<std::string> results;

#ifndef ARBITER_WINDOWS
    path = fs::expandTilde(path);

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
            throw ArbiterError("Error globbing - POSIX stat failed");
        }
    }

    globfree(&buffer);
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    const std::wstring wide(converter.from_bytes(path));

	LPWIN32_FIND_DATAW* data;
	data = (LPWIN32_FIND_DATAW*) malloc(1 * sizeof(LPWIN32_FIND_DATAW));

    HANDLE hFind(FindFirstFileW(wide.c_str(), *data));

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (((*data)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                results.push_back(converter.to_bytes((*data)->cFileName));
            }
        }
        while (FindNextFileW(hFind, *data));
    }
#endif

    return results;
}

} // namespace drivers

namespace fs
{

bool mkdirp(std::string dir)
{
    dir = expandTilde(dir);

#ifndef ARBITER_WINDOWS
    const bool err(::mkdir(dir.c_str(), S_IRWXU | S_IRGRP | S_IROTH));
    return (!err || errno == EEXIST);
#else
    throw ArbiterError("Windows mkdirp not done yet.");
#endif
}

bool remove(std::string filename)
{
    filename = expandTilde(filename);

#ifndef ARBITER_WINDOWS
    return ::remove(filename.c_str()) == 0;
#else
    throw ArbiterError("Windows remove not done yet.");
#endif
}

std::string expandTilde(std::string in)
{
    std::string out(in);

    if (!in.empty() && in.front() == '~')
    {
#ifndef ARBITER_WINDOWS
        if (!getenv("HOME"))
        {
            noHome();
        }

        static const std::string home(getenv("HOME"));
#else
		char* user_profile = 0;
		char* home_path = 0;
		char* home_drive = 0;
		size_t len(0);
		errno_t err(0);
		err = _dupenv_s(&user_profile, &len, "USERPROFILE");
		err = _dupenv_s(&home_drive, &len, "HOMEDRIVE");
		err = _dupenv_s(&home_path, &len, "HOMEPATH");

        if (
                !user_profile &&
                !(home_drive && home_path))
        {
            noHome();
        }

		std::string merged = std::string(home_drive) + std::string(home_path);
		static const std::string home(user_profile ? user_profile : merged);
#endif

        out = home + in.substr(1);
    }

    return out;
}

std::string getTempPath()
{
    std::string result;

#ifndef ARBITER_WINDOWS
    if (const char* t = getenv("TMPDIR"))   return t;
    if (const char* t = getenv("TMP"))      return t;
    if (const char* t = getenv("TEMP"))     return t;
    if (const char* t = getenv("TEMPDIR"))  return t;
    if (result.empty()) return "/tmp";
#else
    throw ArbiterError("Windows getTempPath not done yet.");
#endif

    return result;
}

LocalHandle::LocalHandle(const std::string localPath, const bool isRemote)
    : m_localPath(expandTilde(localPath))
    , m_erase(isRemote)
{ }

LocalHandle::~LocalHandle()
{
    if (m_erase) fs::remove(fs::expandTilde(m_localPath));
}

} // namespace fs
} // namespace arbiter

