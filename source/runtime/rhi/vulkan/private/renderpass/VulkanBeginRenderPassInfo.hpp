// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"              // IWYU pragma: keep
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Everything `vkCmdBeginRenderPass` needs for one pass: the begin info and the clear values it points to.
/// @details Clear values are written at the attachment index the layout gave each target, so resolve attachments
///          (which never clear) simply keep zeroed slots. When no attachment clears, the clear value count is zero
///          and drivers skip reading the array altogether.
/// @note Not copyable: `beginInfo.pClearValues` points into the object itself.
struct GP_RHI_VULKAN_API VulkanBeginRenderPassInfo final
{
    /// @brief Clear values, by attachment index.
    gp::Array<VkClearValue, VulkanRenderTargetLayout::kMaxAttachmentDescriptions> clearValues{};

    /// @brief Begin info pointing at `clearValues`.
    VkRenderPassBeginInfo beginInfo{};

    /// @brief Records the subpass contents through secondary command buffers.
    bool isParallelRenderPass{ false };

    /// @brief Creates an empty description.
    VulkanBeginRenderPassInfo() noexcept = default;

    VulkanBeginRenderPassInfo(const VulkanBeginRenderPassInfo&) = delete;
    VulkanBeginRenderPassInfo& operator=(const VulkanBeginRenderPassInfo&) = delete;
    VulkanBeginRenderPassInfo(VulkanBeginRenderPassInfo&&) = delete;
    VulkanBeginRenderPassInfo& operator=(VulkanBeginRenderPassInfo&&) = delete;

    /// @brief Fills the clear values and the begin info.
    /// @details Color targets without a fast-clear value clear to opaque black; depth-stencil targets without one
    ///          clear to the texture default.
    /// @param[in] rpInfo Pass description the render pass and framebuffer were built from.
    /// @param[in] renderPass Render pass.
    /// @param[in] framebuffer Framebuffer.
    void build(
        const command::RenderPassInfo& rpInfo, const VulkanRenderPass& renderPass, const VulkanFramebuffer& framebuffer
    ) noexcept;

    /// @brief Returns how the first subpass records its commands.
    /// @return Secondary command buffers for parallel passes, inline otherwise.
    [[nodiscard]] VkSubpassContents getSubpassContents() const noexcept
    {
        return isParallelRenderPass ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE;
    }
};

}   // namespace gp::rhi::vulkan
