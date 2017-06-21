#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/google.hpp>
#endif

#ifdef ARBITER_OPENSSL
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#endif

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/transforms.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    std::mutex sslMutex;

    const std::string baseGoogleUrl("www.googleapis.com/storage/v1/");
    const std::string uploadUrl("www.googleapis.com/upload/storage/v1/");
    const http::Query altMediaQuery{ { "alt", "media" } };

    class GResource
    {
    public:
        GResource(std::string path)
        {
            const std::size_t split(path.find("/"));
            m_bucket = path.substr(0, split) + "/";
            if (split != std::string::npos) m_object = path.substr(split + 1);
        }

        const std::string& bucket() const { return m_bucket; }
        const std::string& object() const { return m_object; }
        std::string endpoint() const
        {
            // https://cloud.google.com/storage/docs/json_api/#encoding
            static const std::string exclusions("!$&'()*+,;=:@");

            // https://cloud.google.com/storage/docs/json_api/v1/
            return
                baseGoogleUrl + "b/" + bucket() +
                "o/" + http::sanitize(object(), exclusions);
        }

        std::string uploadEndpoint() const
        {
            return uploadUrl + "b/" + bucket() + "o";
        }

        std::string listEndpoint() const
        {
            return baseGoogleUrl + "b/" + bucket() + "o";
        }

    private:
        std::string m_bucket;
        std::string m_object;

    };
} // unnamed namespace

namespace drivers
{

Google::Google(http::Pool& pool, std::unique_ptr<Auth> auth)
    : Https(pool)
    , m_auth(std::move(auth))
{ }

std::unique_ptr<Google> Google::create(
        http::Pool& pool,
        const Json::Value& json)
{
    if (auto auth = Auth::create(json))
    {
        return util::makeUnique<Google>(pool, std::move(auth));
    }

    return std::unique_ptr<Google>();
}

std::unique_ptr<std::size_t> Google::tryGetSize(const std::string path) const
{
    http::Headers headers(m_auth->headers());
    const GResource resource(path);

    drivers::Https https(m_pool);
    const auto res(
            https.internalHead(resource.endpoint(), headers, altMediaQuery));

    if (res.ok() && res.headers().count("Content-Length"))
    {
        const auto& s(res.headers().at("Content-Length"));
        return util::makeUnique<std::size_t>(std::stoul(s));
    }

    return std::unique_ptr<std::size_t>();
}

bool Google::get(
        const std::string path,
        std::vector<char>& data,
        const http::Headers userHeaders,
        const http::Query query) const
{
    http::Headers headers(m_auth->headers());
    headers.insert(userHeaders.begin(), userHeaders.end());
    const GResource resource(path);

    drivers::Https https(m_pool);
    const auto res(
            https.internalGet(resource.endpoint(), headers, altMediaQuery));

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

void Google::put(
        const std::string path,
        const std::vector<char>& data,
        const http::Headers userHeaders,
        const http::Query userQuery) const
{
    const GResource resource(path);
    const std::string url(resource.uploadEndpoint());

    http::Headers headers(m_auth->headers());
    headers["Expect"] = "";
    headers.insert(userHeaders.begin(), userHeaders.end());

    http::Query query(userQuery);
    query["uploadType"] = "media";
    query["name"] = resource.object();

    drivers::Https https(m_pool);
    const auto res(https.internalPost(url, data, headers, query));
}

std::vector<std::string> Google::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;

    path.pop_back();
    const bool recursive(path.back() == '*');
    if (recursive) path.pop_back();

    const GResource resource(path);
    const std::string url(resource.listEndpoint());
    std::string pageToken;

    drivers::Https https(m_pool);
    http::Query query;

    // When the delimiter is set to "/", then the response will contain a
    // "prefixes" key in addition to the "items" key.  The "prefixes" key will
    // contain the directories found, which we will ignore.
    if (!recursive) query["delimiter"] = "/";
    if (resource.object().size()) query["prefix"] = resource.object();

    do
    {
        if (pageToken.size()) query["pageToken"] = pageToken;

        const auto res(https.internalGet(url, m_auth->headers(), query));

        if (!res.ok())
        {
            throw ArbiterError(std::to_string(res.code()) + ": " + res.str());
        }

        const Json::Value json(util::parse(res.str()));
        for (const auto& item : json["items"])
        {
            results.push_back(
                    type() + "://" +
                    resource.bucket() + item["name"].asString());
        }

        pageToken = json["nextPageToken"].asString();
    } while (pageToken.size());

    return results;
}

///////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Google::Auth> Google::Auth::create(const Json::Value& json)
{
    if (auto path = util::env("GOOGLE_APPLICATION_CREDENTIALS"))
    {
        if (const auto file = drivers::Fs().tryGet(*path))
        {
            return util::makeUnique<Auth>(util::parse(*file));
        }
    }
    else if (json.isString())
    {
        const auto path = json.asString();
        if (const auto file = drivers::Fs().tryGet(path))
        {
            return util::makeUnique<Auth>(util::parse(*file));
        }
    }

    return std::unique_ptr<Auth>();
}

Google::Auth::Auth(const Json::Value& creds)
    : m_creds(creds)
{
    maybeRefresh();
}

http::Headers Google::Auth::headers() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    maybeRefresh();
    return m_headers;
}

