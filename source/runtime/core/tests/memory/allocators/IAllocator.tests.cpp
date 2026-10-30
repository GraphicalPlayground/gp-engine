// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/IAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::memory::tests
{

namespace
{

/// @brief Custom `IAllocator` test double recording every virtual call for contract verification.
class RecordingAllocator final : public IAllocator
{
public:
    gp::UInt32 allocateCalls{ 0u };
    gp::UInt32 reallocateCalls{ 0u };
    gp::UInt32 deallocateCalls{ 0u };
    gp::USize lastRequestedSize{ 0u };
    gp::UInt32 lastAlignment{ 0u };
    alignas(64) gp::UInt8 backingStorage[512]{};

    [[nodiscard]] virtual void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept override
    {
        ++allocateCalls;
        lastRequestedSize = sizeInBytes;
        lastAlignment = alignment;
        if (sizeInBytes == 0u || sizeInBytes > sizeof(backingStorage))
        {
            return nullptr;
        }
        return backingStorage;
    }

    [[nodiscard]] virtual void* reallocate(
        void* ptr,
        const gp::USize /*oldSizeInBytes*/,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept override
    {
        ++reallocateCalls;
        lastRequestedSize = newSizeInBytes;
        lastAlignment = alignment;
        if (newSizeInBytes == 0u)
        {
            deallocate(ptr, 0u, alignment);
            return nullptr;
        }
        return backingStorage;
    }

    virtual void deallocate(
        void* /*ptr*/, const gp::USize /*oldSizeInBytes*/ = 0u, const gp::UInt32 /*alignment*/ = kDefaultAlignment
    ) noexcept override
    {
        ++deallocateCalls;
    }

    [[nodiscard]] virtual gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 /*alignment*/ = kDefaultAlignment
    ) const noexcept override
    {
        return requestedSize <= sizeof(backingStorage) ? sizeof(backingStorage) : 0u;
    }

    [[nodiscard]] virtual bool owns(const void* ptr) const noexcept override
    {
        return ptr == static_cast<const void*>(backingStorage);
    }

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "RecordingAllocator";
    }
};

}   // namespace

TEST(IAllocatorTest, VirtualDispatchAndDefaultArgumentsContract)
{
    RecordingAllocator recorder{};
    IAllocator& interfaceRef = recorder;

    EXPECT_STREQ(interfaceRef.getName(), "RecordingAllocator");

    void* ptr = interfaceRef.allocate(64u);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(recorder.allocateCalls, 1u);
    EXPECT_EQ(recorder.lastRequestedSize, 64u);
    EXPECT_EQ(recorder.lastAlignment, kDefaultAlignment);
    EXPECT_TRUE(interfaceRef.owns(ptr));
    EXPECT_EQ(interfaceRef.getActualAllocationSize(64u), 512u);

    ptr = interfaceRef.reallocate(ptr, 64u, 128u, 32u);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(recorder.reallocateCalls, 1u);
    EXPECT_EQ(recorder.lastRequestedSize, 128u);
    EXPECT_EQ(recorder.lastAlignment, 32u);

    interfaceRef.deallocate(ptr, 128u);
    EXPECT_EQ(recorder.deallocateCalls, 1u);
}

}   // namespace gp::memory::tests
