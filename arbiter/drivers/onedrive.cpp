#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/onedrive.hpp>
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/json.hpp>
#include <arbiter/util/transforms.hpp>
#endif

#include <vector>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace drivers
{

std::string baseOneDriveUrl = "graph.microsoft.com/v1.0/me/drive/root/";

OneDrive::OneDrive(http::Pool& pool, std::unique_ptr<Auth> auth)
    : Https(pool)
    , m_auth(std::move(auth))
{ }

std::unique_ptr<std::size_t> OneDrive::tryGetSize(const std::string path) const
{
    http::Headers headers(m_auth->headers());
    std::string constructed = baseOneDriveUrl + path;

    drivers::Https https(m_pool);
    const auto res(https.internalHead(constructed, headers));

    if (res.ok())
    {
        if (res.headers().count("Content-Length"))
        {
            const auto& s(res.headers().at("Content-Length"));
            return makeUnique<std::size_t>(std::stoull(s));
        }
        else if (res.headers().count("content-length"))
        {
            const auto& s(res.headers().at("content-length"));
            return makeUnique<std::size_t>(std::stoull(s));
        }
    }

    return std::unique_ptr<std::size_t>();
}

OneDrive::Auth::Auth(std::string s)
    : m_token("Bearer " + json::parse(s).at("onedrive_token").get<std::string>())
    { }

http::Headers OneDrive::Auth::headers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_headers;
}

bool OneDrive::get(
        const std::string path,
        std::vector<char>& data,
        const http::Headers userHeaders,
        const http::Query query) const
{
    http::Headers headers(m_auth->headers());
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

}//drivers

}//arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
};
#endif