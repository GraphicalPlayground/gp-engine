// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/utils/AllocatorUtilities.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::memory::tests
{

constexpr bool testConstexprSlackCalculations() noexcept
{
    const gp::USize shrinkZero = detail::defaultCalculateSlackShrink<gp::USize>(0u, 32u, sizeof(gp::UInt32), false, 8u);
    if (shrinkZero != 0u)
    {
        return false;
    }

    const gp::USize reserveExact = detail::defaultCalculateSlackReserve<gp::USize>(24u, sizeof(gp::UInt64), false, 8u);
    if (reserveExact != 24u)
    {
        return false;
    }

    const gp::USize growFirst = detail::defaultCalculateSlackGrow<gp::USize>(1u, 0u, sizeof(gp::UInt32), false, 8u);
    if (growFirst < 1u)
    {
        return false;
    }

    const gp::USize growNext = detail::defaultCalculateSlackGrow<gp::USize>(17u, 16u, sizeof(gp::UInt32), false, 8u);
    if (growNext <= 17u)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprSlackCalculations(), "Constexpr AllocatorUtilities slack evaluation failed.");
static_assert(std::is_empty_v<gp::UntypedContainerElement>, "UntypedContainerElement must be an empty tag struct.");

template <typename InSizeType>
class AllocatorUtilitiesTypedTest : public ::testing::Test
{};

using ContainerSizeTypes = ::testing::Types<gp::Int32, gp::UInt32, gp::Int64, gp::USize>;
TYPED_TEST_SUITE(AllocatorUtilitiesTypedTest, ContainerSizeTypes);

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackReserveWithoutQuantizationReturnsExactSize)
{
    const TypeParam requested = static_cast<TypeParam>(42);
    const TypeParam reserved =
        detail::defaultCalculateSlackReserve<TypeParam>(requested, sizeof(gp::UInt64), false, kDefaultAlignment);

    EXPECT_EQ(reserved, requested);
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackReserveWithQuantizationReturnsAtLeastRequestedSize)
{
    const TypeParam requested = static_cast<TypeParam>(19);
    const TypeParam reserved =
        detail::defaultCalculateSlackReserve<TypeParam>(requested, sizeof(gp::UInt32), true, 16u);

    EXPECT_GE(reserved, requested);
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackReserveZeroElementsReturnsZero)
{
    const TypeParam reservedNoQuant =
        detail::defaultCalculateSlackReserve<TypeParam>(static_cast<TypeParam>(0), sizeof(gp::UInt32), false);
    const TypeParam reservedQuant =
        detail::defaultCalculateSlackReserve<TypeParam>(static_cast<TypeParam>(0), sizeof(gp::UInt32), true);

    EXPECT_EQ(reservedNoQuant, static_cast<TypeParam>(0));
    EXPECT_EQ(reservedQuant, static_cast<TypeParam>(0));
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackGrowFromEmptyContainer)
{
    // Small initial request
    const TypeParam smallInitial = detail::defaultCalculateSlackGrow<TypeParam>(
        static_cast<TypeParam>(1), static_cast<TypeParam>(0), sizeof(gp::UInt32), false
    );
    EXPECT_GE(smallInitial, static_cast<TypeParam>(1));

    // Large initial request exceeding firstGrow threshold
    const TypeParam largeInitial = detail::defaultCalculateSlackGrow<TypeParam>(
        static_cast<TypeParam>(128), static_cast<TypeParam>(0), sizeof(gp::UInt32), false
    );
    EXPECT_GE(largeInitial, static_cast<TypeParam>(128));
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackGrowGeometricProgression)
{
    TypeParam capacity = static_cast<TypeParam>(0);
    for (TypeParam size = static_cast<TypeParam>(1); size <= static_cast<TypeParam>(256); ++size)
    {
        if (size > capacity)
        {
            const TypeParam nextCapacity =
                detail::defaultCalculateSlackGrow<TypeParam>(size, capacity, sizeof(gp::UInt64), true, 16u);
            EXPECT_GE(nextCapacity, size);
            EXPECT_GT(nextCapacity, capacity);
            capacity = nextCapacity;
        }
    }
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackShrinkToZeroAlwaysReclaimsAllSlack)
{
    const TypeParam shrunk = detail::defaultCalculateSlackShrink<TypeParam>(
        static_cast<TypeParam>(0), static_cast<TypeParam>(100), sizeof(gp::UInt32), true
    );

    EXPECT_EQ(shrunk, static_cast<TypeParam>(0));
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackShrinkRetainsCapacityForSmallSlack)
{
    // Only 10 elements of slack (< 64 elements and < 16 KB): must NOT reallocate/shrink
    const TypeParam currentCapacity = static_cast<TypeParam>(100);
    const TypeParam newSize = static_cast<TypeParam>(90);
    const TypeParam result =
        detail::defaultCalculateSlackShrink<TypeParam>(newSize, currentCapacity, sizeof(gp::UInt32), false);

    EXPECT_EQ(result, currentCapacity);
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackShrinkReclaimsWhenSlackExceedsOneThirdAnd64Elements)
{
    // Current capacity 300, new size 100 -> 200 slack elements (> 64 and 3*100 < 2*300)
    const TypeParam currentCapacity = static_cast<TypeParam>(300);
    const TypeParam newSize = static_cast<TypeParam>(100);

    const TypeParam unquantizedResult =
        detail::defaultCalculateSlackShrink<TypeParam>(newSize, currentCapacity, sizeof(gp::UInt32), false);
    EXPECT_EQ(unquantizedResult, newSize);

    const TypeParam quantizedResult =
        detail::defaultCalculateSlackShrink<TypeParam>(newSize, currentCapacity, sizeof(gp::UInt32), true);
    EXPECT_GE(quantizedResult, newSize);
    EXPECT_LT(quantizedResult, currentCapacity);
}

TYPED_TEST(AllocatorUtilitiesTypedTest, CalculateSlackShrinkReclaimsWhenSlackBytesExceed16KiB)
{
    // 100 slack elements * 256 bytes = 25,600 bytes (>= 16,384 bytes) and > 64 elements
    const TypeParam currentCapacity = static_cast<TypeParam>(500);
    const TypeParam newSize = static_cast<TypeParam>(400);

    const TypeParam shrunk = detail::defaultCalculateSlackShrink<TypeParam>(newSize, currentCapacity, 256u, false);
    EXPECT_EQ(shrunk, newSize);
}

}   // namespace gp::memory::tests
