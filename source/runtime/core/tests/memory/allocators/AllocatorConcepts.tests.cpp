// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/AlignedHeapAllocator.hpp"
#include "memory/allocators/FixedBufferAllocator.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/allocators/InlineAllocator.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/allocators/PolymorphicAllocator.hpp"
#include "memory/allocators/PoolAllocator.hpp"
#include "memory/allocators/StackAllocator.hpp"
#include "memory/allocators/TaggedAllocator.hpp"
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include <gtest/gtest.h>

namespace gp::memory::tests
{

namespace
{

struct NotAnAllocator
{
    gp::Int32 dummy{ 0 };
};

/// @brief Missing `noexcept` on `allocate`, must be rejected by `IsAllocator`.
struct ThrowingAllocatorMock
{
    void* allocate(const gp::USize, const gp::UInt32)
    {
        return nullptr;
    }

    void* reallocate(void*, const gp::USize, const gp::USize, const gp::UInt32) noexcept
    {
        return nullptr;
    }

    void deallocate(void*, const gp::USize, const gp::UInt32) noexcept
    {}

    gp::USize getActualAllocationSize(const gp::USize, const gp::UInt32) const noexcept
    {
        return 0u;
    }

    bool owns(const void*) const noexcept
    {
        return false;
    }
};

/// @brief Missing `owns(const void*)`, must be rejected by `IsAllocator`.
struct IncompleteAllocatorMock
{
    void* allocate(const gp::USize, const gp::UInt32) noexcept
    {
        return nullptr;
    }

    void* reallocate(void*, const gp::USize, const gp::USize, const gp::UInt32) noexcept
    {
        return nullptr;
    }

    void deallocate(void*, const gp::USize, const gp::UInt32) noexcept
    {}

    gp::USize getActualAllocationSize(const gp::USize, const gp::UInt32) const noexcept
    {
        return 0u;
    }
};

}   // namespace

TEST(AllocatorConceptsTest, IsAllocatorAcceptsValidEngineAllocatorsAndRejectsInvalidTypes)
{
    EXPECT_TRUE(gp::concepts::IsAllocator<HeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<DefaultAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<Simd16HeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<Simd32HeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<CacheAlignedHeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<LinearAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<StackAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<PoolAllocator>);
    EXPECT_TRUE((gp::concepts::IsAllocator<FixedBufferAllocator<128u, 16u>>));
    EXPECT_TRUE((gp::concepts::IsAllocator<InlineAllocator<128u, 16u, HeapAllocator>>));
    EXPECT_TRUE(gp::concepts::IsAllocator<ThreadScratchAllocator>);
    EXPECT_TRUE(gp::concepts::IsAllocator<TaggedAllocator<HeapAllocator>>);
    EXPECT_TRUE(gp::concepts::IsAllocator<PolymorphicAllocator>);

    EXPECT_FALSE(gp::concepts::IsAllocator<void>);
    EXPECT_FALSE(gp::concepts::IsAllocator<gp::Int32>);
    EXPECT_FALSE(gp::concepts::IsAllocator<NotAnAllocator>);
    EXPECT_FALSE(gp::concepts::IsAllocator<ThrowingAllocatorMock>);
    EXPECT_FALSE(gp::concepts::IsAllocator<IncompleteAllocatorMock>);
}

TEST(AllocatorConceptsTest, IsStatelessAllocatorDistinguishesEmptyFromStatefulAllocators)
{
    EXPECT_TRUE(gp::concepts::IsStatelessAllocator<HeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsStatelessAllocator<Simd16HeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsStatelessAllocator<Simd32HeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsStatelessAllocator<CacheAlignedHeapAllocator>);
    EXPECT_TRUE(gp::concepts::IsStatelessAllocator<ThreadScratchAllocator>);

    EXPECT_FALSE(gp::concepts::IsStatelessAllocator<LinearAllocator>);
    EXPECT_FALSE(gp::concepts::IsStatelessAllocator<StackAllocator>);
    EXPECT_FALSE(gp::concepts::IsStatelessAllocator<PoolAllocator>);
    EXPECT_FALSE(gp::concepts::IsStatelessAllocator<FixedBufferAllocator<64u>>);
    EXPECT_FALSE(gp::concepts::IsStatelessAllocator<InlineAllocator<64u>>);
    EXPECT_FALSE(gp::concepts::IsStatelessAllocator<TaggedAllocator<HeapAllocator>>);
    EXPECT_FALSE(gp::concepts::IsStatelessAllocator<PolymorphicAllocator>);
}

TEST(AllocatorConceptsTest, IsMarkableAllocatorDistinguishesBookmarkableArenas)
{
    EXPECT_TRUE(gp::concepts::IsMarkableAllocator<LinearAllocator>);
    EXPECT_TRUE(gp::concepts::IsMarkableAllocator<StackAllocator>);
    EXPECT_TRUE(gp::concepts::IsMarkableAllocator<FixedBufferAllocator<256u>>);
    EXPECT_TRUE(gp::concepts::IsMarkableAllocator<ThreadScratchAllocator>);

    EXPECT_FALSE(gp::concepts::IsMarkableAllocator<HeapAllocator>);
    EXPECT_FALSE(gp::concepts::IsMarkableAllocator<Simd16HeapAllocator>);
    EXPECT_FALSE(gp::concepts::IsMarkableAllocator<PoolAllocator>);
    EXPECT_FALSE(gp::concepts::IsMarkableAllocator<InlineAllocator<128u>>);
    EXPECT_FALSE(gp::concepts::IsMarkableAllocator<TaggedAllocator<HeapAllocator>>);
    EXPECT_FALSE(gp::concepts::IsMarkableAllocator<PolymorphicAllocator>);
}

}   // namespace gp::memory::tests
