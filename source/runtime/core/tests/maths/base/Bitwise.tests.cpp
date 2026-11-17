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

TEST(BitwiseTest, BitMaskIsolationAndClearingHelpers)
{
    static_assert(isolateLowestSetBit<UInt8>(0u) == 0u);
    static_assert(isolateLowestSetBit<UInt32>(0b101100u) == 0b000100u);
    static_assert(isolateLowestSetBit<UInt64>(0x8000000000000000ull) == 0x8000000000000000ull);

    static_assert(clearLowestSetBit<UInt8>(0u) == 0u);
    static_assert(clearLowestSetBit<UInt32>(0b101100u) == 0b101000u);
    static_assert(clearLowestSetBit<UInt64>(UInt64{ 1 } << 63) == 0ull);

    static_assert(bitMask<UInt8>(0u) == 0x01u);
    static_assert(bitMask<UInt8>(7u) == 0x80u);
    static_assert(bitMask<UInt64>(63u) == (UInt64{ 1 } << 63));

    static_assert(trailingBitMask<UInt8>(0u) == 0x00u);
    static_assert(trailingBitMask<UInt8>(5u) == 0x1Fu);
    static_assert(trailingBitMask<UInt8>(8u) == 0xFFu);
    static_assert(trailingBitMask<UInt8>(20u) == 0xFFu);
    static_assert(trailingBitMask<UInt64>(0u) == 0ull);
    static_assert(trailingBitMask<UInt64>(63u) == 0x7FFFFFFFFFFFFFFFull);
    static_assert(trailingBitMask<UInt64>(64u) == 0xFFFFFFFFFFFFFFFFull);

    EXPECT_EQ(isolateLowestSetBit(0b110100u), 0b000100u);
    EXPECT_EQ(clearLowestSetBit(0b110100u), 0b110000u);
    EXPECT_EQ(bitMask<UInt32>(17u), 1u << 17u);
    EXPECT_EQ(trailingBitMask<UInt32>(12u), 0xFFFu);
}

