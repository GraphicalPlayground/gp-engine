// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanSwapChainSelection.hpp"
#include "CoreMinimal.hpp"
#include "translation/VulkanPixelFormat.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Value of `currentExtent` meaning "the swapchain decides the extent".
constexpr gp::UInt32 kVulkanUndefinedSurfaceExtent = 0xFFFFFFFFu;

/// @brief Clamps `value` into `[minValue, maxValue]`.
/// @param[in] value Value to clamp.
/// @param[in] minValue Lower bound.
/// @param[in] maxValue Upper bound; ignored when smaller than `minValue`.
/// @return Clamped value.
[[nodiscard]] constexpr gp::UInt32
    clampExtent(const gp::UInt32 value, const gp::UInt32 minValue, const gp::UInt32 maxValue) noexcept
{
    const gp::UInt32 lowered = (maxValue >= minValue && value > maxValue) ? maxValue : value;
    return (lowered < minValue) ? minValue : lowered;
}

/// @brief Reports whether `modes` contains `mode`.
/// @param[in] modes Available present modes.
/// @param[in] mode Mode to look for.
/// @return True when found.
[[nodiscard]] bool
    containsPresentMode(const gp::ConstVectorView<VkPresentModeKHR> modes, const VkPresentModeKHR mode) noexcept
{
    for (const VkPresentModeKHR available: modes)
    {
        if (available == mode)
        {
            return true;
        }
    }
    return false;
}

/// @brief Finds the first reported format the engine has a pixel format for.
/// @param[in] availableFormats Reported surface formats.
/// @param[in] requiredColorSpace Color space the entry must use; ignored when `anyColorSpace` is true.
/// @param[in] anyColorSpace True to accept every color space.
/// @param[out] outSelection Receives the entry on success.
/// @return True when an entry was found.
[[nodiscard]] bool findFirstKnownFormat(
    const gp::ConstVectorView<VkSurfaceFormatKHR> availableFormats,
    const VkColorSpaceKHR requiredColorSpace,
    const bool anyColorSpace,
    VulkanSurfaceFormatSelection& outSelection
) noexcept
{
    for (const VkSurfaceFormatKHR& available: availableFormats)
    {
        if (!anyColorSpace && (available.colorSpace != requiredColorSpace))
        {
            continue;
        }

        const PixelFormat pixelFormat = vkFormatToPixelFormat(available.format);
        if (pixelFormat != PixelFormat::Unknown)
        {
            outSelection.surfaceFormat = available;
            outSelection.pixelFormat = pixelFormat;
            outSelection.isFallback = true;
            return true;
        }
    }
    return false;
}

}   // namespace

