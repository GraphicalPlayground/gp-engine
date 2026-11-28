// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Creates a native render pass from a layout.
/// @details Goes through `vkCreateRenderPass2` when `VK_KHR_create_renderpass2` is enabled and loaded (the only path
///          that carries depth resolve and shading rate attachments), through `vkCreateRenderPass` otherwise.
/// @param[in] device Logical device.
/// @param[in] caps Capability snapshot of `device`.
/// @param[in] layout Render target layout.
/// @return New render pass, or `VK_NULL_HANDLE` on failure or without a device.
[[nodiscard]] GP_RHI_VULKAN_API VkRenderPass createVulkanRenderPass(
    const VulkanDevice& device, const VulkanRenderTargetCaps& caps, const VulkanRenderTargetLayout& layout
) noexcept;

/// @brief Owns one native render pass and the layout it was built from.
/// @details Render passes are immutable and shared: the render pass manager creates one per full layout hash and hands
///          out raw pointers for as long as it lives. The layout copy lets begin-pass code read the clear value count,
///          the extent and the attachment descriptions without going back to the manager.
/// @note Destruction goes through the deferred deletion queue when one is given, so command buffers still in flight
///       can keep referencing the handle.
class GP_RHI_VULKAN_API VulkanRenderPass final
{
public:
    /// @brief Creates the native render pass.
    /// @param[in] device Logical device; must outlive the render pass.
    /// @param[in] deferredDeletionQueue Queue that retires the handle, or `nullptr` to destroy immediately.
    /// @param[in] caps Capability snapshot of `device`.
    /// @param[in] layout Render target layout; copied.
    VulkanRenderPass(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        const VulkanRenderTargetCaps& caps,
        const VulkanRenderTargetLayout& layout
    ) noexcept;

    /// @brief Retires the native render pass.
    ~VulkanRenderPass() noexcept;

    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
    VulkanRenderPass(VulkanRenderPass&&) = delete;
    VulkanRenderPass& operator=(VulkanRenderPass&&) = delete;

    /// @brief Returns the native handle.
    /// @return Render pass, or `VK_NULL_HANDLE` when creation failed.
    [[nodiscard]] VkRenderPass getHandle() const noexcept
    {
        return m_renderPass;
    }

    /// @brief Returns the layout the render pass was built from.
    /// @return Layout copy.
    [[nodiscard]] const VulkanRenderTargetLayout& getLayout() const noexcept
    {
        return m_layout;
    }

    /// @brief Returns the number of clear values `vkCmdBeginRenderPass` must receive.
    /// @return Every attachment when one of them clears, zero otherwise.
    [[nodiscard]] gp::UInt32 getNumUsedClearValues() const noexcept
    {
        return m_numUsedClearValues;
    }

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Queue retiring the handle, or `nullptr`.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Layout copy.
    VulkanRenderTargetLayout m_layout;

    /// @brief Native handle.
    VkRenderPass m_renderPass{ VK_NULL_HANDLE };

    /// @brief Number of clear values the begin info needs.
    gp::UInt32 m_numUsedClearValues{ 0u };
};

}   // namespace gp::rhi::vulkan
