// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/VectorView.hpp"              // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"             // IWYU pragma: keep
#include "presentation/VulkanPresentationEnums.hpp"
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "presentation/VulkanPresentationTypes.hpp"
#include "RHIForward.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Engine pixel format a swapchain uses when the caller has no preference.
/// @details `B8G8R8A8` is the format every desktop presentation engine reports first.
inline constexpr PixelFormat kVulkanDefaultBackBufferFormat = PixelFormat::B8G8R8A8_UNorm;

/// @brief Picks the swapchain color space matching the renderer's display output.
/// @details SDR sRGB maps to `SRGB_NONLINEAR`, SDR Rec.709 to `BT709_NONLINEAR` and the PQ outputs to
///          `HDR10_ST2084`. Every other output (scRGB, linear) has no swapchain equivalent here and falls back to
///          `SRGB_NONLINEAR`. Non-sRGB color spaces only show up in the surface formats when
///          `VK_EXT_swapchain_colorspace` is enabled, so `selectVulkanSurfaceFormat()` falls back on its own otherwise.
/// @param[in] outputFormat Display output requested by the renderer.
/// @return Requested color space.
[[nodiscard]] GP_RHI_VULKAN_API VkColorSpaceKHR
    selectVulkanSwapChainColorSpace(VulkanDisplayOutputFormat outputFormat) noexcept;

/// @brief Picks the surface format the swapchain is created with.
/// @details In order of preference:
///          1. A surface reporting a single `VK_FORMAT_UNDEFINED` entry accepts anything: the requested format wins.
///          2. The requested format in the requested color space.
///          3. The first format the engine knows in the requested color space.
///          4. The first format the engine knows in any color space.
///          `Unknown` requests `kVulkanDefaultBackBufferFormat`.
/// @param[in] availableFormats Formats reported by `vkGetPhysicalDeviceSurfaceFormatsKHR`.
/// @param[in] requestedFormat Requested engine pixel format, or `Unknown`.
/// @param[in] requestedColorSpace Requested color space.
/// @param[out] outSelection Receives the chosen format; untouched on failure.
/// @return False when no reported format maps to an engine pixel format.
[[nodiscard]] GP_RHI_VULKAN_API bool selectVulkanSurfaceFormat(
    gp::ConstVectorView<VkSurfaceFormatKHR> availableFormats,
    PixelFormat requestedFormat,
    VkColorSpaceKHR requestedColorSpace,
    VulkanSurfaceFormatSelection& outSelection
) noexcept;

/// @brief Picks the presentation mode.
/// @details A valid command-line override wins when the surface supports it. Otherwise: `IMMEDIATE` when vertical
///          sync is off (no queue, tearing allowed), then `MAILBOX` (latest frame replaces the queued one), then
///          `FIFO` (the only mode every implementation must support), then whatever comes first.
/// @param[in] availableModes Modes reported by `vkGetPhysicalDeviceSurfacePresentModesKHR`.
/// @param[in] lockToVsync True to synchronize with vertical blank.
/// @param[in] overrideMode Forced `VkPresentModeKHR` value, or a negative value for none.
/// @return Selected mode; `FIFO` when `availableModes` is empty.
[[nodiscard]] GP_RHI_VULKAN_API VkPresentModeKHR selectVulkanPresentMode(
    gp::ConstVectorView<VkPresentModeKHR> availableModes, bool lockToVsync, gp::Int32 overrideMode
) noexcept;

/// @brief Clamps the requested image count to the surface limits.
/// @param[in] capabilities Surface capabilities.
/// @param[in] desiredCount Requested number of images.
/// @return Count within `[minImageCount, maxImageCount]` (`maxImageCount == 0` means unbounded).
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32
    selectVulkanSwapChainImageCount(const VkSurfaceCapabilitiesKHR& capabilities, gp::UInt32 desiredCount) noexcept;

/// @brief Picks the transform applied by the presentation engine.
/// @details With pre-transform allowed, the current transform is used so a rotated display shows images the
///          application rendered rotated, which avoids a compositor pass. Otherwise identity is preferred.
/// @param[in] capabilities Surface capabilities.
/// @param[in] allowPreTransform True to accept the surface's current transform.
/// @return Selected transform.
[[nodiscard]] GP_RHI_VULKAN_API VkSurfaceTransformFlagBitsKHR
    selectVulkanSurfaceTransform(const VkSurfaceCapabilitiesKHR& capabilities, bool allowPreTransform) noexcept;

