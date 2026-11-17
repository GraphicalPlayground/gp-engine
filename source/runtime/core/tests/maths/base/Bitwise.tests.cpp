// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/base/Bitwise.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::math::tests
{

TEST(BitwiseTest, CountTrailingZerosCoversAllUnsignedWidthsAndZero)
{
    EXPECT_EQ(countTrailingZeros<UInt8>(0u), 8u);
    EXPECT_EQ(countTrailingZeros<UInt8>(0x10u), 4u);
    EXPECT_EQ(countTrailingZeros<UInt16>(0u), 16u);
    EXPECT_EQ(countTrailingZeros<UInt16>(0x0800u), 11u);

    EXPECT_EQ(countTrailingZeros(0u), 32u);
    EXPECT_EQ(countTrailingZeros(1u), 0u);
    EXPECT_EQ(countTrailingZeros(0b10101000u), 3u);
    EXPECT_EQ(countTrailingZeros(0x80000000u), 31u);

    EXPECT_EQ(countTrailingZeros(UInt64{ 0 }), UInt64{ 64 });
    EXPECT_EQ(countTrailingZeros(UInt64{ 1 } << 40), UInt64{ 40 });

    static_assert(countTrailingZeros(0u) == 32u);
    static_assert(countTrailingZeros(0b1000u) == 3u);
    static_assert(countTrailingZeros(UInt64{ 1 } << 63) == UInt64{ 63 });
}

TEST(BitwiseTest, CountBitsCountsSetBitsAcrossAllWidths)
{
    EXPECT_EQ(countBits<UInt8>(0u), 0u);
    EXPECT_EQ(countBits<UInt8>(0xFFu), 8u);
    EXPECT_EQ(countBits<UInt16>(0xA5A5u), 8u);

    EXPECT_EQ(countBits(0u), 0u);
    EXPECT_EQ(countBits(1u), 1u);
    EXPECT_EQ(countBits(0xAAAAAAAAu), 16u);
    EXPECT_EQ(countBits(0xFFFFFFFFu), 32u);

    EXPECT_EQ(countBits(UInt64{ 0 }), UInt64{ 0 });
    EXPECT_EQ(countBits(0xFFFFFFFFFFFFFFFFull), UInt64{ 64 });
    EXPECT_EQ(countBits(0x8000000000000001ull), UInt64{ 2 });

    static_assert(countBits(0u) == 0u);
    static_assert(countBits(0b101101u) == 4u);
    static_assert(countBits(0xFFFFFFFFFFFFFFFFull) == UInt64{ 64 });
}

}   // namespace gp::math::tests
