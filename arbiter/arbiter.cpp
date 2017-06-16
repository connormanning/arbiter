#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>

#include <arbiter/driver.hpp>
#include <arbiter/util/util.hpp>
#endif

#include <algorithm>
#include <cstdlib>
#include <sstream>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    const std::string delimiter("://");

#ifdef ARBITER_CURL
    const std::size_t concurrentHttpReqs(32);
    const std::size_t httpRetryCount(8);
#endif

    // Merge B into A, without overwriting any keys from A.
    Json::Value merge(const Json::Value& a, const Json::Value& b)
    {
        Json::Value out(a);

        if (!b.isNull())
        {
            for (const auto& key : b.getMemberNames())
            {
                // If A doesn't have this key, then set it to B's value.
                // If A has the key but it's an object, then recursively merge.
                // Otherwise A already has a value here that we won't overwrite.
                if (!out.isMember(key)) out[key] = b[key];
                else if (out[key].isObject()) merge(out[key], b[key]);
            }
        }

        return out;
    }

    Json::Value getConfig(const Json::Value& in)
    {
        Json::Value config;
        std::string path("~/.arbiter/config.json");

        if      (auto p = util::env("ARBITER_CONFIG_FILE")) path = *p;
        else if (auto p = util::env("ARBITER_CONFIG_PATH")) path = *p;

        if (auto data = drivers::Fs().tryGet(path))
        {
            std::istringstream ss(*data);
            ss >> config;
        }

        return merge(in, config);
    }
}

Arbiter::Arbiter() : Arbiter(Json::nullValue) { }

Arbiter::Arbiter(const Json::Value& in)
    : m_drivers()
#ifdef ARBITER_CURL
    , m_pool(new http::Pool(concurrentHttpReqs, httpRetryCount, getConfig(in)))
#endif
{
    using namespace drivers;

    const Json::Value json(getConfig(in));

    auto fs(Fs::create(json["file"]));
    if (fs) m_drivers[fs->type()] = std::move(fs);

    auto test(Test::create(json["test"]));
    if (test) m_drivers[test->type()] = std::move(test);

#ifdef ARBITER_CURL
    auto http(Http::create(*m_pool, json["http"]));
    if (http) m_drivers[http->type()] = std::move(http);

    auto https(Https::create(*m_pool, json["http"]));
    if (https) m_drivers[https->type()] = std::move(https);

    if (json["s3"].isArray())
    {
        for (const auto& sub : json["s3"])
        {
            auto s3(S3::create(*m_pool, sub));
            m_drivers[s3->type()] = std::move(s3);
        }
    }
    else
    {
        auto s3(S3::create(*m_pool, json["s3"]));
        if (s3) m_drivers[s3->type()] = std::move(s3);
    }

    // Credential-based drivers should probably all do something similar to the
    // S3 driver to support multiple profiles.
    auto dropbox(Dropbox::create(*m_pool, json["dropbox"]));
    if (dropbox) m_drivers[dropbox->type()] = std::move(dropbox);

#ifdef ARBITER_OPENSSL
    auto google(Google::create(*m_pool, json["google"]));
    if (google) m_drivers[google->type()] = std::move(google);
#endif

#endif
}

bool Arbiter::hasDriver(const std::string path) const
{
    return m_drivers.count(getType(path));
}

void Arbiter::addDriver(const std::string type, std::unique_ptr<Driver> driver)
{
    if (!driver) throw ArbiterError("Cannot add empty driver for " + type);
    m_drivers[type] = std::move(driver);
}

std::string Arbiter::get(const std::string path) const
{
    return getDriver(path).get(stripType(path));
}

std::vector<char> Arbiter::getBinary(const std::string path) const
{
    return getDriver(path).getBinary(stripType(path));
}

std::unique_ptr<std::string> Arbiter::tryGet(std::string path) const
{
    return getDriver(path).tryGet(stripType(path));
}

std::unique_ptr<std::vector<char>> Arbiter::tryGetBinary(std::string path) const
{
    return getDriver(path).tryGetBinary(stripType(path));
}

std::size_t Arbiter::getSize(const std::string path) const
{
    return getDriver(path).getSize(stripType(path));
}

std::unique_ptr<std::size_t> Arbiter::tryGetSize(const std::string path) const
{
    return getDriver(path).tryGetSize(stripType(path));
}

void Arbiter::put(const std::string path, const std::string& data) const
{
    return getDriver(path).put(stripType(path), data);
}

void Arbiter::put(const std::string path, const std::vector<char>& data) const
{
    return getDriver(path).put(stripType(path), data);
}

std::string Arbiter::get(
        const std::string path,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver(path).get(stripType(path), headers, query);
}

std::unique_ptr<std::string> Arbiter::tryGet(
        const std::string path,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver(path).tryGet(stripType(path), headers, query);
}

std::vector<char> Arbiter::getBinary(
        const std::string path,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver(path).getBinary(stripType(path), headers, query);
}

std::unique_ptr<std::vector<char>> Arbiter::tryGetBinary(
        const std::string path,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver(path).tryGetBinary(stripType(path), headers, query);
}

void Arbiter::put(
        const std::string path,
        const std::string& data,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver(path).put(stripType(path), data, headers, query);
}

void Arbiter::put(
        const std::string path,
        const std::vector<char>& data,
        const http::Headers headers,
        const http::Query query) const
{
    return getHttpDriver(path).put(stripType(path), data, headers, query);
}

