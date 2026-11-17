// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/base/FloatingPoint.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::math::tests
{

TEST(FloatingPointTest, IsNaNAndIsFiniteWorkAtCompileTimeAndRuntime)
{
    constexpr Float32 kQuietNaNf = std::numeric_limits<Float32>::quiet_NaN();
    constexpr Float32 kSigNaNf = std::numeric_limits<Float32>::signaling_NaN();
    constexpr Float32 kInfF = std::numeric_limits<Float32>::infinity();
    constexpr Float64 kQuietNaNd = std::numeric_limits<Float64>::quiet_NaN();
    constexpr Float64 kInfD = std::numeric_limits<Float64>::infinity();

    static_assert(isNaN(kQuietNaNf));
    static_assert(!isNaN(0.0f));
    static_assert(!isNaN(kInfF));
    static_assert(!isNaN(-kInfF));

    static_assert(isNaN(kQuietNaNd));
    static_assert(!isNaN(42.0));
    static_assert(!isNaN(kInfD));

    static_assert(isFinite(0.0f));
    static_assert(isFinite(-0.0f));
    static_assert(isFinite(3.14f));
    static_assert(!isFinite(kInfF));
    static_assert(!isFinite(-kInfF));
    static_assert(!isFinite(kQuietNaNf));

    static_assert(isFinite(-123456.789));
    static_assert(!isFinite(kInfD));
    static_assert(!isFinite(-kInfD));
    static_assert(!isFinite(kQuietNaNd));

    EXPECT_TRUE(isNaN(kQuietNaNf));
    EXPECT_TRUE(isNaN(kSigNaNf));
    EXPECT_FALSE(isNaN(1.0f));
    EXPECT_FALSE(isNaN(kInfF));

    EXPECT_TRUE(isFinite(1.0f));
    EXPECT_FALSE(isFinite(kInfF));
    EXPECT_FALSE(isFinite(kQuietNaNf));

    EXPECT_TRUE(isNaN(kQuietNaNd));
    EXPECT_FALSE(isNaN(kInfD));
    EXPECT_TRUE(isFinite(1.0));
    EXPECT_FALSE(isFinite(kInfD));
}

TEST(FloatingPointTest, FloorCeilToIntAndTruncToIntMatchFloatingPointContracts)
{
    static_assert(floor(3.9f) == 3.0f);
    static_assert(floor(3.0f) == 3.0f);
    static_assert(floor(-3.1f) == -4.0f);
    static_assert(floor(-3.0f) == -3.0f);
    static_assert(floor(-0.5) == -1.0);
    static_assert(floor(16777216.0f) == 16777216.0f);

    static_assert(ceilToInt(3.1f) == 4);
    static_assert(ceilToInt(3.0f) == 3);
    static_assert(ceilToInt(-3.9f) == -3);
    static_assert(ceilToInt(-3.0f) == -3);
    static_assert(ceilToInt<Int64>(10000000000.25) == Int64{ 10000000001 });

    static_assert(truncToInt(3.9f) == 3);
    static_assert(truncToInt(-3.9f) == -3);
    static_assert(truncToInt<Int64>(-10000000000.75) == Int64{ -10000000000 });

    EXPECT_FLOAT_EQ(floor(3.9f), 3.0f);
    EXPECT_FLOAT_EQ(floor(-3.1f), -4.0f);
    EXPECT_FLOAT_EQ(floor(-3.0f), -3.0f);
    EXPECT_DOUBLE_EQ(floor(-0.25), -1.0);

    EXPECT_EQ(ceilToInt(3.1f), 4);
    EXPECT_EQ(ceilToInt(3.0f), 3);
    EXPECT_EQ(ceilToInt(-3.9f), -3);
    EXPECT_EQ(ceilToInt(-0.25f), 0);

    EXPECT_EQ(truncToInt(3.99f), 3);
    EXPECT_EQ(truncToInt(-3.99f), -3);
    EXPECT_EQ(truncToInt<Int64>(-42.8), Int64{ -42 });
}

TEST(FloatingPointTest, ModfSplitsIntegerAndSignedFractionalParts)
{
    Float32 intPartF = 0.0f;
    const Float32 fracF = modf(3.75f, &intPartF);
    EXPECT_FLOAT_EQ(intPartF, 3.0f);
    EXPECT_FLOAT_EQ(fracF, 0.75f);

    Float64 intPartD = 0.0;
    const Float64 fracD = modf(-4.25, &intPartD);
    EXPECT_DOUBLE_EQ(intPartD, -4.0);
    EXPECT_DOUBLE_EQ(fracD, -0.25);

    // Null output pointer should remain safe and still compute the fractional part.
    EXPECT_DOUBLE_EQ(modf(2.5, static_cast<Float64*>(nullptr)), 0.5);

    constexpr auto evalConstexprModf = []() consteval
    {
        Float64 ip = 0.0;
        const Float64 fp = modf(-5.5, &ip);
        return ip == -5.0 && fp == -0.5;
    };
    static_assert(evalConstexprModf());
}

}   // namespace gp::math::tests