/// @brief Reports whether a transform rotates by a quarter turn (width and height trade places).
/// @param[in] transform Surface transform.
/// @return True for 90 and 270 degree rotations, mirrored or not.
[[nodiscard]] GP_RHI_VULKAN_API bool isVulkanQuarterTurnTransform(VkSurfaceTransformFlagBitsKHR transform) noexcept;

/// @brief Picks the swapchain image extent.
/// @details A surface with a defined `currentExtent` imposes it. A surface reporting `0xFFFFFFFF` lets the swapchain
///          decide: the requested size, swapped for quarter-turn transforms, clamped to the surface limits.
/// @param[in] capabilities Surface capabilities.
/// @param[in] desiredWidth Requested width in pixels.
/// @param[in] desiredHeight Requested height in pixels.
/// @param[in] transform Transform the swapchain is created with.
/// @return Image extent.
[[nodiscard]] GP_RHI_VULKAN_API VkExtent2D selectVulkanSwapChainExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    gp::UInt32 desiredWidth,
    gp::UInt32 desiredHeight,
    VkSurfaceTransformFlagBitsKHR transform
) noexcept;

/// @brief Picks how the swapchain alpha is composited with the desktop.
/// @param[in] supportedFlags Supported composite alpha modes.
/// @return `OPAQUE` when supported, else `INHERIT`, else the lowest supported mode.
[[nodiscard]] GP_RHI_VULKAN_API VkCompositeAlphaFlagBitsKHR
    selectVulkanCompositeAlpha(VkCompositeAlphaFlagsKHR supportedFlags) noexcept;

/// @brief Translates the result of `vkAcquireNextImageKHR`.
/// @param[in] result Native result.
/// @return `Healthy` for success and suboptimal, the matching status otherwise.
[[nodiscard]] GP_RHI_VULKAN_API VulkanSwapChainStatus translateVulkanAcquireResult(VkResult result) noexcept;

/// @brief Translates the result of `vkQueuePresentKHR`.
/// @details Losing exclusive fullscreen is reported as `OutOfDate`: the swapchain must be recreated either way.
/// @param[in] result Native result.
/// @return `Healthy` for success and suboptimal, the matching status otherwise.
[[nodiscard]] GP_RHI_VULKAN_API VulkanSwapChainStatus translateVulkanPresentResult(VkResult result) noexcept;

/// @brief Computes how long the CPU frame pacer sleeps before presenting.
/// @details Presents are spaced by `periodNs`. When early, the pacer sleeps until the target and the next target is one
///          period later; when late, it does not sleep and restarts the cadence from now, so a hitch never causes a
///          burst of catch-up frames.
/// @param[in] nowNs Current time in nanoseconds.
/// @param[in] nextTargetNs Time the next present should happen at; `0` for the first present.
/// @param[in] periodNs Target time between presents.
/// @param[out] outNextTargetNs Receives the target of the following present.
/// @return Nanoseconds to sleep, `0` when late.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64 computeVulkanFramePacerSleep(
    gp::UInt64 nowNs, gp::UInt64 nextTargetNs, gp::UInt64 periodNs, gp::UInt64& outNextTargetNs
) noexcept;

/// @brief Picks the queue images are presented from.
/// @details The graphics queue is the default. The async compute queue is used when allowed, able to present to the
///          surface and distinct from the graphics queue: presenting from it lets the next frame's graphics work start
///          without waiting for the present.
/// @param[in] graphicsQueue Graphics queue.
/// @param[in] graphicsSupportsPresent True when the graphics queue family can present to the surface.
/// @param[in] computeQueue Async compute queue, or `nullptr`.
/// @param[in] computeSupportsPresent True when the compute queue family can present to the surface.
/// @param[in] allowPresentOnCompute True to allow presenting from the compute queue.
/// @return Present queue, or `nullptr` when no queue can present.
[[nodiscard]] GP_RHI_VULKAN_API VulkanQueue* selectVulkanPresentQueue(
    VulkanQueue* graphicsQueue,
    bool graphicsSupportsPresent,
    VulkanQueue* computeQueue,
    bool computeSupportsPresent,
    bool allowPresentOnCompute
) noexcept;

}   // namespace gp::rhi::vulkan
