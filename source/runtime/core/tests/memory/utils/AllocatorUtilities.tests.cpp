// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/utils/AllocatorUtilities.hpp"
#include "memory/allocators/FixedBufferAllocator.hpp"
#include "memory/allocators/PoolAllocator.hpp"
#include <gtest/gtest.h>
#include <limits>
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

    // Regression: Large Int32 values (> INT32_MAX / 2) must not trigger signed 32-bit multiplication overflow UB!
    const gp::Int32 largeCurrent = 1'200'000'000;
    const gp::Int32 largeNew = 1'199'999'960;
    const gp::Int32 shrinkLarge =
        detail::defaultCalculateSlackShrink<gp::Int32>(largeNew, largeCurrent, 1u, false, 8u);
    if (shrinkLarge != largeCurrent)
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

TEST(AllocatorUtilitiesTest, GrowClampsToSizeTypeMaxInsteadOfWrappingOn32BitSizeTypes)
{
    // For Int32, requesting growth from 1,800,000,000 elements (1.375x = 2,475,000,000 > INT32_MAX)
    // must clamp to INT32_MAX instead of wrapping negative or truncating.
    const gp::Int32 currentCap = 1'600'000'000;
    const gp::Int32 requested = 1'800'000'000;
    const gp::Int32 grown = detail::defaultCalculateSlackGrow<gp::Int32>(requested, currentCap, 4u, false);
    EXPECT_EQ(grown, std::numeric_limits<gp::Int32>::max());

    // For UInt32, requesting growth from 3,500,000,000 elements (1.375x = 4,812,500,000 > UINT32_MAX)
    // where modulo 2^32 wrap would otherwise land on ~517,532,704, must clamp to UINT32_MAX.
    const gp::UInt32 uCurrentCap = 3'200'000'000u;
    const gp::UInt32 uRequested = 3'500'000'000u;
    const gp::UInt32 uGrown = detail::defaultCalculateSlackGrow<gp::UInt32>(uRequested, uCurrentCap, 4u, false);
    EXPECT_EQ(uGrown, std::numeric_limits<gp::UInt32>::max());
}

TEST(AllocatorUtilitiesTest, AllocatorAwareSlackFunctionsRespectBoundAllocatorQuantization)
{
    FixedBufferAllocator<60u, 16u> fixedArena{};
    // 13 elements of 4 bytes = 52 bytes -> FixedBufferAllocator does not overround past 60 bytes
    const gp::USize reservedInFixed =
        calculateSlackReserve<gp::USize>(fixedArena, 13u, sizeof(gp::UInt32), true, 16u);
    EXPECT_EQ(reservedInFixed, 13u);

    PoolAllocator pool(64u, 8u, 16u, nullptr, "Test.SlackPool");
    // 5 elements of 4 bytes = 20 bytes -> PoolAllocator has 64-byte fixed blocks, so quantized capacity is 64 / 4 = 16!
    const gp::USize reservedInPool =
        calculateSlackReserve<gp::USize>(pool, 5u, sizeof(gp::UInt32), true, 16u);
    EXPECT_EQ(reservedInPool, 16u);
}

}   // namespace gp::memory::tests
