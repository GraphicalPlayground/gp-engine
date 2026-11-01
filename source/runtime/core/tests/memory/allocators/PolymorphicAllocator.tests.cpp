// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PolymorphicAllocator.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/allocators/PoolAllocator.hpp"
#include "memory/allocators/StackAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::memory::tests
{

namespace
{

/// @brief Type-erased subsystem function accepting any runtime `IAllocator` via `PolymorphicAllocator`.
void exerciseTypeErasedAllocator(PolymorphicAllocator allocator, const gp::USize initialSize, const gp::USize grownSize)
{
    void* ptr = allocator.allocate(initialSize, 16u);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(ptr, 16u));
    EXPECT_TRUE(allocator.owns(ptr));
    EXPECT_GE(allocator.getActualAllocationSize(initialSize, 16u), initialSize);

    gp::UInt8* bytes = static_cast<gp::UInt8*>(ptr);
    for (gp::USize i = 0u; i < initialSize; ++i)
    {
        bytes[i] = static_cast<gp::UInt8>(0xC0u + (i & 0x1Fu));
    }

    void* resized = allocator.reallocate(ptr, initialSize, grownSize, 16u);
    ASSERT_NE(resized, nullptr);
    EXPECT_TRUE(allocator.owns(resized));

    const gp::UInt8* resizedBytes = static_cast<const gp::UInt8*>(resized);
    for (gp::USize i = 0u; i < initialSize; ++i)
    {
        EXPECT_EQ(resizedBytes[i], static_cast<gp::UInt8>(0xC0u + (i & 0x1Fu)));
    }

    allocator.deallocate(resized, grownSize, 16u);
}

}   // namespace

TEST(PolymorphicAllocatorTest, DefaultAndNullptrConstructionBindToDefaultHeapResource)
{
    PolymorphicAllocator defaultPoly{};
    PolymorphicAllocator nullConstructedPoly(static_cast<IAllocator*>(nullptr));

    EXPECT_NE(defaultPoly.getResource(), nullptr);
    EXPECT_EQ(defaultPoly.getResource(), &PolymorphicAllocator::getDefaultResource());
    EXPECT_EQ(defaultPoly, nullConstructedPoly);
    EXPECT_STREQ(defaultPoly.getName(), "PolymorphicAllocator.DefaultHeap");

    exerciseTypeErasedAllocator(defaultPoly, 32u, 128u);
}

TEST(PolymorphicAllocatorTest, DispatchesPolymorphicallyAcrossLinearStackAndPoolArenas)
{
    LinearAllocator linearArena(2048u, nullptr, "Poly.Linear");
    StackAllocator stackArena(2048u, nullptr, "Poly.Stack");
    PoolAllocator poolArena(128u, 8u, 16u, nullptr, "Poly.Pool");

    PolymorphicAllocator polyLinear(linearArena);
    PolymorphicAllocator polyStack(stackArena);
    PolymorphicAllocator polyPool(&poolArena);

    EXPECT_NE(polyLinear, polyStack);
    EXPECT_NE(polyStack, polyPool);
    EXPECT_STREQ(polyLinear.getName(), "Poly.Linear");
    EXPECT_STREQ(polyStack.getName(), "Poly.Stack");
    EXPECT_STREQ(polyPool.getName(), "Poly.Pool");

    exerciseTypeErasedAllocator(polyLinear, 32u, 64u);
    exerciseTypeErasedAllocator(polyStack, 32u, 64u);
    exerciseTypeErasedAllocator(polyPool, 32u, 64u);

    // Because each exercise deallocated its top/pool block, all 3 arenas returned to 0 used bytes!
    EXPECT_EQ(linearArena.getUsedBytes(), 0u);
    EXPECT_EQ(stackArena.getUsedBytes(), 0u);
    EXPECT_EQ(poolArena.getUsedBlockCount(), 0u);
}

}   // namespace gp::memory::tests
