// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Scalar
#include "maths/base/Scalar.hpp"

namespace gp::math::tests
{

TEST(ScalarTest, MinMax)
{
    EXPECT_EQ(min<int>(5, 10), 5);
    EXPECT_EQ(min<float>(5.5f, 2.2f), 2.2f);
    EXPECT_EQ(max<int>(5, 10), 10);
    EXPECT_EQ(max<float>(5.5f, 2.2f), 5.5f);

    EXPECT_EQ(min<int>(10, 5, 8), 5);
    EXPECT_EQ(max<int>(10, 5, 8), 10);

    static_assert(min(1, 2) == 1, "constexpr min failed");
    static_assert(max(1, 2, 3) == 3, "constexpr max failed");
}

TEST(ScalarTest, Clamp)
{
    EXPECT_EQ(clamp<int>(5, 0, 10), 5);
    EXPECT_EQ(clamp<int>(-5, 0, 10), 0);
    EXPECT_EQ(clamp<int>(15, 0, 10), 10);

    static_assert(clamp(15, 0, 10) == 10, "constexpr clamp failed");
}

TEST(ScalarTest, AbsSign)
{
    EXPECT_EQ(abs<int>(-5), 5);
    EXPECT_EQ(abs<float>(5.5f), 5.5f);
    EXPECT_EQ(abs<int>(0), 0);

    EXPECT_EQ(sign<int>(-50), -1);
    EXPECT_EQ(sign<int>(50), 1);
    EXPECT_EQ(sign<int>(0), 0);
    EXPECT_EQ(sign<float>(-0.001f), -1.0f);
}

TEST(ScalarTest, NearlyZeroAndEqual)
{
    EXPECT_TRUE(isNearlyZero<float>(0.0f));
    EXPECT_TRUE(isNearlyZero<float>(0.000001f, 0.00001f));
    EXPECT_FALSE(isNearlyZero<float>(0.1f, 0.01f));

    EXPECT_TRUE(isNearlyEqual<float>(1.0f, 1.0f));
    EXPECT_TRUE(isNearlyEqual<float>(1.0f, 1.000001f, 0.00001f));
    EXPECT_FALSE(isNearlyEqual<float>(1.0f, 1.1f, 0.01f));
}

TEST(ScalarTest, LerpAndInverseLerp)
{
    EXPECT_FLOAT_EQ((lerp<float, float>(0.0f, 10.0f, 0.5f)), 5.0f);
    EXPECT_FLOAT_EQ((lerp<float, float>(0.0f, 10.0f, 0.0f)), 0.0f);
    EXPECT_FLOAT_EQ((lerp<float, float>(0.0f, 10.0f, 1.0f)), 10.0f);

    EXPECT_FLOAT_EQ(inverseLerp<float>(0.0f, 10.0f, 5.0f), 0.5f);
    EXPECT_FLOAT_EQ(inverseLerp<float>(0.0f, 10.0f, 15.0f), 1.0f);
    EXPECT_FLOAT_EQ(inverseLerp<float>(0.0f, 10.0f, -5.0f), 0.0f);
    EXPECT_FLOAT_EQ(inverseLerp<float>(5.0f, 5.0f, 10.0f), 0.0f);
}

TEST(ScalarTest, RemapAndSmoothstep)
{
    EXPECT_FLOAT_EQ(remap<float>(0.0f, 10.0f, 0.0f, 100.0f, 5.0f), 50.0f);
    EXPECT_FLOAT_EQ(remap<float>(0.0f, 10.0f, 0.0f, 100.0f, 15.0f), 100.0f);

    EXPECT_FLOAT_EQ(smoothstep<float>(0.0f, 1.0f, -0.5f), 0.0f);
    EXPECT_FLOAT_EQ(smoothstep<float>(0.0f, 1.0f, 1.5f), 1.0f);
    EXPECT_FLOAT_EQ(smoothstep<float>(0.0f, 1.0f, 0.5f), 0.5f);
}

TEST(ScalarTest, DegreesRadians)
{
    EXPECT_FLOAT_EQ(degreesToRadians<float>(180.0f), 3.1415926535f);
    EXPECT_FLOAT_EQ(radiansToDegrees<float>(3.1415926535f), 180.0f);
}

TEST(ScalarTest, SquareRoots)
{
    EXPECT_FLOAT_EQ(sqrt<float>(16.0f), 4.0f);
    EXPECT_FLOAT_EQ(inverseSqrt<float>(16.0f), 0.25f);
}

TEST(ScalarTest, PowerOfTwoUtils)
{
    EXPECT_FALSE(isPowerOfTwo<int>(0));
    EXPECT_TRUE(isPowerOfTwo<int>(1));
    EXPECT_TRUE(isPowerOfTwo<int>(2));
    EXPECT_FALSE(isPowerOfTwo<int>(3));
    EXPECT_TRUE(isPowerOfTwo<int>(1024));
    EXPECT_FALSE(isPowerOfTwo<int>(-4));

    EXPECT_EQ(nextPowerOfTwo<uint32_t>(0), 1u);
    EXPECT_EQ(nextPowerOfTwo<uint32_t>(1), 1u);
    EXPECT_EQ(nextPowerOfTwo<uint32_t>(2), 2u);
    EXPECT_EQ(nextPowerOfTwo<uint32_t>(3), 4u);
    EXPECT_EQ(nextPowerOfTwo<uint32_t>(1000), 1024u);
}

TEST(ScalarTest, Log2AndBitwise)
{
    EXPECT_EQ(floorLog2<uint32_t>(0), 0u);
    EXPECT_EQ(floorLog2<uint32_t>(1), 0u);
    EXPECT_EQ(floorLog2<uint32_t>(2), 1u);
    EXPECT_EQ(floorLog2<uint32_t>(3), 1u);
    EXPECT_EQ(floorLog2<uint32_t>(1024), 10u);

    EXPECT_EQ(ceilLog2<uint32_t>(0), 0u);
    EXPECT_EQ(ceilLog2<uint32_t>(1), 0u);
    EXPECT_EQ(ceilLog2<uint32_t>(2), 1u);
    EXPECT_EQ(ceilLog2<uint32_t>(3), 2u);
    EXPECT_EQ(ceilLog2<uint32_t>(1024), 10u);
    EXPECT_EQ(ceilLog2<uint32_t>(1025), 11u);

    EXPECT_EQ(roundUpToPowerOfTwo<uint32_t>(0), 1u);
    EXPECT_EQ(roundUpToPowerOfTwo<uint32_t>(3), 4u);
    EXPECT_EQ(roundUpToPowerOfTwo<uint32_t>(1000), 1024u);

    EXPECT_EQ(roundDownToPowerOfTwo<uint32_t>(0), 0u);
    EXPECT_EQ(roundDownToPowerOfTwo<uint32_t>(3), 2u);
    EXPECT_EQ(roundDownToPowerOfTwo<uint32_t>(1000), 512u);

    EXPECT_EQ(countLeadingZeros<uint32_t>(0), 32u);
    EXPECT_EQ(countLeadingZeros<uint32_t>(1), 31u);
    EXPECT_EQ(countLeadingZeros<uint32_t>(~0u), 0u);
}

}   // namespace gp::math::tests
