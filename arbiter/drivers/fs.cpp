#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/util.hpp>
#endif

#ifndef ARBITER_WINDOWS
#include <glob.h>
#include <sys/stat.h>
#else
#define UNICODE
#include <Shlwapi.h>
#include <iterator>
#include <locale>
#include <codecvt>
#include <windows.h>
#include <direct.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <istream>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    // Binary output, overwriting any existing file with a conflicting name.
    const std::ios_base::openmode binaryTruncMode(
            std::ofstream::binary |
            std::ofstream::out |
            std::ofstream::trunc);

    std::string getHome()
    {
        std::string s;

#ifndef ARBITER_WINDOWS
        if (auto home = env("HOME")) s = *home;
#else
        if (auto userProfile = env("USERPROFILE"))
        {
            s = *userProfile;
        }
        else
        {
            auto homeDrive(env("HOMEDRIVE"));
            auto homePath(env("HOMEPATH"));

            if (homeDrive && homePath) s = *homeDrive + *homePath;
        }
#endif
        if (s.empty()) std::cout << "No home directory found" << std::endl;

        return s;
    }
}

namespace drivers
{

std::unique_ptr<Fs> Fs::create()
{
    return std::unique_ptr<Fs>(new Fs());
}

std::unique_ptr<std::size_t> Fs::tryGetSize(std::string path) const
{
    std::unique_ptr<std::size_t> size;

    path = expandTilde(path);

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

    path = expandTilde(path);
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
    path = expandTilde(path);
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

void Fs::copy(std::string src, std::string dst) const
{
    src = expandTilde(src);
    dst = expandTilde(dst);

    std::ifstream instream(src, std::ifstream::in | std::ifstream::binary);
    if (!instream.good())
    {
        throw ArbiterError("Could not open " + src + " for reading");
    }
    instream >> std::noskipws;

    std::ofstream outstream(dst, binaryTruncMode);
    if (!outstream.good())
    {
        throw ArbiterError("Could not open " + dst + " for writing");
    }

    outstream << instream.rdbuf();
}

std::vector<std::string> Fs::glob(std::string path, bool verbose) const
{
    return arbiter::glob(path);
}

std::vector<char> Fs::getBinaryChunk(std::string path, size_t start,
    size_t end) const 
{
    std::vector<char> retBuffer;
    std::ifstream iStream(path, std::ifstream::binary | std::ios::in);
    if (!iStream.good()) {
        throw ArbiterError("Unable to open "+ path);
    }

    iStream.seekg(0, std::ios::end);
    if (end > iStream.tellg()) end = iStream.tellg();
	
    retBuffer.resize(end - start);
	
    iStream.seekg(start, std::ios::beg);
    if (!iStream.good()) {
        throw ArbiterError("Unable to Move to "+ std::to_string(start));
    }

    iStream.read(retBuffer.data(), end - start);
    if (!iStream.good()) {
        throw ArbiterError("Unable to read " + std::to_string(start) + " - " + std::to_string(end));
    }
	
    iStream.close();

    return retBuffer;
}

} // namespace drivers


bool mkdirp(std::string raw)
{
    const std::string dir(([&raw]()
    {
        std::string s(expandTilde(raw));

        // Remove consecutive slashes.  For Windows, we'll need to be careful
        // not to remove drive letters like C:\\.
        const auto end = std::unique(s.begin(), s.end(), [](char l, char r)
        {
            return isSlash(l) && isSlash(r);
        });

        s = std::string(s.begin(), end);
        if (s.size() && isSlash(s.back())) s.pop_back();
        return s;
    })());

    auto it(dir.begin());
    const auto end(dir.cend());

    do
    {
        it = std::find_if(++it, end, isSlash);

        const std::string cur(dir.begin(), it);
#ifndef ARBITER_WINDOWS
        const bool err(::mkdir(cur.c_str(), 0777));
        if (err && errno != EEXIST) return false;
#else
        // Use CreateDirectory instead of _mkdir; it is more reliable when creating directories on a drive other than the working path.

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		const std::wstring wide(converter.from_bytes(cur));
		const bool err(::CreateDirectoryW(wide.c_str(), NULL));
        if (err && ::GetLastError() != ERROR_ALREADY_EXISTS) return false;
#endif
    }
    while (it != end);

    return true;

}

bool remove(std::string filename)
{
    filename = expandTilde(filename);

    return ::remove(filename.c_str()) == 0;
}

namespace
{
    struct Globs
    {
        std::vector<std::string> files;
        std::vector<std::string> dirs;
    };

template<typename C>
	std::basic_string<C> remove_dups(std::basic_string<C> s, C c)
	{
		C cc[3] = { c, c };
		auto pos = s.find(cc);
		while (pos != s.npos) {
			s.erase(pos, 1);
			pos = s.find(cc, pos + 1);
		}
		return s;
	}

#ifdef ARBITER_WINDOWS
	bool icase_wchar_cmp(wchar_t a, wchar_t b)
	{
		return std::toupper(a, std::locale()) == std::toupper(b, std::locale());
	}


