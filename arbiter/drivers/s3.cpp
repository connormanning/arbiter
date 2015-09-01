#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/s3.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/drivers/fs.hpp>
#include <arbiter/third/xml/xml.hpp>
#include <arbiter/util/crypto.hpp>
#endif

namespace arbiter
{

namespace
{
    const std::string baseUrl(".s3.amazonaws.com/");

    std::string getQueryString(const Query& query)
    {
        std::string result;

        bool first(true);
        for (const auto& q : query)
        {
            result += (first ? "?" : "&") + q.first + "=" + q.second;
            first = false;
        }

        return result;
    }

    struct Resource
    {
        Resource(std::string fullPath)
        {
            const std::size_t split(fullPath.find("/"));

            bucket = fullPath.substr(0, split);

            if (split != std::string::npos)
            {
                object = fullPath.substr(split + 1);
            }
        }

        std::string buildPath(Query query = Query()) const
        {
            const std::string queryString(getQueryString(query));
            return "http://" + bucket + baseUrl + object + queryString;
        }

        std::string bucket;
        std::string object;
    };

    typedef Xml::xml_node<> XmlNode;

    const std::string badResponse("Unexpected contents in AWS response");
}

AwsAuth::AwsAuth(const std::string access, const std::string hidden)
    : m_access(access)
    , m_hidden(hidden)
{ }

std::unique_ptr<AwsAuth> AwsAuth::find(std::string user)
{
    std::unique_ptr<AwsAuth> auth;

    if (user.empty())
    {
        user = getenv("AWS_PROFILE") ? getenv("AWS_PROFILE") : "default";
    }

    FsDriver fs;
    std::unique_ptr<std::string> file(fs.tryGet("~/.aws/credentials"));

    // First, try reading credentials file.
    if (file)
    {
        std::size_t index(0);
        std::size_t pos(0);
        std::vector<std::string> lines;

        do
        {
            index = file->find('\n', pos);
            std::string line(file->substr(pos, index - pos));

            line.erase(
                    std::remove_if(line.begin(), line.end(), ::isspace),
                    line.end());

            lines.push_back(line);

            pos = index + 1;
        }
        while (index != std::string::npos);

        if (lines.size() >= 3)
        {
            std::size_t i(0);

            const std::string userFind("[" + user + "]");
            const std::string accessFind("aws_access_key_id=");
            const std::string hiddenFind("aws_secret_access_key=");

            while (i < lines.size() - 2 && !auth)
            {
                if (lines[i].find(userFind) != std::string::npos)
                {
                    const std::string& accessLine(lines[i + 1]);
                    const std::string& hiddenLine(lines[i + 2]);

                    std::size_t accessPos(accessLine.find(accessFind));
                    std::size_t hiddenPos(hiddenLine.find(hiddenFind));

                    if (
                            accessPos != std::string::npos &&
                            hiddenPos != std::string::npos)
                    {
                        const std::string access(
                                accessLine.substr(
                                    accessPos + accessFind.size(),
                                    accessLine.find(';')));

                        const std::string hidden(
                                hiddenLine.substr(
                                    hiddenPos + hiddenFind.size(),
                                    hiddenLine.find(';')));

                        auth.reset(new AwsAuth(access, hidden));
                    }
                }

                ++i;
            }
        }
    }

    // Fall back to environment settings.
    if (!auth)
    {
        if (getenv("AWS_ACCESS_KEY_ID") && getenv("AWS_SECRET_ACCESS_KEY"))
        {
            auth.reset(
                    new AwsAuth(
                        getenv("AWS_ACCESS_KEY_ID"),
                        getenv("AWS_SECRET_ACCESS_KEY")));
        }
        else if (
                getenv("AMAZON_ACCESS_KEY_ID") &&
                getenv("AMAZON_SECRET_ACCESS_KEY"))
        {
            auth.reset(
                    new AwsAuth(
                        getenv("AMAZON_ACCESS_KEY_ID"),
                        getenv("AMAZON_SECRET_ACCESS_KEY")));
        }
    }

    return auth;
}

std::string AwsAuth::access() const
{
    return m_access;
}

std::string AwsAuth::hidden() const
{
    return m_hidden;
}

S3Driver::S3Driver(HttpPool& pool, const AwsAuth auth)
    : m_pool(pool)
    , m_auth(auth)
{ }

bool S3Driver::get(std::string rawPath, std::vector<char>& data) const
{
    return get(rawPath, Query(), data);
}

bool S3Driver::get(
        const std::string rawPath,
        const Query& query,
        std::vector<char>& data) const
{
    const Resource resource(rawPath);

    const std::string path(resource.buildPath(query));
    const Headers headers(httpGetHeaders(rawPath));

    auto http(m_pool.acquire());

    HttpResponse res(http.get(path, headers));

    if (res.ok())
    {
        data = res.data();
        return true;
    }
    else
    {
        // TODO If verbose:
        // std::cout << std::string(res.data().begin(), res.data().end()) <<
            // std::endl;
        return false;
    }
}

std::vector<char> S3Driver::get(std::string rawPath, const Query& query) const
{
    std::vector<char> data;

    if (!get(rawPath, query, data))
    {
        throw std::runtime_error("Couldn't S3 GET " + rawPath);
    }

    return data;
}

void S3Driver::put(std::string rawPath, const std::vector<char>& data) const
{
    const Resource resource(rawPath);

    const std::string path(resource.buildPath());
    const Headers headers(httpPutHeaders(rawPath));

    auto http(m_pool.acquire());

    if (!http.put(path, data, headers).ok())
    {
        throw std::runtime_error("Couldn't S3 PUT to " + rawPath);
    }
}

std::vector<std::string> S3Driver::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;

