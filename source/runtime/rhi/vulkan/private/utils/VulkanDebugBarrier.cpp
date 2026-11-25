// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "utils/VulkanDebugBarrier.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanDebugBarrier(
    "r.Vulkan.DebugBarrier",
    0,
    "Forces a full barrier for debugging. This is a mask/bitfield (so add up the values)!\n"
    " 0: Don't (default)\n"
    " 1: Enable heavy barriers after EndRenderPass()\n"
    " 2: Enable heavy barriers after every dispatch\n"
    " 4: Enable heavy barriers after upload cmd buffers\n"
    " 8: Enable heavy barriers after active cmd buffers\n"
    " 16: Enable heavy buffer barrier after uploads\n"
    " 32: Enable heavy buffer barrier between acquiring back buffer and blitting into swapchain\n",
    gp::console::ConsoleVarFlags::None
);

/// @brief Every access type a heavy barrier synchronizes.
constexpr VkAccessFlags kHeavyWeightAccessMask =
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
    VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |
    VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT | VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;

}   // namespace

VkMemoryBarrier makeHeavyWeightMemoryBarrier() noexcept
{
    VkMemoryBarrier barrier = zeroVulkanStruct<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    barrier.srcAccessMask = kHeavyWeightAccessMask;
    barrier.dstAccessMask = kHeavyWeightAccessMask;
    return barrier;
}

void recordHeavyWeightBarrier(const VkCommandBuffer commandBuffer) noexcept
{
    const VkMemoryBarrier barrier = makeHeavyWeightMemoryBarrier();
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0u,
        1u,
        &barrier,
        0u,
        nullptr,
        0u,
        nullptr
    );
}

bool shouldRecordDebugBarrier([[maybe_unused]] const gp::Int32 categoryMask) noexcept
{
#if GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT
    return (g_cvarVulkanDebugBarrier.getValueOnAnyThread() & categoryMask) != 0;
#else
    return false;
#endif
}

void debugHeavyWeightBarrier(const VkCommandBuffer commandBuffer, const gp::Int32 categoryMask) noexcept
{
    if (shouldRecordDebugBarrier(categoryMask))
    {
        recordHeavyWeightBarrier(commandBuffer);
    }
}

}   // namespace gp::rhi::vulkan

