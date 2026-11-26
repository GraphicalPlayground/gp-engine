// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQuerySettings.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanQuerySettingsTest, DefaultConsoleVariablesSelectDocumentedBehaviour)
{
    EXPECT_EQ(getVulkanTimestampQueryStage(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    EXPECT_EQ(getVulkanTimingQueryPoolSize(), 128u);
    EXPECT_EQ(getVulkanQueryPoolDeletionDelay(), 10);
    EXPECT_EQ(getVulkanOcclusionQueryControlFlags(), static_cast<VkQueryControlFlags>(VK_QUERY_CONTROL_PRECISE_BIT));
}

TEST(VulkanQuerySettingsTest, TimestampConversionUsesTheTickPeriod)
{
    // One nanosecond per tick: one million ticks are one millisecond.
    EXPECT_EQ(convertVulkanTimestampToMicroseconds(1'000'000u, 1.0f), 1'000u);

    // Two nanoseconds per tick doubles the duration.
    EXPECT_EQ(convertVulkanTimestampToMicroseconds(1'000'000'000u, 2.0f), 2'000'000u);

    // Sub-nanosecond periods shrink it.
    EXPECT_EQ(convertVulkanTimestampToMicroseconds(4'000'000u, 0.25f), 1'000u);

    EXPECT_EQ(convertVulkanTimestampToMicroseconds(0u, 1.0f), 0u);
}

TEST(VulkanQuerySettingsTest, TimestampConversionRejectsInvalidPeriods)
{
    EXPECT_EQ(convertVulkanTimestampToMicroseconds(1'000'000u, 0.0f), 0u);
    EXPECT_EQ(convertVulkanTimestampToMicroseconds(1'000'000u, -1.0f), 0u);
}

}   // namespace gp::rhi::vulkan::tests
