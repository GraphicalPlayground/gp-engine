// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanCommonPipelineDescriptorState.hpp"
#include "pendingstate/VulkanPendingStateForward.hpp"    // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"            // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"                             // IWYU pragma: keep
#include "VulkanThirdParty.hpp"                          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Descriptor bindings of one graphics pipeline.
/// @details One set per used graphics stage (vertex, pixel, geometry, mesh, task). On top of the common state it
///          rebinds the input attachments of the active subpass before every draw: those views come from the
///          render pass, not from the shader parameters, and change whenever the render pass does.
/// @note Not thread-safe; owned by a pending graphics state on the recording thread.
class GP_RHI_VULKAN_API VulkanGraphicsPipelineDescriptorState final : public VulkanCommonPipelineDescriptorState
{
public:
    /// @brief Builds the state of a graphics pipeline from its layout and shaders.
    /// @param[in] device Owning device.
    /// @param[in] pipeline Graphics pipeline with a layout; must outlive the state.
    /// @param[in] defaults Default and global samplers and views.
    /// @param[in] useDescriptorCache True to track hashable infos for the descriptor set cache.
    VulkanGraphicsPipelineDescriptorState(
        VulkanDevice& device,
        const VulkanGraphicsPipelineState& pipeline,
        const VulkanDescriptorDefaults& defaults,
        bool useDescriptorCache
    ) noexcept;

    /// @brief Builds a state without a pipeline object.
    /// @details Used by tools and tests that finalize a layout from shader headers directly.
    /// @param[in] device Owning device.
    /// @param[in] layout Set layouts finalized for the shaders; must outlive the state.
    /// @param[in] pipelineLayout Native pipeline layout, or `VK_NULL_HANDLE`.
    /// @param[in] codeHeaders Code header per graphics stage; must outlive the state.
    /// @param[in] defaults Default and global samplers and views.
    /// @param[in] useDescriptorCache True to track hashable infos for the descriptor set cache.
    VulkanGraphicsPipelineDescriptorState(
        VulkanDevice& device,
        const VulkanDescriptorSetsLayout& layout,
        VkPipelineLayout pipelineLayout,
        const VulkanUniformBufferGatherInfo& codeHeaders,
        const VulkanDescriptorDefaults& defaults,
        bool useDescriptorCache
    ) noexcept;

    /// @brief Destroys the state.
    ~VulkanGraphicsPipelineDescriptorState() noexcept = default;

    VulkanGraphicsPipelineDescriptorState(const VulkanGraphicsPipelineDescriptorState&) = delete;
    VulkanGraphicsPipelineDescriptorState& operator=(const VulkanGraphicsPipelineDescriptorState&) = delete;
    VulkanGraphicsPipelineDescriptorState(VulkanGraphicsPipelineDescriptorState&&) = delete;
    VulkanGraphicsPipelineDescriptorState& operator=(VulkanGraphicsPipelineDescriptorState&&) = delete;

    /// @brief Binds every input attachment the shaders read to the matching attachment of the active subpass.
    /// @param[in] context Source of the render pass attachment views.
    /// @return True when a binding changed and the sets must be rewritten.
    bool updateInputAttachments(const IVulkanPendingStateContext& context) noexcept;

    /// @brief Reports whether the shaders read input attachments.
    /// @return True when `updateInputAttachments()` has work to do.
    [[nodiscard]] bool hasInputAttachments() const noexcept;

    /// @brief Returns the pipeline the state belongs to.
    /// @return Pipeline, or null for a state built without one.
    [[nodiscard]] const VulkanGraphicsPipelineState* getPipeline() const noexcept
    {
        return m_pipeline;
    }

private:
    /// @brief Pipeline the state belongs to.
    const VulkanGraphicsPipelineState* m_pipeline{ nullptr };
};

}   // namespace gp::rhi::vulkan
