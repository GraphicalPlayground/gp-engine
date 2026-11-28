// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "VulkanThirdParty.hpp"                // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Device-wide inputs used when translating `sync::ResourceAccess` values into Vulkan flags.
/// @details Mirrors the handful of global capability values the translation tables depend on, so they stay pure
///          functions of their arguments and can be tested without a live device.
struct VulkanBarrierTranslationDesc
{
    /// @brief Graphics shader stages present on the device (vertex and fragment, plus geometry/task/mesh if enabled).
    VkPipelineStageFlags graphicsPipelineStageBits{ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };

    /// @brief True when ray tracing shaders are enabled; adds the ray tracing stage to acceleration structure access.
    bool supportsRayTracingShaders{ false };

    /// @brief Encoding used by shading rate source textures.
    VulkanShadingRateImageType shadingRateImageType{ VulkanShadingRateImageType::NotSupported };

    /// @brief True when `SRVGraphicsNonPixel` is tracked separately from pixel reads.
    /// @details When false, the vertex and geometry stages are stripped from destination masks unless the previous
    ///          writer was a compute or transfer stage, because only those can update data the vertex stage reads.
    bool supportsDedicatedSRVGraphicsNonPixelAccess{ false };
};

/// @brief Queue family and capability snapshot consumed by transition processing.
struct VulkanBarrierDeviceDesc
{
    /// @brief Separate depth/stencil layouts, `synchronization2` and render pass 2 are all enabled.
    /// @details Selects the `synchronization2` path; without it only the legacy single-queue path is available.
    bool supportsParallelRendering{ false };

    /// @brief Images moved between the graphics and compute families need explicit release/acquire barriers.
    bool needsQueueFamilyOwnershipTransfers{ true };

    /// @brief A dedicated async compute queue exists.
    bool hasAsyncComputeQueue{ false };

    /// @brief `VK_KHR_maintenance9` is enabled (changes how 3D texture slices are addressed).
    bool hasKHRMaintenance9{ false };

    /// @brief Queue family index of the graphics queue.
    gp::UInt32 graphicsQueueFamilyIndex{ 0u };

    /// @brief Queue family index of the async compute queue (equals the graphics family when none exists).
    gp::UInt32 computeQueueFamilyIndex{ 0u };

    /// @brief Stage bits accepted by the async compute queue; used to strip graphics-only stages.
    VkPipelineStageFlags computeSupportedStages{ 0u };

    /// @brief Access bits accepted by the async compute queue; used to strip graphics-only accesses.
    VkAccessFlags computeSupportedAccess{ 0u };
};

/// @brief Runtime toggles controlling barrier batching, collapsing and split barriers.
/// @details Defaults match the console variable defaults read by `getVulkanBarrierSettings()`.
struct VulkanBarrierSettings
{
    /// @brief Collapses buffer barriers and image barriers without a layout change into one global memory barrier.
    bool useMemoryBarrierOpt{ true };

    /// @brief Maximum barriers per `vkCmdPipelineBarrier2KHR` call; zero or negative means unlimited.
    /// @note Any positive value also disables split barriers.
    gp::Int32 maxBarriersPerBatch{ -1 };

    /// @brief Allows split barriers built on `synchronization2` events.
    bool allowSplitBarriers{ true };

    /// @brief Images use `VK_SHARING_MODE_CONCURRENT`, so no queue family ownership transfer is recorded for them.
    bool allowConcurrentImage{ false };
};

}   // namespace gp::rhi::vulkan