    if (path.size() < 2 || path.substr(path.size() - 2) != "/*")
    {
        throw std::runtime_error("Invalid glob path: " + path);
    }

    path.resize(path.size() - 2);

    // https://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGET.html
    const Resource resource(path);
    const std::string& bucket(resource.bucket);
    const std::string& object(resource.object);
    const std::string prefix(
            resource.object.empty() ? "" : resource.object + "/");

    Query query;

    if (prefix.size()) query["prefix"] = prefix;

    bool more(false);

    do
    {
        if (verbose) std::cout << "." << std::flush;

        auto data = get(resource.bucket + "/", query);
        data.push_back('\0');

        Xml::xml_document<> xml;

        // May throw Xml::parse_error.
        xml.parse<0>(data.data());

        if (XmlNode* topNode = xml.first_node("ListBucketResult"))
        {
            if (XmlNode* truncNode = topNode->first_node("IsTruncated"))
            {
                std::string t(truncNode->value());
                std::transform(t.begin(), t.end(), t.begin(), tolower);

                more = (t == "true");
            }

            XmlNode* conNode(topNode->first_node("Contents"));

            if (!conNode) throw std::runtime_error(badResponse);

            for ( ; conNode; conNode = conNode->next_sibling())
            {
                if (XmlNode* keyNode = conNode->first_node("Key"))
                {
                    std::string key(keyNode->value());

                    // The prefix may contain slashes (i.e. is a sub-dir)
                    // but we only include the top level after that.
                    if (key.find('/', prefix.size()) == std::string::npos)
                    {
                        results.push_back("s3://" + bucket + "/" + key);

                        if (more)
                        {
                            query["marker"] =
                                (object.size() ? object + "/" : "") +
                                key.substr(prefix.size());
                        }
                    }
                }
                else
                {
                    throw std::runtime_error(badResponse);
                }
            }
        }
        else
        {
            throw std::runtime_error(badResponse);
        }

        xml.clear();
    }
    while (more);

    return results;
}

