// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanMemoryStatsTranslation.hpp"
#include "CoreMinimal.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "profiling/RHIStats.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief One gibibyte.
constexpr gp::UInt64 kGiB = 1024ull * 1024ull * 1024ull;

}   // namespace

TEST(VulkanMemoryStatsTranslationTest, EmptySnapshotTranslatesToZeroes)
{
    const profiling::RHIMemoryStats stats = makeVulkanRHIMemoryStats(VulkanMemoryStats{}, 0u, 0u);

    EXPECT_EQ(stats.dedicatedVideoMemory, 0u);
    EXPECT_EQ(stats.dedicatedSystemMemory, 0u);
    EXPECT_EQ(stats.sharedSystemMemory, 0u);
    EXPECT_EQ(stats.totalGraphicsMemory, 0u);
    EXPECT_EQ(stats.budgetVideoMemory, 0u);
    EXPECT_EQ(stats.usedVideoMemory, 0u);
    EXPECT_EQ(stats.availableVideoMemory, 0u);
    EXPECT_EQ(stats.demotedVideoMemory, 0u);
    EXPECT_FALSE(stats.isOverBudget());
}

TEST(VulkanMemoryStatsTranslationTest, DeviceLocalHeapsBecomeVideoMemory)
{
    VulkanMemoryStats snapshot{};
    snapshot.budgetLocal = 7u * kGiB;
    snapshot.usedLocal = 2u * kGiB;
    snapshot.availableLocal = 5u * kGiB;
    snapshot.budgetSystem = 15u * kGiB;
    snapshot.usedSystem = 1u * kGiB;
    snapshot.availableSystem = 14u * kGiB;

    const profiling::RHIMemoryStats stats = makeVulkanRHIMemoryStats(snapshot, 8u * kGiB, 16u * kGiB);

    EXPECT_EQ(stats.dedicatedVideoMemory, 8u * kGiB);
    EXPECT_EQ(stats.dedicatedSystemMemory, 0u);
    EXPECT_EQ(stats.sharedSystemMemory, 16u * kGiB);
    EXPECT_EQ(stats.totalGraphicsMemory, 24u * kGiB);
    EXPECT_EQ(stats.budgetVideoMemory, 7u * kGiB);
    EXPECT_EQ(stats.usedVideoMemory, 2u * kGiB);
    EXPECT_EQ(stats.availableVideoMemory, 5u * kGiB);
    EXPECT_EQ(stats.demotedVideoMemory, 0u);
    EXPECT_FALSE(stats.isOverBudget());
}

TEST(VulkanMemoryStatsTranslationTest, OverBudgetLocalUsageIsReportedAsDemoted)
{
    VulkanMemoryStats snapshot{};
    snapshot.budgetLocal = 4u * kGiB;
    snapshot.usedLocal = 5u * kGiB;
    snapshot.availableLocal = 0u;
    snapshot.demotedLocal = 1u * kGiB;

    const profiling::RHIMemoryStats stats = makeVulkanRHIMemoryStats(snapshot, 4u * kGiB, 0u);

    EXPECT_EQ(stats.demotedVideoMemory, 1u * kGiB);
    EXPECT_EQ(stats.availableVideoMemory, 0u);
    EXPECT_TRUE(stats.isOverBudget());
}

TEST(VulkanMemoryStatsTranslationTest, SystemHeapUsageDoesNotLeakIntoVideoFields)
{
    VulkanMemoryStats snapshot{};
    snapshot.budgetSystem = 32u * kGiB;
    snapshot.usedSystem = 31u * kGiB;
    snapshot.demotedSystem = 3u * kGiB;

    const profiling::RHIMemoryStats stats = makeVulkanRHIMemoryStats(snapshot, 0u, 32u * kGiB);

    EXPECT_EQ(stats.dedicatedVideoMemory, 0u);
    EXPECT_EQ(stats.usedVideoMemory, 0u);
    EXPECT_EQ(stats.demotedVideoMemory, 0u);
    EXPECT_EQ(stats.totalGraphicsMemory, 32u * kGiB);
}

}   // namespace gp::rhi::vulkan::tests
