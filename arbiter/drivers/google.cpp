#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/google.hpp>
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

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/json.hpp>
#include <arbiter/util/transforms.hpp>
#endif

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

using namespace internal;

namespace
{
    std::mutex sslMutex;

    const char baseGoogleUrl[] = "www.googleapis.com/storage/v1/";
    const char uploadUrl[] = "www.googleapis.com/upload/storage/v1/";
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
        static const char exclusions[];
        std::string endpoint() const
        {
            // https://cloud.google.com/storage/docs/json_api/v1/
            return
                std::string(baseGoogleUrl) + "b/" + bucket() +
                "o/" + http::sanitize(object(), exclusions);
        }

        std::string uploadEndpoint() const
        {
            return std::string(uploadUrl) + "b/" + bucket() + "o";
        }

        std::string listEndpoint() const
        {
            return std::string(baseGoogleUrl) + "b/" + bucket() + "o";
        }

    private:
        std::string m_bucket;
        std::string m_object;

    };
    
    // https://cloud.google.com/storage/docs/json_api/#encoding
    const char GResource::exclusions[] = "!$&'()*+,;=:@";
    
} // unnamed namespace

namespace drivers
{

Google::Google(http::Pool& pool, std::unique_ptr<Auth> auth)
    : Https(pool)
    , m_auth(std::move(auth))
{ }

std::unique_ptr<Google> Google::create(http::Pool& pool, const std::string s)
{
    if (auto auth = Auth::create(s))
    {
        return makeUnique<Google>(pool, std::move(auth));
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
        return makeUnique<std::size_t>(std::stoull(s));
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
    query["name"] = http::sanitize(resource.object(), GResource::exclusions);

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

        const json j(json::parse(res.str()));
        for (const json& item : j.at("items"))
        {
            results.push_back(
                    type() + "://" +
                    resource.bucket() + item.at("name").get<std::string>());
        }

        pageToken = j.value("nextPageToken", "");
    } while (pageToken.size());

    return results;
}

///////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Google::Auth> Google::Auth::create(const std::string s)
{
    const json j(json::parse(s));
    if (auto path = env("GOOGLE_APPLICATION_CREDENTIALS"))
    {
        if (const auto file = drivers::Fs().tryGet(*path))
        {
            try
            {
                return makeUnique<Auth>(*file);
            }
            catch (const ArbiterError& e)
            {
                std::cout << e.what() << std::endl;
                return std::unique_ptr<Auth>();
            }
        }
    }
    else if (j.is_string())
    {
        const auto path(j.get<std::string>());
        if (const auto file = drivers::Fs().tryGet(path))
        {
            return makeUnique<Auth>(*file);
        }
    }
    else if (j.is_object())
    {
        return makeUnique<Auth>(s);
    }

    return std::unique_ptr<Auth>();
}

Google::Auth::Auth(const std::string s)
    : m_clientEmail(json::parse(s).at("client_email").get<std::string>())
    , m_privateKey(json::parse(s).at("private_key").get<std::string>())
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
    const json h { { "alg", "RS256" }, { "typ", "JWT" } };
    const json c {
        { "iss", m_clientEmail },
        { "scope", "https://www.googleapis.com/auth/devstorage.read_write" },
        { "aud", "https://www.googleapis.com/oauth2/v4/token" },
        { "iat", now },
        { "exp", now + 3600 }
    };

    const std::string header(encodeBase64(h.dump()));
    const std::string claims(encodeBase64(c.dump()));

    const std::string key(m_privateKey);
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

    if (!res.ok())
    {
        throw ArbiterError(
                "Failed to get token for Google authentication, "
                "request came back with response: " + res.str());
    }

    const json token(json::parse(res.str()));
    m_headers["Authorization"] =
        "Bearer " + token.at("access_token").get<std::string>();
    m_expiration = now + token.at("expires_in").get<int64_t>();
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
        // BIO_new_mem_buf needs non-const char*, so use a vector.
        std::vector<char> vec(s.data(), s.data() + s.size());

        EVP_PKEY* key(nullptr);
        if (BIO* bio = BIO_new_mem_buf(vec.data(), vec.size()))
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

#   ifdef ARBITER_OPENSSL_ATLEAST_1_1
    EVP_MD_CTX* ctx(EVP_MD_CTX_new());
#   else
    EVP_MD_CTX ctxAlloc;
    EVP_MD_CTX* ctx(&ctxAlloc);
#   endif
    EVP_MD_CTX_init(ctx);
    EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, key);

    if (EVP_DigestSignUpdate(ctx, data.data(), data.size()) == 1)
    {
        std::size_t size(0);
        if (EVP_DigestSignFinal(ctx, nullptr, &size) == 1)
        {
            std::vector<unsigned char> v(size, 0);
            if (EVP_DigestSignFinal(ctx, v.data(), &size) == 1)
            {
                signature.assign(reinterpret_cast<const char*>(v.data()), size);
            }
        }
    }

#   ifdef ARBITER_OPENSSL_ATLEAST_1_1
    EVP_MD_CTX_free(ctx);
#   else
    EVP_MD_CTX_cleanup(ctx);
#   endif

    if (signature.empty()) throw ArbiterError("Could not sign JWT");
    return signature;
#else
    throw ArbiterError("Cannot use google driver without OpenSSL");
#endif
}

void Google::upload(std::string path, const std::string resourcepath) const {
    Fs fs;
    size_t fileSize = fs.getSize(resourcepath);
    const GResource resource(path);
    std::string url(resource.uploadEndpoint());

    drivers::Https https(m_pool);

    http::Query query;
    query["uploadType"] = "resumable";
    query["name"] = http::sanitize(resource.object(), GResource::exclusions);

    http::Headers headers(m_auth->headers());
    headers["X-Upload-Content-Length"]=std::to_string(fileSize);
    headers["X-Upload-Content-Type"] ="application/octet-stream";
    std::vector<char> data;
    headers["Content-Length"] = std::to_string(data.size());

    // Initiate upload request. This will return an upload location, where we can perform chunked upload.
    auto res(https.internalPost(url,data, headers, query));
    if (!res.ok()){
        throw ArbiterError("Failed to initialize upload session.");
    }

    headers.clear();
    query.clear();
    url = res.headers().at("Location");

    // Trim Received url, It contains spaces at the start and may be at end.
    url.erase(0, url.find_first_not_of(' '));
    url.erase(url.find_last_not_of(' ') + 1);

    // Perform chunked upload
    for (size_t start(0); start < fileSize && (res.ok() || res.code() == 308); start += chunkSize) {
        const std::size_t end((std::min)(start + chunkSize, fileSize));
        const std::string range("bytes "+std::to_string(start)+"-"+std::to_string(end-1)+"/"+std::to_string(fileSize));
          
        const auto data(fs.getBinaryChunk(resourcepath, start, end));

        headers["Content-Range"]=range;
        headers["Content-Length"] = std::to_string(data.size());
          
        res = https.internalPut(url, data, headers, query);
    }

    if (!res.ok()) {
        throw ArbiterError("Error while uploading "+path+", Error code : "+std::to_string(res.code()));
    }
}

} // namespace drivers
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