void Google::Auth::maybeRefresh() const
{
    using namespace crypto;

    const auto now(Time().asUnix());
    if (m_expiration - now > 120) return;   // Refresh when under 2 mins left.

    // https://developers.google.com/identity/protocols/OAuth2ServiceAccount
    Json::Value h;
    h["alg"] = "RS256";
    h["typ"] = "JWT";

    Json::Value c;
    c["iss"] = m_creds["client_email"].asString();
    c["scope"] = "https://www.googleapis.com/auth/devstorage.read_write";
    c["aud"] = "https://www.googleapis.com/oauth2/v4/token";
    c["iat"] = Json::Int64(now);
    c["exp"] = Json::Int64(now + 3600);

    const std::string header(encodeBase64(util::toFastString(h)));
    const std::string claims(encodeBase64(util::toFastString(c)));

    const std::string key(m_creds["private_key"].asString());
    const std::string signature(
            http::sanitize(encodeBase64(sign(header + '.' + claims, key))));

    const std::string assertion(header + '.' + claims + '.' + signature);
    const std::string sbody =
        "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&"
        "assertion=" + assertion;
    const std::vector<char> body(sbody.begin(), sbody.end());

    const http::Headers headers { { "Expect", "" } };
    const std::string tokenRequestUrl("www.googleapis.com/oauth2/v4/token");

    http::Pool pool;
    drivers::Https https(pool);
    const auto res(https.internalPost(tokenRequestUrl, body, headers));

    if (!res.ok()) throw ArbiterError("Failed to get token: " + res.str());

    const Json::Value token(util::parse(res.str()));
    m_headers["Authorization"] = "Bearer " + token["access_token"].asString();
    m_expiration = now + token["expires_in"].asInt64();
}

std::string Google::Auth::sign(
        const std::string data,
        const std::string pkey) const
{
    std::string signature;

#ifdef ARBITER_OPENSSL
    std::lock_guard<std::mutex> lock(sslMutex);

    auto loadKey([](std::string s, bool isPublic)->EVP_PKEY*
    {
        EVP_PKEY* key(nullptr);
        if (BIO* bio = BIO_new_mem_buf(s.data(), -1))
        {
            if (isPublic)
            {
                key = PEM_read_bio_PUBKEY(bio, &key, nullptr, nullptr);
            }
            else
            {
                key = PEM_read_bio_PrivateKey(bio, &key, nullptr, nullptr);
            }

            BIO_free(bio);

            if (!key)
            {
                std::vector<char> err(256, 0);
                ERR_error_string(ERR_get_error(), err.data());
                throw ArbiterError(
                        std::string("Could not load key: ") + err.data());
            }
        }

        return key;
    });


    EVP_PKEY* key(loadKey(pkey, false));

    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
    EVP_DigestSignInit(&ctx, nullptr, EVP_sha256(), nullptr, key);

    if (EVP_DigestSignUpdate(&ctx, data.data(), data.size()) == 1)
    {
        std::size_t size(0);
        if (EVP_DigestSignFinal(&ctx, nullptr, &size) == 1)
        {
            std::vector<unsigned char> v(size, 0);
            if (EVP_DigestSignFinal(&ctx, v.data(), &size) == 1)
            {
                signature.assign(reinterpret_cast<const char*>(v.data()), size);
            }
        }
    }

    EVP_MD_CTX_cleanup(&ctx);
    if (signature.empty()) throw ArbiterError("Could not sign JWT");
    return signature;
#else
    throw ArbiterError("Cannot use google driver without OpenSSL");
#endif
}

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

