// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/Interval.hpp"
#include "concepts/Memory.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

TEST(IntervalTest, DefaultSentinelClosedConstructionAndLayoutInvariants)
{
    static_assert(sizeof(Interval<UInt32>) == sizeof(UInt32) * 2u);
    static_assert(concepts::IsTriviallyCopyable<Interval<UInt32>>);
    static_assert(concepts::IsRelocatable<Interval<UInt64>>);

    constexpr Interval<UInt32> kEmpty{};
    static_assert(!kEmpty.isValid());
    static_assert(kEmpty.isEmpty());
    static_assert(!kEmpty.isValidHalfOpen());
    static_assert(kEmpty.size() == 0u);
    static_assert(kEmpty.center() == 0u);
    static_assert(!kEmpty.contains(0u));
    static_assert(!kEmpty.containsHalfOpen(0u));

    constexpr Interval<Int32> kPoint(5, 5);
    static_assert(kPoint.isValid());
    static_assert(!kPoint.isEmpty());
    static_assert(!kPoint.isValidHalfOpen());
    static_assert(kPoint.size() == 0);
    static_assert(kPoint.contains(5));
    static_assert(!kPoint.containsHalfOpen(5));

    // CTAD deduces Interval<UInt32> from matching bounds.
    constexpr Interval deduced(10u, 20u);
    static_assert(deduced.size() == 10u);
    static_assert(deduced.center() == 15u);

    // Midpoint near numeric limits must remain overflow-free.
    constexpr Interval<Int32> kWideSigned(-100, NumericLimits<Int32>::max());
    static_assert(kWideSigned.center() == 1073741773);

    EXPECT_FALSE(kEmpty.isValid());
    EXPECT_EQ(kEmpty.size(), 0u);
    EXPECT_TRUE(kPoint.contains(5));
    EXPECT_FALSE(kPoint.containsHalfOpen(5));
}

TEST(IntervalTest, ClosedMembershipOverlapAndIntersectHandleTransientPassRanges)
{
    const Interval<UInt32> passA = Interval<UInt32>::closed(2u, 6u);
    const Interval<UInt32> passB = Interval<UInt32>::closed(4u, 9u);
    const Interval<UInt32> touching = Interval<UInt32>::closed(6u, 8u);
    const Interval<UInt32> disjoint = Interval<UInt32>::closed(10u, 12u);

    EXPECT_TRUE(passA.contains(2u));
    EXPECT_TRUE(passA.contains(6u));
    EXPECT_FALSE(passA.contains(7u));
    EXPECT_TRUE(passA.contains(Interval<UInt32>::closed(3u, 5u)));
    EXPECT_FALSE(passA.contains(passB));

    EXPECT_TRUE(passA.overlaps(passB));
    EXPECT_TRUE(passA.overlaps(touching));
    EXPECT_FALSE(passA.overlaps(disjoint));

    const Interval<UInt32> overlap = passA.intersect(passB);
    EXPECT_EQ(overlap, Interval<UInt32>(4u, 6u));
    EXPECT_EQ(overlap.size(), 2u);

    const Interval<UInt32> endpointOverlap = passA.intersect(touching);
    EXPECT_EQ(endpointOverlap, Interval<UInt32>(6u, 6u));
    EXPECT_TRUE(endpointOverlap.isValid());

    EXPECT_FALSE(passA.intersect(disjoint).isValid());
    EXPECT_FALSE(passA.intersect(Interval<UInt32>::empty()).isValid());

    static_assert(Interval<UInt32>(1u, 5u).intersect(Interval<UInt32>(3u, 8u)) == Interval<UInt32>(3u, 5u));
}

TEST(IntervalTest, HalfOpenOperationsTreatAdjacentMemorySpansAsDisjoint)
{
    constexpr Interval<UInt64> kSpanA = Interval<UInt64>::halfOpen(0ull, 64ull);
    constexpr Interval<UInt64> kSpanB = Interval<UInt64>::halfOpen(64ull, 128ull);
    constexpr Interval<UInt64> kSpanOverlap = Interval<UInt64>::halfOpen(32ull, 96ull);

    static_assert(kSpanA.isValidHalfOpen());
    static_assert(kSpanA.containsHalfOpen(0ull));
    static_assert(!kSpanA.containsHalfOpen(64ull));
    static_assert(kSpanA.containsHalfOpen(Interval<UInt64>::halfOpen(16ull, 64ull)));
    static_assert(!kSpanA.overlapsHalfOpen(kSpanB));
    static_assert(kSpanA.overlapsHalfOpen(kSpanOverlap));
    static_assert(kSpanA.intersectHalfOpen(kSpanB).isEmpty());
    static_assert(kSpanA.intersectHalfOpen(kSpanOverlap) == Interval<UInt64>(32ull, 64ull));

    EXPECT_FALSE(kSpanA.overlapsHalfOpen(kSpanB));
    EXPECT_TRUE(kSpanA.overlapsHalfOpen(kSpanOverlap));
    EXPECT_EQ(kSpanA.intersectHalfOpen(kSpanOverlap).size(), 32ull);
}

TEST(IntervalTest, IncludeMergeAndExpandGrowFloatingAndSignedBounds)
{
    Interval<Float32> bounds;
    EXPECT_TRUE(bounds.isEmpty());

    bounds.include(10.0f);
    EXPECT_EQ(bounds, Interval<Float32>(10.0f, 10.0f));

    bounds.include(-2.5f);
    EXPECT_EQ(bounds, Interval<Float32>(-2.5f, 10.0f));
    EXPECT_FLOAT_EQ(bounds.size(), 12.5f);
    EXPECT_FLOAT_EQ(bounds.center(), 3.75f);

    bounds.expand(1.5f);
    EXPECT_EQ(bounds, Interval<Float32>(-4.0f, 11.5f));

    const Interval<Int32> merged = Interval<Int32>::merge(Interval<Int32>::empty(), Interval<Int32>(-5, 3));
    EXPECT_EQ(merged, Interval<Int32>(-5, 3));

    const Interval<Int32> mergedBoth = Interval<Int32>::merge(Interval<Int32>(-5, 3), Interval<Int32>(1, 9));
    EXPECT_EQ(mergedBoth, Interval<Int32>(-5, 9));

    // Empty expand remains a safe no-op without overflow.
    Interval<UInt32> untouchedEmpty;
    untouchedEmpty.expand(4u);
    EXPECT_TRUE(untouchedEmpty.isEmpty());
}

}   // namespace gp::tests
