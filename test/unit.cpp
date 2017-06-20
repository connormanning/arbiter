#include <numeric>

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
    const std::string path(root + "test.txt");
    const std::string data("0123456789");
    const http::Headers headers{ { "Range", "bytes=0-5" } };

    // Dropbox is slow to update files, so we'd get failures here without a
    // timeout since the old file will often be returned.  Just skip it.
    if (!a.isHttpDerived(root) || a.getDriver(root).type() == "dropbox")
    {
        return;
    }

    EXPECT_NO_THROW(a.put(path, data));
    EXPECT_EQ(a.get(path, headers), "012345");
}

const auto tests = std::accumulate(
        Config::get().begin(),
        Config::get().end(),
        std::vector<std::string>(),
        [](const std::vector<std::string>& in, const Json::Value& entry)
        {
            if (in.empty()) std::cout << "Testing PUT/GET with:" << std::endl;
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