	bool icase_cmp(std::wstring const& s1, std::wstring const& s2)
	{
		return (s1.size() == s2.size()) &&
			std::equal(s1.begin(), s1.end(), s2.begin(),
				icase_wchar_cmp);
	}
#endif

    Globs globOne(std::string path)
    {
        Globs results;

#ifndef ARBITER_WINDOWS
        glob_t buffer;
        struct stat info;

        ::glob(path.c_str(), GLOB_NOSORT | GLOB_MARK, 0, &buffer);

        for (std::size_t i(0); i < buffer.gl_pathc; ++i)
        {
            const std::string val(buffer.gl_pathv[i]);

            if (stat(val.c_str(), &info) == 0)
            {
                if (S_ISREG(info.st_mode)) results.files.push_back(val);
                else if (S_ISDIR(info.st_mode)) results.dirs.push_back(val);
            }
            else
            {
                throw ArbiterError("Error globbing - POSIX stat failed");
            }
        }

        globfree(&buffer);
#else
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		std::wstring wide(converter.from_bytes(path));

		WIN32_FIND_DATAW data{};
		LPCWSTR fname = wide.c_str();
        HANDLE hFind(INVALID_HANDLE_VALUE);
		hFind = FindFirstFileW(fname, &data);

		if (hFind == (HANDLE)-1) return results; // bad filename

        if (hFind != INVALID_HANDLE_VALUE )
        {
            do
            {
				if (icase_cmp(std::wstring(data.cFileName), L".") ||
					icase_cmp(std::wstring(data.cFileName), L".."))
					continue;

				std::vector<wchar_t> buf(MAX_PATH);
				wide.erase(std::remove(wide.begin(), wide.end(), '*'), wide.end());

				std::replace(wide.begin(), wide.end(), '\\', '/');

				std::copy(wide.begin(), wide.end(), buf.begin()	);
                BOOL appended = PathAppendW(buf.data(), data.cFileName);

				std::wstring output(buf.data(), wcslen( buf.data()));

                // Erase any \'s
                output.erase(std::remove(output.begin(), output.end(), '\\'), output.end());

                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    results.dirs.push_back(converter.to_bytes(output));

                    output.append(L"/*");
                    Globs more = globOne(converter.to_bytes(output));
                    std::copy(more.dirs.begin(), more.dirs.end(), std::back_inserter(results.dirs));
                    std::copy(more.files.begin(), more.files.end(), std::back_inserter(results.files));

                }

				results.files.push_back(
					converter.to_bytes(output));
            }
            while (FindNextFileW(hFind, &data));
        }
		FindClose(hFind);
#endif

        return results;
    }

    std::vector<std::string> walk(std::string dir)
    {
        std::vector<std::string> paths;
        paths.push_back(dir);

        for (const auto& d : globOne(dir + '*').dirs)
        {
            const auto next(walk(d));
            paths.insert(paths.end(), next.begin(), next.end());
        }

        return paths;
    }
}

std::vector<std::string> glob(std::string path)
{
    std::vector<std::string> results;

    path = expandTilde(path);

    if (path.find('*') == std::string::npos)
    {
        results.push_back(path);
        return results;
    }

    std::vector<std::string> dirs;

    const std::size_t recPos(path.find("**"));
    if (recPos != std::string::npos)
    {
        // Convert this recursive glob into multiple non-recursive ones.
        const auto pre(path.substr(0, recPos));     // Cut off before the '*'.
        const auto post(path.substr(recPos + 1));   // Includes the second '*'.

        for (const auto d : walk(pre)) dirs.push_back(d + post);
    }
    else
    {
        dirs.push_back(path);
    }

    for (const auto& p : dirs)
    {
        Globs globs(globOne(p));
        results.insert(results.end(), globs.files.begin(), globs.files.end());
    }

    return results;
}

std::string expandTilde(std::string in)
{
    std::string out(in);
    static std::string home(getHome());
    if (!in.empty() && in.front() == '~')
    {
        if (home.empty()) throw ArbiterError("No home directory found");
        out = home + in.substr(1);
    }

    return out;
}

std::string getTempPath()
{
    std::string tmp;
#ifndef ARBITER_WINDOWS
    if (const auto t = env("TMPDIR"))         tmp = *t;
    else if (const auto t = env("TMP"))       tmp = *t;
    else if (const auto t = env("TEMP"))      tmp = *t;
    else if (const auto t = env("TEMPDIR"))   tmp = *t;
    else tmp = "/tmp";
#else
    std::vector<char> path(MAX_PATH, '\0');
    if (GetTempPath(MAX_PATH, path.data())) tmp.assign(path.data());
#endif

    if (tmp.empty()) throw ArbiterError("Could not find a temp path.");
    if (tmp.back() != '/') tmp += '/';
    return tmp;
}

LocalHandle::LocalHandle(const std::string localPath, const bool isRemote)
    : m_localPath(expandTilde(localPath))
    , m_erase(isRemote)
{ }

LocalHandle::~LocalHandle()
{
    if (m_erase) remove(expandTilde(m_localPath));
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