std::vector<std::string> S3Driver::httpGetHeaders(std::string filePath) const
{
    const std::string httpDate(getHttpDate());
    const std::string signedEncodedString(
            getSignedEncodedString(
                "GET",
                filePath,
                httpDate));

    const std::string dateHeader("Date: " + httpDate);
    const std::string authHeader(
            "Authorization: AWS " +
            m_auth.access() + ":" +
            signedEncodedString);
    std::vector<std::string> headers;
    headers.push_back(dateHeader);
    headers.push_back(authHeader);
    return headers;
}

std::vector<std::string> S3Driver::httpPutHeaders(std::string filePath) const
{
    const std::string httpDate(getHttpDate());
    const std::string signedEncodedString(
            getSignedEncodedString(
                "PUT",
                filePath,
                httpDate,
                "application/octet-stream"));

    const std::string typeHeader("Content-Type: application/octet-stream");
    const std::string dateHeader("Date: " + httpDate);
    const std::string authHeader(
            "Authorization: AWS " +
            m_auth.access() + ":" +
            signedEncodedString);

    std::vector<std::string> headers;
    headers.push_back(typeHeader);
    headers.push_back(dateHeader);
    headers.push_back(authHeader);
    headers.push_back("Transfer-Encoding:");
    headers.push_back("Expect:");
    return headers;
}

std::string S3Driver::getHttpDate() const
{
    time_t rawTime;
    char charBuf[80];

    time(&rawTime);

#ifndef ARBITER_WINDOWS
    tm* timeInfoPtr = localtime(&rawTime);
#else
    tm timeInfo;
    localtime_s(&timeInfo, &rawTime);
    tm* timeInfoPtr(&timeInfo);
#endif

    strftime(charBuf, 80, "%a, %d %b %Y %H:%M:%S %z", timeInfoPtr);
    std::string stringBuf(charBuf);

    return stringBuf;
}

std::string S3Driver::getSignedEncodedString(
        std::string command,
        std::string file,
        std::string httpDate,
        std::string contentType) const
{
    const std::string toSign(getStringToSign(
                command,
                file,
                httpDate,
                contentType));

    const std::vector<char> signedData(signString(toSign));
    return encodeBase64(signedData);
}

std::string S3Driver::getStringToSign(
        std::string command,
        std::string file,
        std::string httpDate,
        std::string contentType) const
{
    return
        command + "\n" +
        "\n" +
        contentType + "\n" +
        httpDate + "\n" +
        "/" + file;
}

std::vector<char> S3Driver::signString(std::string input) const
{
    return crypto::hmacSha1(m_auth.hidden(), input);
}

std::string S3Driver::encodeBase64(std::vector<char> data) const
{
    std::vector<uint8_t> input;
    for (std::size_t i(0); i < data.size(); ++i)
    {
        char c(data[i]);
        input.push_back(*reinterpret_cast<uint8_t*>(&c));
    }

    const std::string vals(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

    std::size_t fullSteps(input.size() / 3);
    while (input.size() % 3) input.push_back(0);
    uint8_t* pos(input.data());
    uint8_t* end(input.data() + fullSteps * 3);

    std::string output(fullSteps * 4, '_');
    std::size_t outIndex(0);

    const uint32_t mask(0x3F);

    while (pos != end)
    {
        uint32_t chunk((*pos) << 16 | *(pos + 1) << 8 | *(pos + 2));

        output[outIndex++] = vals[(chunk >> 18) & mask];
        output[outIndex++] = vals[(chunk >> 12) & mask];
        output[outIndex++] = vals[(chunk >>  6) & mask];
        output[outIndex++] = vals[chunk & mask];

        pos += 3;
    }

    if (end != input.data() + input.size())
    {
        const std::size_t num(pos - end == 1 ? 2 : 3);
        uint32_t chunk(*(pos) << 16 | *(pos + 1) << 8 | *(pos + 2));

        output.push_back(vals[(chunk >> 18) & mask]);
        output.push_back(vals[(chunk >> 12) & mask]);
        if (num == 3) output.push_back(vals[(chunk >> 6) & mask]);
    }

    while (output.size() % 4) output.push_back('=');

    return output;
}

} // namespace arbiter

