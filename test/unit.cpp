#include <arbiter/arbiter.hpp>

#include "gtest/gtest.h"

TEST(Env, HomeDir)
{
    std::string home;

    ASSERT_NO_THROW(home = arbiter::fs::expandTilde("~"));
    EXPECT_NE(home, "~");
    EXPECT_FALSE(home.empty());
}

TEST(Arbiter, HttpDerivation)
{
    arbiter::Arbiter a;
    EXPECT_TRUE(a.isHttpDerived("http://arbitercpp.com"));
    EXPECT_FALSE(a.isHttpDerived("~/data"));
    EXPECT_FALSE(a.isHttpDerived("."));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

