// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanPendingTextureLocks.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "resources/VulkanTextureTypes.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Storage whose addresses stand in for textures: the registry only compares pointers.
gp::UInt8 g_textureTokens[8]{};

[[nodiscard]] const VulkanTexture* fakeTexture(const gp::USize index) noexcept
{
    return reinterpret_cast<const VulkanTexture*>(&g_textureTokens[index]);
}

[[nodiscard]] VulkanPendingTextureLock
    makeLock(const gp::UInt32 mipIndex, const gp::UInt32 arrayLayer, const resource::ResourceLockMode mode) noexcept
{
    VulkanPendingTextureLock pendingLock{};
    pendingLock.mipIndex = mipIndex;
    pendingLock.arrayLayer = arrayLayer;
    pendingLock.lockMode = mode;
    return pendingLock;
}

}   // namespace

TEST(VulkanPendingTextureLocksTest, StartsEmpty)
{
    VulkanPendingTextureLockRegistry registry{};
    VulkanPendingTextureLock pendingLock{};

    EXPECT_EQ(registry.size(), 0u);
    EXPECT_FALSE(registry.contains(fakeTexture(0u), 0u, 0u));
    EXPECT_FALSE(registry.take(fakeTexture(0u), 0u, 0u, pendingLock));
}

TEST(VulkanPendingTextureLocksTest, TakeReturnsTheRecordOnce)
{
    VulkanPendingTextureLockRegistry registry{};
    registry.add(fakeTexture(0u), makeLock(2u, 3u, resource::ResourceLockMode::ReadOnly));
    EXPECT_TRUE(registry.contains(fakeTexture(0u), 2u, 3u));

    VulkanPendingTextureLock pendingLock{};
    ASSERT_TRUE(registry.take(fakeTexture(0u), 2u, 3u, pendingLock));
    EXPECT_EQ(pendingLock.mipIndex, 2u);
    EXPECT_EQ(pendingLock.arrayLayer, 3u);
    EXPECT_EQ(pendingLock.lockMode, resource::ResourceLockMode::ReadOnly);

    EXPECT_FALSE(registry.take(fakeTexture(0u), 2u, 3u, pendingLock));
    EXPECT_EQ(registry.size(), 0u);
}

TEST(VulkanPendingTextureLocksTest, SubresourcesAreKeyedIndependently)
{
    VulkanPendingTextureLockRegistry registry{};
    registry.add(fakeTexture(0u), makeLock(0u, 0u, resource::ResourceLockMode::WriteOnly));
    registry.add(fakeTexture(0u), makeLock(1u, 0u, resource::ResourceLockMode::ReadOnly));
    registry.add(fakeTexture(0u), makeLock(0u, 1u, resource::ResourceLockMode::WriteOnly));
    registry.add(fakeTexture(1u), makeLock(0u, 0u, resource::ResourceLockMode::ReadOnly));
    EXPECT_EQ(registry.size(), 4u);

    VulkanPendingTextureLock pendingLock{};
    ASSERT_TRUE(registry.take(fakeTexture(0u), 1u, 0u, pendingLock));
    EXPECT_EQ(pendingLock.lockMode, resource::ResourceLockMode::ReadOnly);

    EXPECT_TRUE(registry.contains(fakeTexture(0u), 0u, 0u));
    EXPECT_TRUE(registry.contains(fakeTexture(0u), 0u, 1u));
    EXPECT_TRUE(registry.contains(fakeTexture(1u), 0u, 0u));
    EXPECT_FALSE(registry.contains(fakeTexture(1u), 1u, 0u));
    EXPECT_EQ(registry.size(), 3u);
}

TEST(VulkanPendingTextureLocksTest, GlobalRegistryIsShared)
{
    EXPECT_EQ(&getVulkanPendingTextureLocks(), &getVulkanPendingTextureLocks());
}

TEST(VulkanPendingTextureLocksTest, ConcurrentAddAndTakeBalance)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 2000u;

    VulkanPendingTextureLockRegistry registry{};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    std::vector<gp::UInt32> failures(kThreadCount, 0u);

    for (gp::UInt32 threadIndex = 0u; threadIndex < kThreadCount; ++threadIndex)
    {
        workers.emplace_back([&registry, &failures, threadIndex]()
        {
            const VulkanTexture* const texture = fakeTexture(threadIndex);
            for (gp::UInt32 iteration = 0u; iteration < kIterationsPerThread; ++iteration)
            {
                registry.add(texture, makeLock(iteration % 4u, 0u, resource::ResourceLockMode::WriteOnly));
                VulkanPendingTextureLock pendingLock{};
                if (!registry.take(texture, iteration % 4u, 0u, pendingLock))
                {
                    ++failures[threadIndex];
                }
            }
        });
    }

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    for (const gp::UInt32 failureCount: failures)
    {
        EXPECT_EQ(failureCount, 0u);
    }
    EXPECT_EQ(registry.size(), 0u);
}

}   // namespace gp::rhi::vulkan::tests