TEST(BitwiseTest, GetAndClearNextBitAndHighestBitHelpers)
{
    constexpr auto runConstexprSweep = []() noexcept -> UInt32
    {
        UInt64 mask = (UInt64{ 1 } << 0) | (UInt64{ 1 } << 7) | (UInt64{ 1 } << 63);
        const UInt32 b0 = getAndClearNextBit(mask);
        const UInt32 b1 = getAndClearNextBit(mask);
        const UInt32 b2 = getAndClearNextBit(mask);
        return (mask == 0ull && b0 == 0u && b1 == 7u && b2 == 63u) ? 1u : 0u;
    };
    static_assert(runConstexprSweep() == 1u);

    constexpr auto runConstexprHighest = []() noexcept -> UInt32
    {
        UInt32 mask = (1u << 2) | (1u << 19) | (1u << 31);
        UInt32 out = 0u;
        const bool ok1 = tryGetAndClearHighestBit(mask, out);
        const UInt32 h1 = out;
        const UInt32 h2 = getAndClearHighestBit(mask);
        const UInt32 h3 = getAndClearHighestBit(mask);
        const bool okEmpty = tryGetAndClearHighestBit(mask, out);
        return (ok1 && !okEmpty && h1 == 31u && h2 == 19u && h3 == 2u && mask == 0u) ? 1u : 0u;
    };
    static_assert(runConstexprHighest() == 1u);

    UInt64 word = (UInt64{ 1 } << 3) | (UInt64{ 1 } << 18) | (UInt64{ 1 } << 42) | (UInt64{ 1 } << 63);
    UInt32 extracted[4]{};
    UInt32 count = 0u;
    UInt32 bitIdx = 999u;
    while (tryGetAndClearNextBit(word, bitIdx))
    {
        extracted[count++] = bitIdx;
    }
    EXPECT_EQ(word, 0ull);
    EXPECT_EQ(count, 4u);
    EXPECT_EQ(extracted[0], 3u);
    EXPECT_EQ(extracted[1], 18u);
    EXPECT_EQ(extracted[2], 42u);
    EXPECT_EQ(extracted[3], 63u);

    UInt32 untouched = 777u;
    EXPECT_FALSE(tryGetAndClearNextBit(word, untouched));
    EXPECT_EQ(untouched, 777u);

    UInt8 byteMask = 0b10100100u;
    EXPECT_EQ(getAndClearNextBit(byteMask), 2u);
    EXPECT_EQ(getAndClearNextBit(byteMask), 5u);
    EXPECT_EQ(getAndClearNextBit(byteMask), 7u);
    EXPECT_EQ(byteMask, 0u);

    UInt16 shortMask = 0x8003u;
    EXPECT_EQ(getAndClearHighestBit(shortMask), 15u);
    EXPECT_EQ(getAndClearHighestBit(shortMask), 1u);
    EXPECT_EQ(getAndClearHighestBit(shortMask), 0u);
    EXPECT_EQ(shortMask, 0u);

    // Exhaustive full 64-bit word sweeps in both ascending and descending directions.
    UInt64 fullLow = ~0ULL;
    for (UInt32 expectedBit = 0u; expectedBit < 64u; ++expectedBit)
    {
        UInt32 outBit = 999u;
        ASSERT_TRUE(tryGetAndClearNextBit(fullLow, outBit));
        EXPECT_EQ(outBit, expectedBit);
    }
    EXPECT_EQ(fullLow, 0ULL);
    UInt32 dummyBit = 42u;
    EXPECT_FALSE(tryGetAndClearNextBit(fullLow, dummyBit));
    EXPECT_EQ(dummyBit, 42u);

    UInt64 fullHigh = ~0ULL;
    for (UInt32 step = 0u; step < 64u; ++step)
    {
        UInt32 outBit = 999u;
        ASSERT_TRUE(tryGetAndClearHighestBit(fullHigh, outBit));
        EXPECT_EQ(outBit, 63u - step);
    }
    EXPECT_EQ(fullHigh, 0ULL);
    EXPECT_FALSE(tryGetAndClearHighestBit(fullHigh, dummyBit));
    EXPECT_EQ(dummyBit, 42u);

    // Boundary single-bit and trailing masks across 16-bit, 32-bit, and 64-bit types.
    EXPECT_EQ(bitMask<UInt16>(15u), 0x8000u);
    EXPECT_EQ(bitMask<UInt32>(31u), 0x80000000u);
    EXPECT_EQ(trailingBitMask<UInt16>(0u), 0u);
    EXPECT_EQ(trailingBitMask<UInt16>(16u), 0xFFFFu);
    EXPECT_EQ(trailingBitMask<UInt32>(32u), 0xFFFFFFFFu);
    EXPECT_EQ(trailingBitMask<UInt32>(100u), 0xFFFFFFFFu);
    EXPECT_EQ(isolateLowestSetBit<UInt64>(0ULL), 0ULL);
    EXPECT_EQ(clearLowestSetBit<UInt64>(0ULL), 0ULL);
}

TEST(BitwiseTest, CountLeadingZerosCoversAllUnsignedWidthsAndZero)
{
    EXPECT_EQ(countLeadingZeros<UInt8>(0u), 8u);
    EXPECT_EQ(countLeadingZeros<UInt8>(0x80u), 0u);
    EXPECT_EQ(countLeadingZeros<UInt8>(0x10u), 3u);
    EXPECT_EQ(countLeadingZeros<UInt16>(0u), 16u);
    EXPECT_EQ(countLeadingZeros<UInt16>(0x0800u), 4u);

    EXPECT_EQ(countLeadingZeros(0u), 32u);
    EXPECT_EQ(countLeadingZeros(1u), 31u);
    EXPECT_EQ(countLeadingZeros(0x80000000u), 0u);

    EXPECT_EQ(countLeadingZeros(UInt64{ 0 }), UInt64{ 64 });
    EXPECT_EQ(countLeadingZeros(UInt64{ 1 } << 40), UInt64{ 23 });
    EXPECT_EQ(countLeadingZeros(UInt64{ 1 } << 63), UInt64{ 0 });

    static_assert(countLeadingZeros(0u) == 32u);
    static_assert(countLeadingZeros(UInt64{ 1 } << 63) == UInt64{ 0 });
    static_assert(countLeadingZeros<UInt16>(0x0001u) == 15u);
}

}   // namespace gp::math::tests
