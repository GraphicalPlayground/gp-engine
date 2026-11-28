// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "utils/VulkanDebugBarrier.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

static_assert(kVulkanDebugBarrierEndRenderPass == 1);
static_assert(kVulkanDebugBarrierDispatch == 2);
static_assert(kVulkanDebugBarrierUploadCommandBuffer == 4);
static_assert(kVulkanDebugBarrierActiveCommandBuffer == 8);
static_assert(kVulkanDebugBarrierBufferUpload == 16);
static_assert(kVulkanDebugBarrierSwapchainBlit == 32);

TEST(VulkanDebugBarrierTest, HeavyWeightBarrierCoversEveryAccessBothWays)
{
    const VkMemoryBarrier barrier = makeHeavyWeightMemoryBarrier();

    EXPECT_EQ(barrier.sType, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    EXPECT_EQ(barrier.pNext, nullptr);
    EXPECT_EQ(barrier.srcAccessMask, barrier.dstAccessMask);

    const VkAccessFlags required = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
                                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                                   VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
                                   VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
    EXPECT_EQ(barrier.srcAccessMask & required, required);
}

TEST(VulkanDebugBarrierTest, DisabledByDefault)
{
    EXPECT_FALSE(shouldRecordDebugBarrier(kVulkanDebugBarrierEndRenderPass));
    EXPECT_FALSE(shouldRecordDebugBarrier(kVulkanDebugBarrierBufferUpload));
    EXPECT_FALSE(shouldRecordDebugBarrier(kVulkanDebugBarrierSwapchainBlit));
}

TEST(VulkanDebugBarrierTest, NullCommandBufferIsIgnoredWhenDisabled)
{
    // With the cvar at its default, no command is recorded, so a null handle must be safe.
    debugHeavyWeightBarrier(VK_NULL_HANDLE, kVulkanDebugBarrierBufferUpload);
    SUCCEED();
}

}   // namespace gp::rhi::vulkan::tests

