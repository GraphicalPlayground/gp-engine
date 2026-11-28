// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanViewportRegistry.hpp"
#include "CoreMinimal.hpp"
#include "presentation/VulkanPresentationForward.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Returns a distinct address standing for a viewport; the registry never dereferences it outside `releaseAll`.
/// @param[in] slot Index of the address.
/// @return Fake viewport pointer.
[[nodiscard]] VulkanViewport* makeFakeViewport(const gp::USize slot) noexcept
{
    static gp::UInt8 storage[64]{};
    return reinterpret_cast<VulkanViewport*>(&storage[slot]);
}

}   // namespace

TEST(VulkanViewportRegistryTest, StartsEmpty)
{
    VulkanViewportRegistry registry{};

    EXPECT_EQ(registry.getNumViewports(), 0u);
    EXPECT_FALSE(registry.contains(makeFakeViewport(0u)));
    EXPECT_EQ(registry.releaseAll(), 0u);
}

TEST(VulkanViewportRegistryTest, AddIgnoresNullAndDuplicates)
{
    VulkanViewportRegistry registry{};
    VulkanViewport* const viewport = makeFakeViewport(0u);

    registry.add(nullptr);
    EXPECT_EQ(registry.getNumViewports(), 0u);

    registry.add(viewport);
    registry.add(viewport);
    EXPECT_EQ(registry.getNumViewports(), 1u);
    EXPECT_TRUE(registry.contains(viewport));

    EXPECT_TRUE(registry.remove(viewport));
    EXPECT_EQ(registry.getNumViewports(), 0u);
}

TEST(VulkanViewportRegistryTest, RemoveReportsWhetherTheViewportWasRegistered)
{
    VulkanViewportRegistry registry{};
    VulkanViewport* const first = makeFakeViewport(0u);
    VulkanViewport* const second = makeFakeViewport(1u);
    VulkanViewport* const third = makeFakeViewport(2u);

    registry.add(first);
    registry.add(second);
    registry.add(third);
    EXPECT_EQ(registry.getNumViewports(), 3u);

    EXPECT_TRUE(registry.remove(second));
    EXPECT_FALSE(registry.remove(second));
    EXPECT_FALSE(registry.remove(nullptr));
    EXPECT_EQ(registry.getNumViewports(), 2u);
    EXPECT_TRUE(registry.contains(first));
    EXPECT_FALSE(registry.contains(second));
    EXPECT_TRUE(registry.contains(third));

    EXPECT_TRUE(registry.remove(first));
    EXPECT_TRUE(registry.remove(third));
    EXPECT_EQ(registry.getNumViewports(), 0u);
}

TEST(VulkanViewportRegistryConcurrencyTest, ConcurrentAddAndRemoveLeaveTheRegistryEmpty)
{
    constexpr gp::USize kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 2000u;

    VulkanViewportRegistry registry{};
    std::vector<std::thread> workers{};
    workers.reserve(kThreadCount);

    for (gp::USize threadIndex = 0u; threadIndex < kThreadCount; ++threadIndex)
    {
        workers.emplace_back(
            [&registry, threadIndex]() -> void
        {
            VulkanViewport* const viewport = makeFakeViewport(threadIndex);
            for (gp::UInt32 iteration = 0u; iteration < kIterationsPerThread; ++iteration)
            {
                registry.add(viewport);
                EXPECT_TRUE(registry.contains(viewport));
                EXPECT_TRUE(registry.remove(viewport));
            }
        }
        );
    }

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(registry.getNumViewports(), 0u);
}

}   // namespace gp::rhi::vulkan::tests
