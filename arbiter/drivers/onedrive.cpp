#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/onedrive.hpp>
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/json.hpp>
#include <arbiter/util/transforms.hpp>
#endif

#include <vector>

#ifdef ARBITER_OPENSSL
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

// See: https://www.openssl.org/docs/manmaster/man3/OPENSSL_VERSION_NUMBER.html
#   if OPENSSL_VERSION_NUMBER >= 0x010100000
#   define ARBITER_OPENSSL_ATLEAST_1_1
#   endif

#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace drivers
{

std::string baseOneDriveUrl = "https://graph.microsoft.com/v1.0/me/drive/root:/";

OneDrive::OneDrive(http::Pool& pool, std::unique_ptr<Auth> auth)
    : Https(pool)
    , m_auth(std::move(auth))
{ }

std::unique_ptr<OneDrive> OneDrive::create(http::Pool& pool, const std::string s)
{
    if (auto auth = Auth::create(s))
    {
        return makeUnique<OneDrive>(pool, std::move(auth));
    }

    return std::unique_ptr<OneDrive>();
}

std::unique_ptr<std::size_t> OneDrive::tryGetSize(const std::string path) const
{
    http::Headers headers(m_auth->headers());
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";
    std::string constructed = baseOneDriveUrl + path;

    drivers::Https https(m_pool);
    const auto res(https.internalGet(constructed, headers));

    if (res.ok())
    {
        //parse the data into a json object and extract size key value
        const auto obj = json::parse(res.data());
        if (obj.find("size") != obj.end())
        {
            return makeUnique<std::size_t>(obj.at("size").get<std::size_t>());
        }
    }
    else
    {
        std::cout <<
            "Failed get - " << res.code() << ": " << res.str() << std::endl;
        return std::unique_ptr<std::size_t>();
    }

    return std::unique_ptr<std::size_t>();
}

OneDrive::Auth::Auth(std::string s)
    : m_token("bearer " + json::parse(s).at("onedrive_token").get<std::string>())
    { }

bool OneDrive::get(
        const std::string path,
        std::vector<char>& data,
        const http::Headers userHeaders,
        const http::Query query) const
{
    http::Headers headers(m_auth->headers());
    headers["Content-Type"] = "application/octet-stream";
    headers.insert(userHeaders.begin(), userHeaders.end());
    std::string constructed = baseOneDriveUrl + path + ":/content";

    drivers::Https https(m_pool);
    const auto res(https.internalGet(constructed, headers));

    if (res.ok())
    {
        data = res.data();
        return true;
    }
    else
    {
        std::cout <<
            "Failed get - " << res.code() << ": " << res.str() << std::endl;
        return false;
    }
}

std::unique_ptr<OneDrive::Auth> OneDrive::Auth::create(const std::string s)
{
    return makeUnique<Auth>(s);
}

http::Headers OneDrive::Auth::headers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_headers["Authorization"] = m_token;
    return m_headers;
}

}//drivers

}//arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
};
#endif