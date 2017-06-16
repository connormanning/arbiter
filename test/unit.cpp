#include <arbiter/util/time.hpp>
#include <arbiter/arbiter.hpp>
#include <arbiter/util/transforms.hpp>

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

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

