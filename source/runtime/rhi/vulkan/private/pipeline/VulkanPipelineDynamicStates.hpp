// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanPipelineTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Reads the extension bits that decide which dynamic states graphics pipelines enable.
/// @param[in] device Logical device.
/// @return Snapshot of the four dynamic state extensions.
[[nodiscard]] GP_RHI_VULKAN_API VulkanDynamicStateFeatures
    makeVulkanDynamicStateFeatures(const VulkanDevice& device) noexcept;

/// @brief Lists the dynamic states a graphics pipeline is created with.
/// @details Every state listed here is left out of the baked pipeline and must be set on the command buffer before a
///          draw. The more state is dynamic, the fewer pipelines one material needs:
///          - Always: stencil reference and depth bounds (set per draw by the state cache).
///          - Extended dynamic state 1: cull mode, depth/stencil test state, viewport and scissor *with count*, and the
///            primitive topology (not for mesh pipelines, which have no input assembly). Without it, plain viewport and
///            scissor are dynamic instead.
///          - Extended dynamic state 2: depth bias enable and depth bias.
///          - Extended dynamic state 3: per-attachment blend enable, write mask and equation (only with color
///            attachments), polygon mode, sample count and alpha to coverage.
///          - Vertex input dynamic state: the vertex input layout (not for mesh pipelines).
/// @param[in] features Dynamic state extensions of the device.
/// @param[in] desc Pipeline description (mesh stage and color attachment count).
/// @return Enabled states, at most `kMaxPipelineDynamicStates`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanDynamicStateList fillVulkanDynamicStates(
    const VulkanDynamicStateFeatures& features, const VulkanGraphicsPipelineDesc& desc
) noexcept;

}   // namespace gp::rhi::vulkan
