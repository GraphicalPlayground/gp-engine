// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/HashCombine.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

TEST(HashCombineTest, CombineAndCombineFast32And64)
{
    static_assert(hashCombineFast(0u, 0u) != 0u);
    static_assert(hashCombineFast(0ULL, 0ULL) != 0ULL);
    static_assert(hashCombineFast(1u, 2u) != hashCombineFast(2u, 1u));
    static_assert(hashCombineFast(1ULL, 2ULL) != hashCombineFast(2ULL, 1ULL));

    static_assert(hashCombine(0u, 0u) != 0u);
    static_assert(hashCombine(0ULL, 0ULL) != 0ULL);
    static_assert(hashCombine(1u, 2u) != hashCombine(2u, 1u));
    static_assert(hashCombine(1ULL, 2ULL) != hashCombine(2ULL, 1ULL));

    // Variadic raw-hash chains match sequential fold order.
    static_assert(
        hashCombineFast(10u, 20u, 30u, 40u) == hashCombineFast(hashCombineFast(hashCombineFast(10u, 20u), 30u), 40u)
    );
    static_assert(hashCombineFast(10ULL, 20ULL, 30ULL) == hashCombineFast(hashCombineFast(10ULL, 20ULL), 30ULL));
    static_assert(hashCombine(10u, 20u, 30u, 40u) == hashCombine(hashCombine(hashCombine(10u, 20u), 30u), 40u));
    static_assert(hashCombine(10ULL, 20ULL, 30ULL) == hashCombine(hashCombine(10ULL, 20ULL), 30ULL));

    const Hash128 d1(0x1111ULL, 0x2222ULL);
    const Hash128 d2(0x3333ULL, 0x4444ULL);
    EXPECT_NE(hashCombine(d1, d2), hashCombine(d2, d1));
    EXPECT_NE(hashCombine(d1, d1), Hash128{});
}

TEST(HashCombineTest, SymmetricAndInPlaceHelpers)
{
    // Symmetric combination ignores order while avoiding equal-input cancellation to zero.
    static_assert(hashCombineSymmetric(42u, 99u) == hashCombineSymmetric(99u, 42u));
    static_assert(hashCombineSymmetric(42ULL, 99ULL) == hashCombineSymmetric(99ULL, 42ULL));
    static_assert(hashCombineSymmetric(77u, 77u) != 0u);
    static_assert(hashCombineSymmetric(77ULL, 77ULL) != 0ULL);

    gp::UInt32 seed32 = 123u;
    appendHash(seed32, 456u);
    EXPECT_EQ(seed32, hashCombine(123u, 456u));

    gp::UInt64 seed64 = 123ULL;
    appendHash(seed64, 456ULL);
    EXPECT_EQ(seed64, hashCombine(123ULL, 456ULL));

    gp::UInt32 fast32 = 123u;
    appendHashFast(fast32, 456u);
    EXPECT_EQ(fast32, hashCombineFast(123u, 456u));

    gp::UInt64 fast64 = 123ULL;
    appendHashFast(fast64, 456ULL);
    EXPECT_EQ(fast64, hashCombineFast(123ULL, 456ULL));
}

}   // namespace gp::hashing::tests
