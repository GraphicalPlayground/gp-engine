// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                              // IWYU pragma: keep
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Reads every presentation console variable and command-line switch into one snapshot.
/// @details Owned variables:
///          - `r.Vulkan.KeepSwapChain` (1, 0 on Android): reuse the old swapchain and surface when recreating.
///          - `r.Vulkan.CpuWaitForFence` (0): wait on the CPU for each acquired image.
///          - `r.Vulkan.CPURHIThreadFramePacer` (0): sleep before presenting to hold `r.Vulkan.FramePacerRefreshRate`.
///          - `r.Vulkan.ForcePacingWithoutVSync` (0): pace even with vertical sync off.
///          - `r.Vulkan.FramePacerRefreshRate` (60): refresh rate the pacer targets.
///          - `r.Vulkan.PreTransform` (1): create swapchains with the surface's current transform.
///
///          Variables owned elsewhere are looked up by name and default when unregistered: `r.HDR.Display.OutputDevice`
///          (renderer), `r.VSync` (engine) and `r.Vulkan.AllowPresentOnComputeQueue` (queue selection). The command
///          line switch `-vulkanpresentmode=N` forces a `VkPresentModeKHR`.
/// @note The pacer is off by default: without a display refresh rate query it would cap high refresh monitors to
///       `r.Vulkan.FramePacerRefreshRate`.
/// @return Snapshot of the current settings.
[[nodiscard]] GP_RHI_VULKAN_API VulkanPresentationConfig resolveVulkanPresentationConfig() noexcept;

}   // namespace gp::rhi::vulkan
