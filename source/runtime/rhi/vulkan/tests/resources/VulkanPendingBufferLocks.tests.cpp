// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanPendingBufferLocks.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "resources/VulkanBufferTypes.hpp"
#include "VulkanForward.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Fake buffer identity; the registry only compares pointers.
[[nodiscard]] const VulkanBuffer* fakeBuffer(const gp::UIntPtr id) noexcept
{
    return reinterpret_cast<const VulkanBuffer*>(id * 64u);
}

}   // namespace

TEST(VulkanPendingBufferLockRegistryTest, StartsEmpty)
{
    VulkanPendingBufferLockRegistry registry{};

    EXPECT_EQ(registry.size(), 0u);
    EXPECT_FALSE(registry.contains(fakeBuffer(1u)));

    VulkanPendingBufferLock pendingLock{};
    EXPECT_FALSE(registry.take(fakeBuffer(1u), pendingLock));
}

TEST(VulkanPendingBufferLockRegistryTest, AddThenTakeRoundTripsRecord)
{
    VulkanPendingBufferLockRegistry registry{};

    VulkanPendingBufferLock added{};
    added.offset = 128u;
    added.size = 512u;
    added.lockMode = resource::ResourceLockMode::WriteOnly;
    added.isFirstLock = true;
    registry.add(fakeBuffer(1u), added);

    EXPECT_EQ(registry.size(), 1u);
    EXPECT_TRUE(registry.contains(fakeBuffer(1u)));

    VulkanPendingBufferLock taken{};
    ASSERT_TRUE(registry.take(fakeBuffer(1u), taken));
    EXPECT_EQ(taken.offset, 128u);
    EXPECT_EQ(taken.size, 512u);
    EXPECT_EQ(taken.lockMode, resource::ResourceLockMode::WriteOnly);
    EXPECT_TRUE(taken.isFirstLock);

    EXPECT_EQ(registry.size(), 0u);
    EXPECT_FALSE(registry.contains(fakeBuffer(1u)));
    EXPECT_FALSE(registry.take(fakeBuffer(1u), taken));
}

TEST(VulkanPendingBufferLockRegistryTest, KeepsRecordsOfDifferentBuffersApart)
{
    VulkanPendingBufferLockRegistry registry{};

    for (gp::UIntPtr id = 1u; id <= 4u; ++id)
    {
        VulkanPendingBufferLock pendingLock{};
        pendingLock.size = static_cast<gp::UInt32>(id * 100u);
        registry.add(fakeBuffer(id), pendingLock);
    }
    EXPECT_EQ(registry.size(), 4u);

    VulkanPendingBufferLock taken{};
    ASSERT_TRUE(registry.take(fakeBuffer(2u), taken));
    EXPECT_EQ(taken.size, 200u);
    EXPECT_EQ(registry.size(), 3u);

    // Swap-removal must not lose the moved record.
    ASSERT_TRUE(registry.take(fakeBuffer(4u), taken));
    EXPECT_EQ(taken.size, 400u);
    ASSERT_TRUE(registry.take(fakeBuffer(1u), taken));
    EXPECT_EQ(taken.size, 100u);
    ASSERT_TRUE(registry.take(fakeBuffer(3u), taken));
    EXPECT_EQ(taken.size, 300u);
    EXPECT_EQ(registry.size(), 0u);
}

TEST(VulkanPendingBufferLockRegistryTest, GlobalRegistryIsSingleton)
{
    EXPECT_EQ(&getVulkanPendingBufferLocks(), &getVulkanPendingBufferLocks());
}

TEST(VulkanPendingBufferLockRegistryTest, ConcurrentAddAndTakeKeepEveryRecord)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kBuffersPerThread = 256u;

    VulkanPendingBufferLockRegistry registry{};
    std::vector<std::thread> workers{};
    workers.reserve(kThreadCount);

    for (gp::UInt32 threadIndex = 0u; threadIndex < kThreadCount; ++threadIndex)
    {
        workers.emplace_back([&registry, threadIndex]()
        {
            for (gp::UInt32 index = 0u; index < kBuffersPerThread; ++index)
            {
                const gp::UIntPtr id = 1u + (static_cast<gp::UIntPtr>(threadIndex) * kBuffersPerThread) + index;
                VulkanPendingBufferLock pendingLock{};
                pendingLock.offset = static_cast<gp::UInt32>(id);
                registry.add(fakeBuffer(id), pendingLock);
            }
        });
    }
    for (std::thread& worker: workers)
    {
        worker.join();
    }

    ASSERT_EQ(registry.size(), static_cast<gp::USize>(kThreadCount) * kBuffersPerThread);

    for (gp::UIntPtr id = 1u; id <= static_cast<gp::UIntPtr>(kThreadCount) * kBuffersPerThread; ++id)
    {
        VulkanPendingBufferLock taken{};
        ASSERT_TRUE(registry.take(fakeBuffer(id), taken));
        EXPECT_EQ(taken.offset, static_cast<gp::UInt32>(id));
    }
    EXPECT_EQ(registry.size(), 0u);
}

}   // namespace gp::rhi::vulkan::tests

