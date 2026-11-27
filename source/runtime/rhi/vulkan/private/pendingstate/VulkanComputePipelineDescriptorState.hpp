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
#include "shaders/VulkanShaderForward.hpp"               // IWYU pragma: keep
#include "VulkanForward.hpp"                             // IWYU pragma: keep
#include "VulkanThirdParty.hpp"                          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Descriptor bindings of one compute pipeline.
/// @details A compute layout has a single set (the compute stage), so this is the common state with one writer and
///          the pipeline it belongs to.
/// @note Not thread-safe; owned by a pending compute state on the recording thread.
class GP_RHI_VULKAN_API VulkanComputePipelineDescriptorState final : public VulkanCommonPipelineDescriptorState
{
public:
    /// @brief Builds the state of a compute pipeline from its layout and shader.
    /// @param[in] device Owning device.
    /// @param[in] pipeline Compute pipeline with a layout and a shader; must outlive the state.
    /// @param[in] defaults Default and global samplers and views.
    /// @param[in] useDescriptorCache True to track hashable infos for the descriptor set cache.
    VulkanComputePipelineDescriptorState(
        VulkanDevice& device,
        const VulkanComputePipelineState& pipeline,
        const VulkanDescriptorDefaults& defaults,
        bool useDescriptorCache
    ) noexcept;

    /// @brief Builds a state without a pipeline object.
    /// @details Used by tools and tests that finalize a layout from a shader header directly.
    /// @param[in] device Owning device.
    /// @param[in] layout Set layouts finalized for the compute shader; must outlive the state.
    /// @param[in] pipelineLayout Native pipeline layout, or `VK_NULL_HANDLE`.
    /// @param[in] computeHeader Code header of the compute shader; must outlive the state.
    /// @param[in] defaults Default and global samplers and views.
    /// @param[in] useDescriptorCache True to track hashable infos for the descriptor set cache.
    VulkanComputePipelineDescriptorState(
        VulkanDevice& device,
        const VulkanDescriptorSetsLayout& layout,
        VkPipelineLayout pipelineLayout,
        const VulkanShaderHeader& computeHeader,
        const VulkanDescriptorDefaults& defaults,
        bool useDescriptorCache
    ) noexcept;

    /// @brief Destroys the state.
    ~VulkanComputePipelineDescriptorState() noexcept = default;

    VulkanComputePipelineDescriptorState(const VulkanComputePipelineDescriptorState&) = delete;
    VulkanComputePipelineDescriptorState& operator=(const VulkanComputePipelineDescriptorState&) = delete;
    VulkanComputePipelineDescriptorState(VulkanComputePipelineDescriptorState&&) = delete;
    VulkanComputePipelineDescriptorState& operator=(VulkanComputePipelineDescriptorState&&) = delete;

    /// @brief Returns the pipeline the state belongs to.
    /// @return Pipeline, or null for a state built without one.
    [[nodiscard]] const VulkanComputePipelineState* getPipeline() const noexcept
    {
        return m_pipeline;
    }

private:
    /// @brief Pipeline the state belongs to.
    const VulkanComputePipelineState* m_pipeline{ nullptr };
};

}   // namespace gp::rhi::vulkan
