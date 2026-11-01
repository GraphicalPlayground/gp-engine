// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/HeapAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::memory::tests
{

namespace
{

struct MockContainerWithHeapAllocator
{
    void* data{ nullptr };
    gp::USize size{ 0u };
    gp::USize capacity{ 0u };
    GP_NO_UNIQUE_ADDRESS HeapAllocator allocator{};
};

static_assert(
    sizeof(MockContainerWithHeapAllocator) == (sizeof(void*) + (2u * sizeof(gp::USize))),
    "Stateless HeapAllocator must occupy 0 bytes when embedded via GP_NO_UNIQUE_ADDRESS."
);

struct alignas(32) Aligned32Struct
{
    gp::UInt64 values[4]{};
};

}   // namespace

TEST(HeapAllocatorTest, ZeroSizeAndNullptrEdgeCases)
{
    HeapAllocator allocator{};

    EXPECT_EQ(allocator.allocate(0u), nullptr);
    EXPECT_EQ(allocator.allocate(0u, 64u), nullptr);
    EXPECT_EQ(allocator.reallocate(nullptr, 0u, 0u, 16u), nullptr);
    EXPECT_EQ(allocator.getActualAllocationSize(0u, 16u), 0u);
    EXPECT_FALSE(allocator.owns(nullptr));

    // Deallocating nullptr must be a safe no-op
    allocator.deallocate(nullptr, 0u, 16u);
}

TEST(HeapAllocatorTest, AllocateReallocateGrowShrinkAndDeallocate)
{
    HeapAllocator allocator{};

    void* block = allocator.allocate(64u, 16u);
    ASSERT_NE(block, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(block, 16u));
    EXPECT_TRUE(allocator.owns(block));
    EXPECT_GE(allocator.getActualAllocationSize(64u, 16u), 64u);

    gp::UInt8* bytes = static_cast<gp::UInt8*>(block);
    for (gp::USize i = 0u; i < 64u; ++i)
    {
        bytes[i] = static_cast<gp::UInt8>(i + 1u);
    }

    // Grow allocation to 512 bytes
    void* grown = allocator.reallocate(block, 64u, 512u, 16u);
    ASSERT_NE(grown, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(grown, 16u));

    const gp::UInt8* grownBytes = static_cast<const gp::UInt8*>(grown);
    for (gp::USize i = 0u; i < 64u; ++i)
    {
        EXPECT_EQ(grownBytes[i], static_cast<gp::UInt8>(i + 1u));
    }

    // Shrink allocation to 32 bytes
    void* shrunk = allocator.reallocate(grown, 512u, 32u, 16u);
    ASSERT_NE(shrunk, nullptr);
    const gp::UInt8* shrunkBytes = static_cast<const gp::UInt8*>(shrunk);
    for (gp::USize i = 0u; i < 32u; ++i)
    {
        EXPECT_EQ(shrunkBytes[i], static_cast<gp::UInt8>(i + 1u));
    }

    // Reallocate with newSizeInBytes == 0 frees and returns nullptr
    void* freed = allocator.reallocate(shrunk, 32u, 0u, 16u);
    EXPECT_EQ(freed, nullptr);
}

TEST(HeapAllocatorTest, ReallocateFromNullptrAllocatesFreshBlock)
{
    HeapAllocator allocator{};

    void* fresh = allocator.reallocate(nullptr, 0u, 128u, 32u);
    ASSERT_NE(fresh, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(fresh, 32u));

    allocator.deallocate(fresh, 128u, 32u);
}

TEST(HeapAllocatorTest, AllocateAndDeallocateElementsTypedHelpers)
{
    HeapAllocator allocator{};

    EXPECT_EQ(allocator.allocateElements<Aligned32Struct>(0u), nullptr);

    Aligned32Struct* elements = allocator.allocateElements<Aligned32Struct>(8u);
    ASSERT_NE(elements, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(elements, alignof(Aligned32Struct)));

    for (gp::USize i = 0u; i < 8u; ++i)
    {
        elements[i].values[0] = i * 100ull;
    }
    EXPECT_EQ(elements[7].values[0], 700ull);

    allocator.deallocateElements(elements, 8u);
}

TEST(HeapAllocatorTest, StatelessEqualityComparison)
{
    constexpr HeapAllocator allocA{};
    constexpr DefaultAllocator allocB{};
    static_assert(allocA == allocB, "Stateless HeapAllocator instances must compare equal.");
    EXPECT_EQ(allocA, allocB);
}

}   // namespace gp::memory::tests
