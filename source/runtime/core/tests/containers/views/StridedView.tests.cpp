// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/views/StridedView.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

namespace
{

struct TransformData
{
    gp::Float32 x;
    gp::Float32 y;
    gp::Float32 z;
};

struct EntityRecord
{
    gp::UInt32 id;
    TransformData transform;
    gp::Int32 health;
};

/// @brief Compile-time verification of `gp::StridedView` operations in `constexpr` contexts.
[[nodiscard]] constexpr bool testConstexprStridedView() noexcept
{
    constexpr gp::Int32 values[6] = { 10, 20, 30, 40, 50, 60 };
    const gp::ConstStridedView<gp::Int32> contiguous(values);
    if (!contiguous.isContiguous() || contiguous.size() != 6u || contiguous.stride() != sizeof(gp::Int32))
    {
        return false;
    }

    const gp::ConstStridedView<gp::Int32> evens = contiguous.everyNth(2u);
    if (evens.size() != 3u || evens[0] != 10 || evens[1] != 30 || evens[2] != 50)
    {
        return false;
    }

    const gp::ConstStridedView<gp::Int32> rev = evens.reverseStrided();
    if (rev.size() != 3u || rev[0] != 50 || rev[1] != 30 || rev[2] != 10)
    {
        return false;
    }

    const gp::Int32 constantValue = 42;
    const gp::ConstStridedView<gp::Int32> broadcast =
        gp::ConstStridedView<gp::Int32>::broadcast(constantValue, 5u);
    gp::Int32 sum = 0;
    for (const gp::Int32 v : broadcast)
    {
        sum += v;
    }
    return sum == 210;
}

static_assert(testConstexprStridedView(), "Constexpr gp::StridedView evaluation failed.");

}   // namespace

template <typename InT>
class StridedViewTypedTest : public ::testing::Test
{};

using StridedNumericTypes = ::testing::Types<gp::Int32, gp::UInt64, gp::Float32, gp::Float64>;
TYPED_TEST_SUITE(StridedViewTypedTest, StridedNumericTypes);

TYPED_TEST(StridedViewTypedTest, ContiguousStepAndReverseIteration)
{
    using Elem = TypeParam;
    Elem arr[6] = {
        static_cast<Elem>(1),
        static_cast<Elem>(2),
        static_cast<Elem>(3),
        static_cast<Elem>(4),
        static_cast<Elem>(5),
        static_cast<Elem>(6)
    };

    gp::StridedView<Elem> view(arr);
    EXPECT_TRUE(view.isContiguous());
    EXPECT_EQ(view.size(), 6u);

    gp::StridedView<Elem> stepped = view.everyNth(2u);
    EXPECT_FALSE(stepped.isContiguous());
    ASSERT_EQ(stepped.size(), 3u);
    EXPECT_EQ(stepped[0], static_cast<Elem>(1));
    EXPECT_EQ(stepped[1], static_cast<Elem>(3));
    EXPECT_EQ(stepped[2], static_cast<Elem>(5));

    gp::StridedView<Elem> reversed = stepped.reverseStrided();
    ASSERT_EQ(reversed.size(), 3u);
    EXPECT_LT(reversed.stride(), 0);
    EXPECT_EQ(reversed[0], static_cast<Elem>(5));
    EXPECT_EQ(reversed[1], static_cast<Elem>(3));
    EXPECT_EQ(reversed[2], static_cast<Elem>(1));
}

TEST(StridedViewTests, FootprintAndRandomAccessIteratorConcepts)
{
    static_assert(sizeof(gp::StridedView<gp::Int32>) == 24u);
    static_assert(sizeof(gp::ConstStridedView<gp::Int32>) == 24u);
    static_assert(sizeof(gp::StridedView64<gp::Int32>) == 24u);
    static_assert(sizeof(gp::ConstStridedView64<gp::Int32>) == 24u);

    static_assert(gp::concepts::IsStridedView<gp::StridedView<gp::Int32>>);
    static_assert(gp::concepts::IsStridedView<gp::ConstStridedView<gp::Int32>>);
    static_assert(!gp::concepts::IsStridedView<gp::VectorView<gp::Int32>>);
    static_assert(gp::concepts::IsRelocatable<gp::StridedView<gp::Int32>>);

    static_assert(std::random_access_iterator<gp::StridedView<gp::Int32>::Iterator>);
    static_assert(std::random_access_iterator<gp::StridedView<gp::Int32>::ConstIterator>);
}

