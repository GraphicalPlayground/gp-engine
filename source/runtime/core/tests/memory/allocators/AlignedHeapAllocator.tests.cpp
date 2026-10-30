// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/AlignedHeapAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::memory::tests
{

namespace
{

template <gp::UInt32 InAlignment>
struct AlignmentTag
{
    static constexpr gp::UInt32 kValue = InAlignment;
};

}   // namespace

template <typename InTag>
class AlignedHeapAllocatorTypedTest : public ::testing::Test
{};

using SupportedAlignments = ::testing::Types<
    AlignmentTag<8u>,
    AlignmentTag<16u>,
    AlignmentTag<32u>,
    AlignmentTag<64u>,
    AlignmentTag<128u>,
    AlignmentTag<256u>>;
TYPED_TEST_SUITE(AlignedHeapAllocatorTypedTest, SupportedAlignments);

TYPED_TEST(AlignedHeapAllocatorTypedTest, EnforcesMinimumGuaranteedAlignmentAcrossVariedSizes)
{
    constexpr gp::UInt32 kAlign = TypeParam::kValue;
    AlignedHeapAllocator<kAlign> allocator{};

    EXPECT_EQ(AlignedHeapAllocator<kAlign>::kGuaranteedAlignment, kAlign);
    EXPECT_EQ(allocator.allocate(0u), nullptr);
    EXPECT_FALSE(allocator.owns(nullptr));

    // Even when caller passes a smaller alignment (e.g. 4 or 8), AlignedHeapAllocator must enforce kAlign
    const gp::USize testSizes[5] = { 1u, 13u, 37u, 129u, 1024u };
    for (const gp::USize size: testSizes)
    {
        void* ptr = allocator.allocate(size, 4u);
        ASSERT_NE(ptr, nullptr);
        EXPECT_TRUE(gp::memory::isAligned(ptr, static_cast<gp::UInt64>(kAlign)));
        EXPECT_TRUE(allocator.owns(ptr));
        EXPECT_GE(allocator.getActualAllocationSize(size, 4u), size);

        allocator.deallocate(ptr, size, 4u);
    }
}

TYPED_TEST(AlignedHeapAllocatorTypedTest, ReallocatePreservesAlignmentAndPayload)
{
    constexpr gp::UInt32 kAlign = TypeParam::kValue;
    AlignedHeapAllocator<kAlign> allocator{};

    void* ptr = allocator.allocate(48u, 8u);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(ptr, static_cast<gp::UInt64>(kAlign)));

    gp::UInt8* bytes = static_cast<gp::UInt8*>(ptr);
    for (gp::USize i = 0u; i < 48u; ++i)
    {
        bytes[i] = static_cast<gp::UInt8>(0x30u + i);
    }

    // Grow while requesting even stricter alignment (kAlign * 2)
    const gp::UInt32 stricterAlignment = kAlign * 2u;
    void* grown = allocator.reallocate(ptr, 48u, 300u, stricterAlignment);
    ASSERT_NE(grown, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(grown, static_cast<gp::UInt64>(stricterAlignment)));

    const gp::UInt8* grownBytes = static_cast<const gp::UInt8*>(grown);
    for (gp::USize i = 0u; i < 48u; ++i)
    {
        EXPECT_EQ(grownBytes[i], static_cast<gp::UInt8>(0x30u + i));
    }

    // Reallocate to 0 frees and returns nullptr
    EXPECT_EQ(allocator.reallocate(grown, 300u, 0u, stricterAlignment), nullptr);
}

TEST(AlignedHeapAllocatorTest, CanonicalSimdAndCacheLineAliases)
{
    static_assert(Simd16HeapAllocator::kGuaranteedAlignment == 16u);
    static_assert(Simd32HeapAllocator::kGuaranteedAlignment == 32u);
    static_assert(
        CacheAlignedHeapAllocator::kGuaranteedAlignment == static_cast<gp::UInt32>(HardwareConstants::cacheLineSize)
    );

    Simd16HeapAllocator a16{};
    Simd16HeapAllocator b16{};
    EXPECT_EQ(a16, b16);

    // Verify owns() rejects misaligned addresses
    alignas(64) gp::UInt8 rawBuffer[128]{};
    CacheAlignedHeapAllocator cacheAlloc{};
    EXPECT_TRUE(cacheAlloc.owns(&rawBuffer[0]));
    EXPECT_FALSE(cacheAlloc.owns(&rawBuffer[1]));
}

}   // namespace gp::memory::tests
