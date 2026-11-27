// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"             // IWYU pragma: keep
#include "presentation/VulkanPresentationEnums.hpp"
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "RHIForward.hpp"
#include "VulkanThirdParty.hpp"                         // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Swapchain and surface handed from a destroyed swapchain to its replacement.
/// @details Recreating against the old swapchain (`oldSwapchain`) lets the presentation engine keep the images it is
///          still showing, and reusing the surface avoids a window-system round trip. The new swapchain takes the
///          surface (resetting `surface`); whoever triggered the recreation destroys `swapChain` once the new one
///          exists.
struct VulkanSwapChainRecreateInfo
{
    /// @brief Retired swapchain, passed as `oldSwapchain`; `VK_NULL_HANDLE` when none was kept.
    VkSwapchainKHR swapChain{ VK_NULL_HANDLE };

    /// @brief Surface of the retired swapchain; `VK_NULL_HANDLE` when none was kept.
    VkSurfaceKHR surface{ VK_NULL_HANDLE };
};

/// @brief Surface format and engine pixel format chosen for a swapchain.
struct VulkanSurfaceFormatSelection
{
    /// @brief Native format and color space the swapchain is created with.
    VkSurfaceFormatKHR surfaceFormat{ VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    /// @brief Engine pixel format matching `surfaceFormat.format`.
    PixelFormat pixelFormat{ PixelFormat::Unknown };

    /// @brief True when the requested format or color space was not available and another one was picked.
    bool isFallback{ false };
};

/// @brief Snapshot of every presentation setting read when a swapchain is created.
/// @details Console variables and the command line are read once into this POD, so swapchain creation and the
///          selection functions it uses stay pure and testable with hand-built settings.
struct VulkanPresentationConfig
{
    /// @brief Transfer function and gamut requested by the renderer (`r.HDR.Display.OutputDevice`).
    VulkanDisplayOutputFormat displayOutputFormat{ VulkanDisplayOutputFormat::SdrSrgb };

    /// @brief Present mode forced from the command line (`-vulkanpresentmode=N`); `-1` lets the selection decide.
    gp::Int32 presentModeOverride{ -1 };

    /// @brief Hz the CPU frame pacer targets when vertical sync is on (`r.Vulkan.FramePacerRefreshRate`).
    gp::UInt32 framePacerRefreshRate{ 60u };

    /// @brief Keep the old swapchain and surface alive across a recreation (`r.Vulkan.KeepSwapChain`).
    bool keepSwapChain{ true };

    /// @brief Wait on the CPU for each acquired image with a fence (`r.Vulkan.CpuWaitForFence`).
    bool cpuWaitForFence{ false };

    /// @brief Sleep on the RHI thread before presenting to hold a steady frame rate
    /// (`r.Vulkan.CPURHIThreadFramePacer`).
    bool cpuFramePacer{ false };

    /// @brief Pace even when vertical sync is off (`r.Vulkan.ForcePacingWithoutVSync`).
    bool forcePacingWithoutVSync{ false };

    /// @brief Create the swapchain with the surface's current transform (`r.Vulkan.PreTransform`).
    bool allowPreTransform{ true };

    /// @brief Allow presenting from the async compute queue when it supports the surface
    ///        (`r.Vulkan.AllowPresentOnComputeQueue`).
    bool allowPresentOnComputeQueue{ false };

    /// @brief Initial vertical sync state of new viewports (`r.VSync`).
    bool lockToVsync{ false };
};

}   // namespace gp::rhi::vulkan