void Arbiter::copy(
        const std::string src,
        const std::string dst,
        const bool verbose) const
{
    if (src.empty()) throw ArbiterError("Cannot copy from empty source");
    if (dst.empty()) throw ArbiterError("Cannot copy to empty destination");

    // Globify the source path if it's a directory.  In this case, the source
    // already ends with a slash.
    const std::string srcToResolve(src + (util::isDirectory(src) ? "**" : ""));

    if (srcToResolve.back() != '*')
    {
        // The source is a single file.
        copyFile(src, dst, verbose);
    }
    else
    {
        // We'll need this to mirror the directory structure in the output.
        // All resolved paths will contain this common prefix, so we can
        // determine any nested paths from recursive resolutions by stripping
        // that common portion.
        const Endpoint& srcEndpoint(getEndpoint(util::stripPostfixing(src)));
        const std::string commonPrefix(srcEndpoint.prefixedRoot());

        const Endpoint dstEndpoint(getEndpoint(dst));

        if (srcEndpoint.prefixedRoot() == dstEndpoint.prefixedRoot())
        {
            throw ArbiterError("Cannot copy directory to itself");
        }

        int i(0);
        const auto paths(resolve(srcToResolve, verbose));

        for (const auto& path : paths)
        {
            const std::string subpath(path.substr(commonPrefix.size()));

            if (verbose)
            {
                std::cout <<
                    ++i << " / " << paths.size() << ": " <<
                    path << " -> " << dstEndpoint.fullPath(subpath) <<
                    std::endl;
            }

            if (dstEndpoint.isLocal())
            {
                fs::mkdirp(util::getNonBasename(dstEndpoint.fullPath(subpath)));
            }

            dstEndpoint.put(subpath, getBinary(path));
        }
    }
}

void Arbiter::copyFile(
        const std::string file,
        std::string dst,
        const bool verbose) const
{
    if (dst.empty()) throw ArbiterError("Cannot copy to empty destination");

    const Endpoint dstEndpoint(getEndpoint(dst));

    if (util::isDirectory(dst))
    {
        // If the destination is a directory, maintain the basename of the
        // source file.
        dst += util::getBasename(file);
    }

    if (verbose) std::cout << file << " -> " << dst << std::endl;

    if (dstEndpoint.isLocal()) fs::mkdirp(util::getNonBasename(dst));

    if (getEndpoint(file).type() == dstEndpoint.type())
    {
        // If this copy is within the same driver domain, defer to the
        // hopefully specialized copy method.
        getDriver(file).copy(stripType(file), stripType(dst));
    }
    else
    {
        // Otherwise do a GET/PUT for the copy.
        put(dst, getBinary(file));
    }
}

bool Arbiter::isRemote(const std::string path) const
{
    return getDriver(path).isRemote();
}

bool Arbiter::isLocal(const std::string path) const
{
    return !isRemote(path);
}

bool Arbiter::exists(const std::string path) const
{
    return tryGetSize(path).get() != nullptr;
}

bool Arbiter::isHttpDerived(const std::string path) const
{
    return tryGetHttpDriver(path) != nullptr;
}

std::vector<std::string> Arbiter::resolve(
        const std::string path,
        const bool verbose) const
{
    return getDriver(path).resolve(stripType(path), verbose);
}

Endpoint Arbiter::getEndpoint(const std::string root) const
{
    return Endpoint(getDriver(root), stripType(root));
}

const Driver& Arbiter::getDriver(const std::string path) const
{
    const auto type(getType(path));

    if (!m_drivers.count(type))
    {
        throw ArbiterError("No driver for " + path);
    }

    return *m_drivers.at(type);
}

const drivers::Http* Arbiter::tryGetHttpDriver(const std::string path) const
{
    return dynamic_cast<const drivers::Http*>(&getDriver(path));
}

const drivers::Http& Arbiter::getHttpDriver(const std::string path) const
{
    if (auto d = tryGetHttpDriver(path)) return *d;
    else throw ArbiterError("Cannot get driver for " + path + " as HTTP");
}

std::unique_ptr<fs::LocalHandle> Arbiter::getLocalHandle(
        const std::string path,
        const Endpoint& tempEndpoint) const
{
    std::unique_ptr<fs::LocalHandle> localHandle;

    if (isRemote(path))
    {
        if (tempEndpoint.isRemote())
        {
            throw ArbiterError("Temporary endpoint must be local.");
        }

        std::string name(path);
        std::replace(name.begin(), name.end(), '/', '-');
        std::replace(name.begin(), name.end(), '\\', '-');
        std::replace(name.begin(), name.end(), ':', '_');

        tempEndpoint.put(name, getBinary(path));

        localHandle.reset(
                new fs::LocalHandle(tempEndpoint.root() + name, true));
    }
    else
    {
        localHandle.reset(
                new fs::LocalHandle(fs::expandTilde(stripType(path)), false));
    }

    return localHandle;
}

std::unique_ptr<fs::LocalHandle> Arbiter::getLocalHandle(
        const std::string path,
        std::string tempPath) const
{
    if (tempPath.empty()) tempPath = fs::getTempPath();
    return getLocalHandle(path, getEndpoint(tempPath));
}

std::string Arbiter::getType(const std::string path)
{
    std::string type("file");
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

std::string Arbiter::getExtension(const std::string path)
{
    const std::size_t pos(path.find_last_of('.'));

    if (pos != std::string::npos) return path.substr(pos + 1);
    else return std::string();
}

} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

