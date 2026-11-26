// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Layout of one pipeline: its descriptor sets layout and the `VkPipelineLayout` built from it.
/// @details Usage is three steps: fill `getDescriptorSetsLayout()` with `finalizeBindings()`, call `compile()`, then
///          create the pipeline with `getPipelineLayout()`. Graphics pipeline layouts derive from this class to add the
///          per-stage remapping data that the pipeline descriptor state needs.
/// @note Unlike the set layouts (owned by the cache), the `VkPipelineLayout` is owned by this object and is released
///       through the deferred deletion queue, since in-flight command buffers may still reference it.
class GP_RHI_VULKAN_API VulkanPipelineLayout
{
public:
    /// @brief Creates an uncompiled layout.
    /// @param[in] device Logical device.
    /// @param[in] isGfxLayout True for graphics pipelines, false for compute.
    /// @param[in] usesBindless True when the pipeline reads resources through bindless heaps.
    /// @param[in] deferredDeletionQueue Queue used to release the layout; null destroys it immediately.
    VulkanPipelineLayout(
        VulkanDevice& device,
        bool isGfxLayout,
        bool usesBindless,
        VulkanDeferredDeletionQueue* deferredDeletionQueue = nullptr
    ) noexcept;

    /// @brief Releases the pipeline layout.
    virtual ~VulkanPipelineLayout() noexcept;

    VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
    VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;
    VulkanPipelineLayout(VulkanPipelineLayout&&) = delete;
    VulkanPipelineLayout& operator=(VulkanPipelineLayout&&) = delete;

    /// @brief Compiles the descriptor sets layout and creates the `VkPipelineLayout`.
    /// @param[in,out] cache Device set layout cache.
    /// @return False when already compiled, when the sets layout fails, when the pipeline uses bindless resources
    ///         (the bindless descriptor manager provides that layout and does not exist yet), or on a Vulkan error.
    [[nodiscard]] bool compile(VulkanDescriptorSetLayoutCache& cache) noexcept;

    /// @brief Returns the descriptor sets layout.
    /// @return Sets layout.
    [[nodiscard]] const VulkanDescriptorSetsLayout& getDescriptorSetsLayout() const noexcept
    {
        return m_descriptorSetsLayout;
    }

    /// @brief Returns the descriptor sets layout for `finalizeBindings()`.
    /// @return Mutable sets layout; must not change after `compile()`.
    [[nodiscard]] VulkanDescriptorSetsLayout& getDescriptorSetsLayout() noexcept
    {
        return m_descriptorSetsLayout;
    }

    /// @brief Returns the native pipeline layout.
    /// @return Handle, or `VK_NULL_HANDLE` before `compile()`.
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const noexcept
    {
        return m_pipelineLayout;
    }

    /// @brief Reports whether any stage declares descriptors.
    /// @return True when at least one set layout exists.
    [[nodiscard]] bool hasDescriptors() const noexcept
    {
        return !m_descriptorSetsLayout.getLayouts().isEmpty();
    }

    /// @brief Returns the hash of the descriptor sets layout.
    /// @return Layout hash.
    [[nodiscard]] gp::UInt32 getDescriptorSetLayoutHash() const noexcept
    {
        return m_descriptorSetsLayout.getHash();
    }

    /// @brief Reports whether this is a graphics pipeline layout.
    /// @return True for graphics, false for compute.
    [[nodiscard]] bool isGfxLayout() const noexcept
    {
        return m_isGfxLayout;
    }

    /// @brief Reports whether the layout uses bindless resources.
    /// @return True when bindless.
    [[nodiscard]] bool usesBindless() const noexcept
    {
        return m_usesBindless;
    }

protected:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Queue used to release `m_pipelineLayout`; may be null.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Descriptor sets layout.
    VulkanDescriptorSetsLayout m_descriptorSetsLayout;

    /// @brief Native pipeline layout.
    VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };

    /// @brief True for graphics pipelines.
    bool m_isGfxLayout{ false };

    /// @brief True when bindless.
    bool m_usesBindless{ false };
};

}   // namespace gp::rhi::vulkan
