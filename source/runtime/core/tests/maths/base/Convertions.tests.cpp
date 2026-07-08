// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Convertions utilities
#include "maths/base/Convertions.hpp"

namespace gp::math::convert::tests
{

/// @brief Helper constant for exact Gigabyte boundaries in tests
constexpr UInt64 OneGB = 1024ull * 1024ull * 1024ull;

TEST(BytesToGigabytesTest, HandlesZero)
{
    EXPECT_EQ(bytesToGigabytes<UInt32>(0ull), 0u);
    EXPECT_EQ(bytesToGigabytes<UInt64>(0ull), 0ull);
}

TEST(BytesToGigabytesTest, RoundsUpLessThanOneGigabyte)
{
    EXPECT_EQ(bytesToGigabytes<UInt32>(1ull), 1u);
    EXPECT_EQ(bytesToGigabytes<UInt32>(OneGB - 1ull), 1u);
}

TEST(BytesToGigabytesTest, HandlesExactGigabyteBoundaries)
{
    EXPECT_EQ(bytesToGigabytes<UInt32>(OneGB), 1u);
    EXPECT_EQ(bytesToGigabytes<UInt32>(5ull * OneGB), 5u);
}

TEST(BytesToGigabytesTest, RoundsUpSlightlyOverBoundary)
{
    EXPECT_EQ(bytesToGigabytes<UInt32>(OneGB + 1ull), 2u);
    EXPECT_EQ(bytesToGigabytes<UInt32>((5ull * OneGB) + 1ull), 6u);
}

TEST(BytesToGigabytesTest, CastsToDifferentUnsignedTypes)
{
    EXPECT_EQ(bytesToGigabytes<UInt8>(OneGB), 1);
    EXPECT_EQ(bytesToGigabytes<UInt16>(2ull * OneGB), 2);
    EXPECT_EQ(bytesToGigabytes<UInt32>(3ull * OneGB), 3u);
    EXPECT_EQ(bytesToGigabytes<UInt64>(4ull * OneGB), 4ull);
}

TEST(BytesToGigabytesTest, EvaluatesAtCompileTime)
{
    static_assert(bytesToGigabytes<UInt32>(0ull) == 0u, "Constexpr zero check failed");
    static_assert(bytesToGigabytes<UInt32>(OneGB) == 1u, "Constexpr exact GB check failed");
    static_assert(bytesToGigabytes<UInt32>(OneGB + 1ull) == 2u, "Constexpr round up check failed");
}

}   // namespace gp::math::convert::tests
