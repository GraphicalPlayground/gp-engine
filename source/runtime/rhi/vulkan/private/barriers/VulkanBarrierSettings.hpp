// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Reads the barrier console variables into a `VulkanBarrierSettings` snapshot.
/// @details Reads `r.Vulkan.UseMemoryBarrierOpt`, `r.Vulkan.MaxBarriersPerBatch`, `r.Vulkan.AllowSplitBarriers` and
///          `r.Vulkan.AllowConcurrentImage`.
/// @note Safe to call from any thread; values are read through the render-thread mirror of each variable.
/// @return Current barrier settings.
[[nodiscard]] GP_RHI_VULKAN_API VulkanBarrierSettings getVulkanBarrierSettings() noexcept;

/// @brief Computes the graphics shader stages present on a device.
/// @details Vertex and fragment stages are always present. Geometry is added when `geometryShader` is enabled, task
///          and mesh stages when `VK_EXT_mesh_shader` is enabled.
/// @param[in] geometryShaderEnabled True when the `geometryShader` core feature is enabled.
/// @param[in] meshShaderEnabled True when `VK_EXT_mesh_shader` is enabled.
/// @return Graphics stage bits used for `SRVGraphics` and `UAVGraphics` access.
[[nodiscard]] GP_RHI_VULKAN_API VkPipelineStageFlags
    computeVulkanGraphicsPipelineStageBits(bool geometryShaderEnabled, bool meshShaderEnabled) noexcept;

/// @brief Builds the translation inputs for `device` from its enabled features and the RHI globals.
/// @param[in] device Initialized device.
/// @return Translation descriptor for that device.
[[nodiscard]] GP_RHI_VULKAN_API VulkanBarrierTranslationDesc
    makeVulkanBarrierTranslationDesc(const VulkanDevice& device) noexcept;

/// @brief Builds the queue family snapshot for `device`.
/// @details The compute stage and access masks are derived from the queue family flags of the async compute family
///          (or the graphics family when no dedicated compute queue was created).
/// @param[in] device Initialized device.
/// @return Device descriptor for transition processing.
[[nodiscard]] GP_RHI_VULKAN_API VulkanBarrierDeviceDesc
    makeVulkanBarrierDeviceDesc(const VulkanDevice& device) noexcept;

}   // namespace gp::rhi::vulkan
