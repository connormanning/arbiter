#include <numeric>
#include <set>

#include <arbiter/util/time.hpp>
#include <arbiter/arbiter.hpp>
#include <arbiter/util/transforms.hpp>

#include "config.hpp"

#include "gtest/gtest.h"

using namespace arbiter;

TEST(Env, HomeDir)
{
    std::string home;

    ASSERT_NO_THROW(home = arbiter::fs::expandTilde("~"));
    EXPECT_NE(home, "~");
    EXPECT_FALSE(home.empty());
}

TEST(Arbiter, HttpDerivation)
{
    Arbiter a;
    EXPECT_TRUE(a.isHttpDerived("http://arbitercpp.com"));
    EXPECT_FALSE(a.isHttpDerived("~/data"));
    EXPECT_FALSE(a.isHttpDerived("."));
}

TEST(Arbiter, Time)
{
    Time a;
    Time b(a.str());

    EXPECT_EQ(a.str(), b.str());
    EXPECT_EQ(a - b, 0);

    Time x("2016-03-18T03:14:42Z");
    Time y("2016-03-18T04:24:54Z");
    const int64_t delta(1 * 60 * 60 + 10 * 60 + 12);

    EXPECT_EQ(y - x, delta);
    EXPECT_EQ(x - y, -delta);

    Time epoch("1970-01-01T00:00:00Z");
    EXPECT_EQ(epoch.asUnix(), 0);
}

TEST(Arbiter, Base64)
{
    EXPECT_EQ(crypto::encodeBase64(""), "");
    EXPECT_EQ(crypto::encodeBase64("f"), "Zg==");
    EXPECT_EQ(crypto::encodeBase64("fo"), "Zm8=");
    EXPECT_EQ(crypto::encodeBase64("foo"), "Zm9v");
    EXPECT_EQ(crypto::encodeBase64("foob"), "Zm9vYg==");
    EXPECT_EQ(crypto::encodeBase64("fooba"), "Zm9vYmE=");
    EXPECT_EQ(crypto::encodeBase64("foobar"), "Zm9vYmFy");
}

class DriverTest : public ::testing::TestWithParam<std::string> { };

TEST_P(DriverTest, PutGet)
{
    Arbiter a;

    const std::string root(GetParam());
    const std::string path(root + "test.txt");
    const std::string data("Testing path " + path);

    if (a.isLocal(root)) fs::mkdirp(root);

    EXPECT_NO_THROW(a.put(path, data));
    EXPECT_EQ(a.get(path), data);
}

TEST_P(DriverTest, HttpRange)
{
    Arbiter a;

    const std::string root(GetParam());
    const std::string path(root + "range.txt");
    const std::string data("0123456789");

    const std::size_t x(2);
    const std::size_t y(8);
    const http::Headers headers{
        // The range header is inclusive of the end, so subtract 1 here.
        { "Range", "bytes=" + std::to_string(x) + "-" + std::to_string(y - 1) }
    };

    if (!a.isHttpDerived(root)) return;

    EXPECT_NO_THROW(a.put(path, data));
    EXPECT_EQ(a.get(path, headers), data.substr(x, y - x));
}

TEST_P(DriverTest, Glob)
{
    using Paths = std::set<std::string>;
    Arbiter a;

    const std::string rawRoot(GetParam());

    // Local directories explicitly prefixed with file:// will be returned
    // without that prefix, so strip that off.
    const std::string root(
            a.isLocal(rawRoot) ? Arbiter::stripType(rawRoot) : rawRoot);
    const std::string type(Arbiter::getType(root));

    // No `ls` for plain HTTP/s.
    if (type == "http" || type == "https") return;

    auto put([&a, root](std::string p)
    {
        EXPECT_NO_THROW(a.put(root + p, p));
    });

    if (a.isLocal(root))
    {
        fs::mkdirp(root + "a");
        fs::mkdirp(root + "a/b");
    }

    put("one.txt");
    put("two.txt");
    put("a/one.txt");
    put("a/two.txt");
    put("a/b/one.txt");
    put("a/b/two.txt");

    auto resolve([&a, root](std::string p)
    {
        const auto v = a.resolve(root + p);
        return Paths(v.begin(), v.end());
    });

    auto check([&](std::string p, Paths exp)
    {
        const auto got(resolve(p));
        EXPECT_GE(got.size(), exp.size());

        for (const auto& s : exp)
        {
            EXPECT_TRUE(got.count(root + s)) << p << ": " << root << s;
        }
    });

    // Non-recursive.
    check("*", Paths { "one.txt", "two.txt" });
    check("a/*", Paths { "a/one.txt", "a/two.txt" });
    check("a/b/*", Paths { "a/b/one.txt", "a/b/two.txt" });

    // Recursive.
    check("**", Paths {
            "one.txt", "two.txt",
            "a/one.txt", "a/two.txt",
            "a/b/one.txt", "a/b/two.txt"
        }
    );

    check("a/**", Paths {
            "a/one.txt", "a/two.txt",
            "a/b/one.txt", "a/b/two.txt"
        }
    );

    check("a/b/**", Paths { "a/b/one.txt", "a/b/two.txt" });

    // Not globs - files resolve to themselves.
    check("", Paths { "" });
    check("asdf", Paths { "asdf" });
    check("asdf.txt", Paths { "asdf.txt" });
}

const auto tests = std::accumulate(
        Config::get().begin(),
        Config::get().end(),
        std::vector<std::string>(),
        [](const std::vector<std::string>& in, const Json::Value& entry)
        {
            if (in.empty())
            {
                std::cout << "Testing PUT/GET/LS with:" << std::endl;
            }

            auto out(in);
            const std::string path(entry.asString());
            out.push_back(path + (path.back() != '/' ? "/" : ""));
            std::cout << "\t" << out.back() << std::endl;
            return out;
        });

INSTANTIATE_TEST_CASE_P(
        ConfiguredTests,
        DriverTest,
        ::testing::ValuesIn(tests));

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

