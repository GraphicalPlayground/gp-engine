// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief `r.Vulkan.DebugBarrier` bit: heavy barrier after ending a render pass.
inline constexpr gp::Int32 kVulkanDebugBarrierEndRenderPass = 1;

/// @brief `r.Vulkan.DebugBarrier` bit: heavy barrier after every dispatch.
inline constexpr gp::Int32 kVulkanDebugBarrierDispatch = 2;

/// @brief `r.Vulkan.DebugBarrier` bit: heavy barrier after upload command buffers.
inline constexpr gp::Int32 kVulkanDebugBarrierUploadCommandBuffer = 4;

/// @brief `r.Vulkan.DebugBarrier` bit: heavy barrier after active command buffers.
inline constexpr gp::Int32 kVulkanDebugBarrierActiveCommandBuffer = 8;

/// @brief `r.Vulkan.DebugBarrier` bit: heavy barrier before a buffer upload copy.
inline constexpr gp::Int32 kVulkanDebugBarrierBufferUpload = 16;

/// @brief `r.Vulkan.DebugBarrier` bit: heavy barrier between back buffer acquire and the swapchain blit.
inline constexpr gp::Int32 kVulkanDebugBarrierSwapchainBlit = 32;

/// @brief `r.Vulkan.DebugBarrier` bit: heavy barrier before a texture upload copy.
inline constexpr gp::Int32 kVulkanDebugBarrierTextureUpload = 64;

/// @brief Builds the memory barrier that makes every access type available and visible to every other.
/// @return `VkMemoryBarrier` with all read and write access bits set on both sides.
[[nodiscard]] GP_RHI_VULKAN_API VkMemoryBarrier makeHeavyWeightMemoryBarrier() noexcept;

/// @brief Records `makeHeavyWeightMemoryBarrier()` between `ALL_COMMANDS` and `ALL_COMMANDS`.
/// @details Serializes the whole queue. Only meant for hunting missing barriers.
/// @param[in] commandBuffer Command buffer in the recording state.
GP_RHI_VULKAN_API void recordHeavyWeightBarrier(VkCommandBuffer commandBuffer) noexcept;

/// @brief Reports whether `r.Vulkan.DebugBarrier` enables any bit of `categoryMask`.
/// @note Always false in profile and shipping builds.
/// @param[in] categoryMask Debug category bits of the call site.
/// @return True when a heavy barrier must be recorded.
[[nodiscard]] GP_RHI_VULKAN_API bool shouldRecordDebugBarrier(gp::Int32 categoryMask) noexcept;

/// @brief Records a heavy barrier when `r.Vulkan.DebugBarrier` enables `categoryMask`.
/// @param[in] commandBuffer Command buffer in the recording state.
/// @param[in] categoryMask Debug category bits of the call site.
GP_RHI_VULKAN_API void debugHeavyWeightBarrier(VkCommandBuffer commandBuffer, gp::Int32 categoryMask) noexcept;

}   // namespace gp::rhi::vulkan