VkColorSpaceKHR selectVulkanSwapChainColorSpace(const VulkanDisplayOutputFormat outputFormat) noexcept
{
    switch (outputFormat)
    {
    case VulkanDisplayOutputFormat::SdrRec709:
        return VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
    case VulkanDisplayOutputFormat::HdrAces1000NitSt2084:
    case VulkanDisplayOutputFormat::HdrAces2000NitSt2084:
        return VK_COLOR_SPACE_HDR10_ST2084_EXT;
    case VulkanDisplayOutputFormat::SdrSrgb:
    default:
        break;
    }
    return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

bool selectVulkanSurfaceFormat(
    const gp::ConstVectorView<VkSurfaceFormatKHR> availableFormats,
    const PixelFormat requestedFormat,
    const VkColorSpaceKHR requestedColorSpace,
    VulkanSurfaceFormatSelection& outSelection
) noexcept
{
    if (availableFormats.isEmpty())
    {
        return false;
    }

    const PixelFormat wantedFormat =
        (requestedFormat != PixelFormat::Unknown) ? requestedFormat : kVulkanDefaultBackBufferFormat;
    const VkFormat wantedVkFormat = pixelFormatToVkFormat(wantedFormat, false);

    // A single UNDEFINED entry is the legacy way for a surface to say "no preference".
    if ((availableFormats.size() == 1u) && (availableFormats[0u].format == VK_FORMAT_UNDEFINED) &&
        (wantedVkFormat != VK_FORMAT_UNDEFINED))
    {
        outSelection.surfaceFormat = VkSurfaceFormatKHR{ wantedVkFormat, requestedColorSpace };
        outSelection.pixelFormat = wantedFormat;
        outSelection.isFallback = false;
        return true;
    }

    if (wantedVkFormat != VK_FORMAT_UNDEFINED)
    {
        for (const VkSurfaceFormatKHR& available: availableFormats)
        {
            if ((available.format == wantedVkFormat) && (available.colorSpace == requestedColorSpace))
            {
                outSelection.surfaceFormat = available;
                outSelection.pixelFormat = wantedFormat;
                outSelection.isFallback = false;
                return true;
            }
        }
    }

    if (findFirstKnownFormat(availableFormats, requestedColorSpace, false, outSelection))
    {
        return true;
    }
    return findFirstKnownFormat(availableFormats, requestedColorSpace, true, outSelection);
}

VkPresentModeKHR selectVulkanPresentMode(
    const gp::ConstVectorView<VkPresentModeKHR> availableModes, const bool lockToVsync, const gp::Int32 overrideMode
) noexcept
{
    if (availableModes.isEmpty())
    {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    if (overrideMode >= 0)
    {
        const VkPresentModeKHR forcedMode = static_cast<VkPresentModeKHR>(overrideMode);
        if (containsPresentMode(availableModes, forcedMode))
        {
            return forcedMode;
        }
    }

    if (!lockToVsync && containsPresentMode(availableModes, VK_PRESENT_MODE_IMMEDIATE_KHR))
    {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    if (containsPresentMode(availableModes, VK_PRESENT_MODE_MAILBOX_KHR))
    {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    if (containsPresentMode(availableModes, VK_PRESENT_MODE_FIFO_KHR))
    {
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    return availableModes[0u];
}

gp::UInt32 selectVulkanSwapChainImageCount(
    const VkSurfaceCapabilitiesKHR& capabilities, const gp::UInt32 desiredCount
) noexcept
{
    gp::UInt32 imageCount = (desiredCount < capabilities.minImageCount) ? capabilities.minImageCount : desiredCount;
    if ((capabilities.maxImageCount > 0u) && (imageCount > capabilities.maxImageCount))
    {
        imageCount = capabilities.maxImageCount;
    }
    return imageCount;
}

VkSurfaceTransformFlagBitsKHR
    selectVulkanSurfaceTransform(const VkSurfaceCapabilitiesKHR& capabilities, const bool allowPreTransform) noexcept
{
    if (allowPreTransform)
    {
        return capabilities.currentTransform;
    }
    if ((capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0u)
    {
        return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    return capabilities.currentTransform;
}

bool isVulkanQuarterTurnTransform(const VkSurfaceTransformFlagBitsKHR transform) noexcept
{
    constexpr VkSurfaceTransformFlagsKHR kQuarterTurnMask = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
                                                            VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR |
                                                            VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR |
                                                            VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR;
    return (static_cast<VkSurfaceTransformFlagsKHR>(transform) & kQuarterTurnMask) != 0u;
}

VkExtent2D selectVulkanSwapChainExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    const gp::UInt32 desiredWidth,
    const gp::UInt32 desiredHeight,
    const VkSurfaceTransformFlagBitsKHR transform
) noexcept
{
    if (capabilities.currentExtent.width != kVulkanUndefinedSurfaceExtent)
    {
        return capabilities.currentExtent;
    }

    // The requested size is in window orientation; a quarter-turn pre-transform presents the image rotated.
    const bool swapAxes = isVulkanQuarterTurnTransform(transform);
    const gp::UInt32 width = swapAxes ? desiredHeight : desiredWidth;
    const gp::UInt32 height = swapAxes ? desiredWidth : desiredHeight;

    VkExtent2D extent{};
    extent.width = clampExtent(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = clampExtent(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

VkCompositeAlphaFlagBitsKHR selectVulkanCompositeAlpha(const VkCompositeAlphaFlagsKHR supportedFlags) noexcept
{
    if ((supportedFlags & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0u)
    {
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    if ((supportedFlags & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) != 0u)
    {
        return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }

    // Lowest set bit: x & (~x + 1) isolates it without signed arithmetic.
    const VkCompositeAlphaFlagsKHR lowestBit = supportedFlags & (~supportedFlags + 1u);
    return (lowestBit != 0u) ? static_cast<VkCompositeAlphaFlagBitsKHR>(lowestBit) : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

VulkanSwapChainStatus translateVulkanAcquireResult(const VkResult result) noexcept
{
    switch (result)
    {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        return VulkanSwapChainStatus::Healthy;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return VulkanSwapChainStatus::OutOfDate;
    case VK_ERROR_SURFACE_LOST_KHR:
        return VulkanSwapChainStatus::SurfaceLost;
    case VK_NOT_READY:
    case VK_TIMEOUT:
        return VulkanSwapChainStatus::NotReady;
    default:
        break;
    }
    return VulkanSwapChainStatus::Failed;
}

VulkanSwapChainStatus translateVulkanPresentResult(const VkResult result) noexcept
{
    switch (result)
    {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        return VulkanSwapChainStatus::Healthy;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return VulkanSwapChainStatus::OutOfDate;
    case VK_ERROR_SURFACE_LOST_KHR:
        return VulkanSwapChainStatus::SurfaceLost;
    default:
        break;
    }
    return VulkanSwapChainStatus::Failed;
}

gp::UInt64 computeVulkanFramePacerSleep(
    const gp::UInt64 nowNs, const gp::UInt64 nextTargetNs, const gp::UInt64 periodNs, gp::UInt64& outNextTargetNs
) noexcept
{
    if (nowNs < nextTargetNs)
    {
        outNextTargetNs = nextTargetNs + periodNs;
        return nextTargetNs - nowNs;
    }

    outNextTargetNs = nowNs + periodNs;
    return 0u;
}

VulkanQueue* selectVulkanPresentQueue(
    VulkanQueue* const graphicsQueue,
    const bool graphicsSupportsPresent,
    VulkanQueue* const computeQueue,
    const bool computeSupportsPresent,
    const bool allowPresentOnCompute
) noexcept
{
    if (allowPresentOnCompute && (computeQueue != nullptr) && (computeQueue != graphicsQueue) && computeSupportsPresent)
    {
        return computeQueue;
    }
    return graphicsSupportsPresent ? graphicsQueue : nullptr;
}

}   // namespace gp::rhi::vulkan
