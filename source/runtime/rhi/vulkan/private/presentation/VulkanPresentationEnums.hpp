// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Health of a swapchain after an acquire or a present.
/// @details Every negative value means the swapchain can no longer be used as is and must be recreated (or, for
///          `NotReady`, that the image is simply not available yet). The values are stable so they can be logged and
///          compared across frames.
enum class VulkanSwapChainStatus : gp::Int32
{
    /// @brief The operation succeeded (including `VK_SUBOPTIMAL_KHR`, which still presents correctly).
    Healthy = 0,

    /// @brief The surface changed (resize, mode switch); the swapchain must be recreated against it.
    OutOfDate = -1,

    /// @brief The surface itself is gone; a new surface and a new swapchain are needed.
    SurfaceLost = -2,

    /// @brief No image was available without blocking.
    NotReady = -3,

    /// @brief An unrecoverable error (device lost, out of memory); recreation will not help.
    Failed = -4
};

/// @brief Transfer function and gamut the swapchain images are displayed with.
/// @details Mirrors the values of the `r.HDR.Display.OutputDevice` console variable so the renderer's tonemapper and
///          the swapchain color space always agree.
enum class VulkanDisplayOutputFormat : gp::UInt8
{
    /// @brief 8-bit sRGB transfer function, sRGB primaries.
    SdrSrgb = 0u,

    /// @brief BT.709 transfer function, sRGB primaries.
    SdrRec709,

    /// @brief Explicit gamma mapping, sRGB primaries.
    SdrExplicitGammaMapping,

    /// @brief ST.2084 (PQ) transfer function, BT.2020 primaries, 1000 nit peak.
    HdrAces1000NitSt2084,

    /// @brief ST.2084 (PQ) transfer function, BT.2020 primaries, 2000 nit peak.
    HdrAces2000NitSt2084,

    /// @brief Linear scRGB, 1000 nit peak.
    HdrAces1000NitScRgb,

    /// @brief Linear scRGB, 2000 nit peak.
    HdrAces2000NitScRgb,

    /// @brief Linear output meant for EXR captures.
    HdrLinearExr,

    /// @brief Linear output without tone curve.
    HdrLinearNoToneCurve,

    /// @brief Linear output with tone curve.
    HdrLinearWithToneCurve,

    /// @brief Number of formats; not a valid value.
    Count
};

}   // namespace gp::rhi::vulkan
