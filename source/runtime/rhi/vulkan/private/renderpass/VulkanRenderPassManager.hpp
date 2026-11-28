// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"              // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"             // IWYU pragma: keep
#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Device-wide cache of render passes and framebuffers.
/// @details Two caches with different keys:
///          - render passes are keyed by the layout's full hash (formats, sample counts, load/store ops, layouts):
///            two passes with the same key are interchangeable for `vkCmdBeginRenderPass`;
///          - framebuffers are keyed by the layout's compatible hash chained with the mip and slice of every color
///            target, then matched by image and render area: the compatible hash is enough since a framebuffer
///            only needs a *compatible* render pass.
///          Entries live until the manager is destroyed, except framebuffers, which are dropped as soon as one of
///          their images is destroyed (`notifyDeletedImage()`).
///
///          With `VK_KHR_dynamic_rendering` enabled (and opted in) neither cache is used: passes go through
///          `VulkanDynamicRenderingInfo` instead.
/// @note Thread-safe: each cache has its own lock, so parallel recording contexts can look up render passes while
///       another one creates a framebuffer.
class GP_RHI_VULKAN_API VulkanRenderPassManager final
{
public:
    /// @brief Binds the manager to a device.
    /// @param[in] device Logical device; must outlive the manager.
    /// @param[in] deferredDeletionQueue Queue retiring handles, or `nullptr` to destroy immediately.
    /// @param[in] caps Capability snapshot of `device`.
    /// @param[in] useDynamicRendering Records passes with `VK_KHR_dynamic_rendering`; needs
    /// `caps.hasKHRDynamicRendering`.
    VulkanRenderPassManager(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        const VulkanRenderTargetCaps& caps,
        bool useDynamicRendering = false
    ) noexcept;

    /// @brief Destroys every cached framebuffer and render pass.
    /// @note The caller guarantees the GPU is idle or that the deletion queue outlives in-flight work.
    ~VulkanRenderPassManager() noexcept;

    VulkanRenderPassManager(const VulkanRenderPassManager&) = delete;
    VulkanRenderPassManager& operator=(const VulkanRenderPassManager&) = delete;
    VulkanRenderPassManager(VulkanRenderPassManager&&) = delete;
    VulkanRenderPassManager& operator=(VulkanRenderPassManager&&) = delete;

    /// @brief Returns the render pass of a layout, creating it on first use.
    /// @param[in] layout Render target layout.
    /// @return Cached render pass, owned by the manager; never `nullptr`, but its handle is null on failure.
    [[nodiscard]] VulkanRenderPass* getOrCreateRenderPass(const VulkanRenderTargetLayout& layout) noexcept;

    /// @brief Returns the framebuffer of a set of render targets, creating it on first use.
    /// @param[in] rtInfo Render targets.
    /// @param[in] layout Layout built from the same targets.
    /// @param[in] renderPass Render pass compatible with `layout`.
    /// @return Cached framebuffer, owned by the manager; never `nullptr`, but its handle is null on failure.
    [[nodiscard]] VulkanFramebuffer* getOrCreateFramebuffer(
        const command::SetRenderTargetsInfo& rtInfo,
        const VulkanRenderTargetLayout& layout,
        const VulkanRenderPass& renderPass
    ) noexcept;

    /// @brief Drops every framebuffer referencing an image about to be destroyed.
    /// @details Only render target images can be attachments, so other images return at once. Framebuffers are
    ///          retired through the deletion queue: command buffers in flight may still use them.
    /// @param[in] image Image being destroyed.
    /// @param[in] flags Creation flags of its texture.
    void notifyDeletedImage(VkImage image, resource::TextureCreateFlags flags) noexcept;

    /// @brief Begins a render pass object on a command buffer.
    /// @param[in,out] commandBuffer Primary command buffer outside any pass.
    /// @param[in] rpInfo Pass description `renderPass` and `framebuffer` were built from.
    /// @param[in] renderPass Render pass.
    /// @param[in] framebuffer Framebuffer.
    /// @param[in] isParallelRenderPass Records the subpass contents through secondary command buffers.
    /// @return True when the pass was begun.
    [[nodiscard]] bool beginRenderPass(
        VulkanCommandBuffer& commandBuffer,
        const command::RenderPassInfo& rpInfo,
        const VulkanRenderPass& renderPass,
        const VulkanFramebuffer& framebuffer,
        bool isParallelRenderPass = false
    ) noexcept;

    /// @brief Ends the render pass object open on a command buffer.
    /// @param[in,out] commandBuffer Command buffer inside a pass.
    /// @return True when the pass was ended.
    [[nodiscard]] bool endRenderPass(VulkanCommandBuffer& commandBuffer) noexcept;

    /// @brief Returns the capability snapshot.
    /// @return Capabilities.
    [[nodiscard]] const VulkanRenderTargetCaps& getCaps() const noexcept
    {
        return m_caps;
    }

    /// @brief Reports whether passes are recorded with `VK_KHR_dynamic_rendering`.
    /// @return True when the caches are bypassed.
    [[nodiscard]] bool usesDynamicRendering() const noexcept
    {
        return m_useDynamicRendering;
    }

    /// @brief Returns the number of cached render passes.
    /// @return Render pass count.
    [[nodiscard]] gp::USize getNumRenderPasses() const noexcept;

    /// @brief Returns the number of cached framebuffers.
    /// @return Framebuffer count.
    [[nodiscard]] gp::USize getNumFramebuffers() const noexcept;

    /// @brief Computes the framebuffer cache key of a set of render targets.
    /// @param[in] rtInfo Render targets.
    /// @param[in] layout Layout built from `rtInfo`.
    /// @return Compatible hash chained with the mip and slice of every color target.
    [[nodiscard]] static gp::UInt32 computeFramebufferHash(
        const command::SetRenderTargetsInfo& rtInfo, const VulkanRenderTargetLayout& layout
    ) noexcept;

private:
    /// @brief Cached render pass.
    struct RenderPassEntry final
    {
        /// @brief Full layout hash.
        gp::UInt32 hash{ 0u };

        /// @brief Render pass.
        gp::UniquePtr<VulkanRenderPass> renderPass{};
    };

    /// @brief Cached framebuffer.
    struct FramebufferEntry final
    {
        /// @brief Framebuffer key (`computeFramebufferHash()`).
        gp::UInt32 hash{ 0u };

        /// @brief Framebuffer.
        gp::UniquePtr<VulkanFramebuffer> framebuffer{};
    };

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Queue retiring handles, or `nullptr`.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Capability snapshot.
    VulkanRenderTargetCaps m_caps;

    /// @brief Bypasses both caches.
    bool m_useDynamicRendering{ false };

    /// @brief Guards `m_renderPasses`.
    mutable gp::thread::Mutex m_renderPassMutex;

    /// @brief Cached render passes; linear lookup on a small set of hot keys.
    gp::Vector<RenderPassEntry> m_renderPasses;

    /// @brief Guards `m_framebuffers`.
    mutable gp::thread::Mutex m_framebufferMutex;

    /// @brief Cached framebuffers.
    gp::Vector<FramebufferEntry> m_framebuffers;
};

}   // namespace gp::rhi::vulkan
