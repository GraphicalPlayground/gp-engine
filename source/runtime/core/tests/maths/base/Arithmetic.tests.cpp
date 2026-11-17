// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/base/Arithmetic.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::math::tests
{

TEST(ArithmeticTest, DivideAndRoundUpUnsignedNominalAndBoundaryCases)
{
    EXPECT_EQ(divideAndRoundUp(0u, 4u), 0u);
    EXPECT_EQ(divideAndRoundUp(1u, 4u), 1u);
    EXPECT_EQ(divideAndRoundUp(4u, 4u), 1u);
    EXPECT_EQ(divideAndRoundUp(5u, 4u), 2u);
    EXPECT_EQ(divideAndRoundUp(1920u, 16u), 120u);
    EXPECT_EQ(divideAndRoundUp(1921u, 16u), 121u);

    constexpr UInt32 kMaxU32 = std::numeric_limits<UInt32>::max();
    EXPECT_EQ(divideAndRoundUp(kMaxU32, 1u), kMaxU32);
    EXPECT_EQ(divideAndRoundUp(kMaxU32, 4u), (kMaxU32 / 4u) + 1u);
    EXPECT_EQ(divideAndRoundUp<UInt8>(255u, 16u), 16u);

    static_assert(divideAndRoundUp(0u, 8u) == 0u);
    static_assert(divideAndRoundUp(9u, 8u) == 2u);
    static_assert(divideAndRoundUp(kMaxU32, 4u) == (kMaxU32 / 4u) + 1u);
}

TEST(ArithmeticTest, DivideAndRoundUpSignedAndNegativeValues)
{
    EXPECT_EQ(divideAndRoundUp(0, 4), 0);
    EXPECT_EQ(divideAndRoundUp(1, 4), 1);
    EXPECT_EQ(divideAndRoundUp(4, 4), 1);
    EXPECT_EQ(divideAndRoundUp(5, 4), 2);
    EXPECT_EQ(divideAndRoundUp(-3, 4), 0);
    EXPECT_EQ(divideAndRoundUp(-4, 4), -1);
    EXPECT_EQ(divideAndRoundUp(-5, 4), -1);
    EXPECT_EQ(divideAndRoundUp(-5, -4), 2);

    constexpr Int32 kMaxI32 = std::numeric_limits<Int32>::max();
    EXPECT_EQ(divideAndRoundUp(kMaxI32, 2), (kMaxI32 / 2) + 1);

    static_assert(divideAndRoundUp(-5, 4) == -1);
    static_assert(divideAndRoundUp(-5, -4) == 2);
}

}   // namespace gp::math::tests