TEST(StridedViewTests, AoSMemberProjectionAndChainedNestedProjection)
{
    gp::Vector<EntityRecord> entities{
        { .id = 101u, .transform = { 1.0f, 2.0f, 3.0f }, .health = 100 },
        { .id = 102u, .transform = { 4.0f, 5.0f, 6.0f }, .health = 80 },
        { .id = 103u, .transform = { 7.0f, 8.0f, 9.0f }, .health = 60 },
        { .id = 104u, .transform = { 10.0f, 11.0f, 12.0f }, .health = 40 }
    };

    // Project mutable health field directly from AoS vector
    gp::StridedView<gp::Int32> healthView = gp::makeStridedView(entities, &EntityRecord::health);
    ASSERT_EQ(healthView.size(), 4u);
    EXPECT_EQ(healthView.stride(), static_cast<gp::ISize>(sizeof(EntityRecord)));
    EXPECT_EQ(healthView[0], 100);
    EXPECT_EQ(healthView[1], 80);
    EXPECT_EQ(healthView[2], 60);
    EXPECT_EQ(healthView[3], 40);

    // Mutate health in-place through the strided view
    for (gp::Int32& hp : healthView)
    {
        hp += 10;
    }
    EXPECT_EQ(entities[0].health, 110);
    EXPECT_EQ(entities[3].health, 50);

    // Chained projection: EntityRecord -> TransformData -> z component!
    gp::StridedView<gp::Float32> zCoords =
        gp::makeStridedView(entities, &EntityRecord::transform).project(&TransformData::z);
    ASSERT_EQ(zCoords.size(), 4u);
    EXPECT_EQ(zCoords.stride(), static_cast<gp::ISize>(sizeof(EntityRecord)));
    EXPECT_FLOAT_EQ(zCoords[0], 3.0f);
    EXPECT_FLOAT_EQ(zCoords[1], 6.0f);
    EXPECT_FLOAT_EQ(zCoords[2], 9.0f);
    EXPECT_FLOAT_EQ(zCoords[3], 12.0f);

    // Slice and reverse the projected AoS field view
    gp::ConstStridedView<gp::Float32> reversedMiddleZ = zCoords.slice(1u, 2u).reverseStrided();
    ASSERT_EQ(reversedMiddleZ.size(), 2u);
    EXPECT_FLOAT_EQ(reversedMiddleZ[0], 9.0f);
    EXPECT_FLOAT_EQ(reversedMiddleZ[1], 6.0f);
}

TEST(StridedViewTests, ZeroStrideBroadcastAndSearchOperations)
{
    gp::Int32 broadcastSource = 7;
    gp::StridedView<gp::Int32> broadcast = gp::StridedView<gp::Int32>::broadcast(broadcastSource, 8u);
    EXPECT_EQ(broadcast.size(), 8u);
    EXPECT_EQ(broadcast.stride(), 0);
    EXPECT_EQ(broadcast.count(7), 8u);
    EXPECT_TRUE(broadcast.contains(7));
    EXPECT_FALSE(broadcast.contains(8));

    auto beginIt = broadcast.begin();
    auto endIt = broadcast.end();
    EXPECT_EQ(endIt - beginIt, 8);
    EXPECT_LT(beginIt, endIt);
    EXPECT_EQ(beginIt[5], 7);

    // Compare a strided AoS projection against a contiguous StridedView
    EntityRecord arr[3] = {
        { .id = 1u, .transform = { 0.0f, 0.0f, 0.0f }, .health = 10 },
        { .id = 2u, .transform = { 0.0f, 0.0f, 0.0f }, .health = 20 },
        { .id = 3u, .transform = { 0.0f, 0.0f, 0.0f }, .health = 30 }
    };
    const gp::Int32 flatHealth[3] = { 10, 20, 30 };

    const gp::ConstStridedView<gp::Int32> projectedHealth = gp::makeStridedView(arr, &EntityRecord::health);
    const gp::ConstStridedView<gp::Int32> contiguousHealth(flatHealth);
    EXPECT_EQ(projectedHealth, contiguousHealth);
    EXPECT_EQ(projectedHealth.indexOf(20), 1u);
    EXPECT_EQ(projectedHealth.lastIndexOf(30), 2u);
}

}   // namespace gp::tests
