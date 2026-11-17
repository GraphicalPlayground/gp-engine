// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/HashMix.hpp"
#include "CoreMinimal.hpp"
#include <bit>
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

TEST(HashMixTest, ConstexprMixersAndReferenceVectors)
{
    static_assert(avalancheMix32(0u) == 0u);
    static_assert(avalancheMix32(1u) == 0x514E28B7u);

    static_assert(avalancheMix64(0ULL) == 0ULL);
    static_assert(avalancheMix64(1ULL) == 0xB456BCFC34C2CB2CULL);

    static_assert(splitMix64(0ULL) == 0ULL);
    static_assert(splitMix64(1ULL) == 0x5692161D100B05E5ULL);

    static_assert(prospectorMix32(0u) == 0u);
    static_assert(prospectorMix32(1u) == 0x042741D6u);

    static_assert(cityHash128To64(1ULL, 2ULL) == 0x86668A560EC835A1ULL);
    static_assert(foldHash64To32(0x123456789ABCDEF0ULL) != 0u);

    EXPECT_EQ(hashInt32(gp::Int32{ -1 }), prospectorMix32(0xFFFFFFFFu));
    EXPECT_EQ(hashInt64(gp::Int64{ -1 }), splitMix64(0xFFFFFFFFFFFFFFFFULL));
}

TEST(HashMixTest, FloatingPointNormalizationHandlesNegativeZeroAndNans)
{
    static_assert(normalizeFloatBits32(-0.0f) == 0u);
    static_assert(normalizeFloatBits32(+0.0f) == 0u);
    static_assert(normalizeFloatBits64(-0.0) == 0ULL);
    static_assert(normalizeFloatBits64(+0.0) == 0ULL);

    static_assert(hashFloat(-0.0f) == hashFloat(+0.0f));
    static_assert(hashDouble(-0.0) == hashDouble(+0.0));
    EXPECT_NE(hashFloat(1.0f), hashFloat(-1.0f));
    EXPECT_NE(hashDouble(1.0), hashDouble(-1.0));

    // Distinct NaN bit payloads normalize to one canonical quiet NaN digest.
    const gp::Float32 nanA = std::bit_cast<gp::Float32>(0x7FC00001u);
    const gp::Float32 nanB = std::bit_cast<gp::Float32>(0x7FFFFFFFu);
    EXPECT_EQ(normalizeFloatBits32(nanA), normalizeFloatBits32(nanB));
    EXPECT_EQ(hashFloat(nanA), hashFloat(nanB));

    const gp::Float64 nan64A = std::bit_cast<gp::Float64>(0x7FF8000000000001ULL);
    const gp::Float64 nan64B = std::bit_cast<gp::Float64>(0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(normalizeFloatBits64(nan64A), normalizeFloatBits64(nan64B));
    EXPECT_EQ(hashDouble(nan64A), hashDouble(nan64B));
}

TEST(HashMixTest, PointerSpatialGridAndBucketMapping)
{
    static_assert(hashPointer(nullptr) == 0ULL);

    gp::UInt8 bytes[4]{};
    const gp::UInt64 p0 = hashPointer(&bytes[0]);
    const gp::UInt64 p1 = hashPointer(&bytes[1]);
    EXPECT_NE(p0, 0ULL);
    EXPECT_NE(p1, 0ULL);
    EXPECT_NE(p0, p1);

    // Spatial coordinates remain well-defined for negative cells and distinguish permutations.
    static_assert(hashGrid2D(10, -5) != hashGrid2D(-5, 10));
    static_assert(hashGrid3D(1, 2, 3) != hashGrid3D(3, 2, 1));
    static_assert(hashGrid2D64(10, -5) != hashGrid2D64(-5, 10));
    static_assert(hashGrid3D64(-1, -2, -3) != hashGrid3D64(-3, -2, -1));

    EXPECT_NE(hashGrid3D(-100, 0, 50), hashGrid3D(100, 0, 50));
    EXPECT_NE(hashGrid3D64(0, 0, 0), hashGrid3D64(0, 0, 1));

    // Fast range mapping stays bounded in [0, bucketCount) and safely handles 0 buckets.
    static_assert(mapToBucket(0xFFFFFFFFu, 0u) == 0u);
    static_assert(mapToBucket(0xFFFFFFFFFFFFFFFFULL, 0ULL) == 0ULL);
    static_assert(mapToBucket(0u, 17u) == 0u);
    static_assert(mapToBucket(0xFFFFFFFFu, 17u) == 16u);
    static_assert(mapToBucket(0x8000000000000000ULL, 10ULL) == 5ULL);
    static_assert(mapToBucket(0xFFFFFFFFFFFFFFFFULL, 100ULL) == 99ULL);

    for (gp::UInt32 i = 0u; i < 64u; ++i)
    {
        const gp::UInt32 h32 = prospectorMix32(i + 1u);
        const gp::UInt64 h64 = splitMix64(static_cast<gp::UInt64>(i) + 1ULL);
        EXPECT_LT(mapToBucket(h32, 13u), 13u);
        EXPECT_LT(mapToBucket(h64, 97ULL), 97ULL);
    }
}

}   // namespace gp::hashing::tests
