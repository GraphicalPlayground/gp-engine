// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "contexts/VulkanContextForward.hpp"            // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"             // IWYU pragma: keep
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "presentation/VulkanPresentationTypes.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"       // IWYU pragma: keep
#include "resources/VulkanResourceServices.hpp"
#include "RHIForward.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Number of images a viewport asks its swapchain for (triple buffering).
inline constexpr gp::UInt32 kVulkanNumRequestedSwapChainImages = 3u;

/// @brief Creation parameters of a `VulkanSwapChain`.
struct VulkanSwapChainDesc
{
    /// @brief Device the swapchain is created on. Required.
    VulkanDevice* device{ nullptr };

    /// @brief Fence manager backing CPU waits on acquired images; required only with `config.cpuWaitForFence`.
    VulkanFenceManager* fenceManager{ nullptr };

    /// @brief Requested engine pixel format; `Unknown` picks the platform default.
    PixelFormat pixelFormat{ PixelFormat::Unknown };

    /// @brief Requested width in pixels; the surface may impose another extent.
    gp::UInt32 width{ 0u };

    /// @brief Requested height in pixels; the surface may impose another extent.
    gp::UInt32 height{ 0u };

    /// @brief Requested number of images, clamped to the surface limits.
    gp::UInt32 desiredImageCount{ kVulkanNumRequestedSwapChainImages };

    /// @brief True to ask for exclusive fullscreen where the platform supports it.
    bool isFullscreen{ false };

    /// @brief True to present on vertical blank (selects `FIFO`/`MAILBOX` over `IMMEDIATE`).
    bool lockToVsync{ false };

    /// @brief Presentation settings read when the swapchain is created.
    VulkanPresentationConfig config{};
};

/// @brief Non-owning bundle of the device-level services a `VulkanViewport` presents with.
/// @details The dynamic RHI owns every service. Optional services may be `nullptr`: without a compute queue the
///          graphics queue presents, without a submitter frame events degrade to blocking sync point waits, and
///          without a render pass manager no framebuffer cache needs invalidating.
struct VulkanViewportServices
{
    /// @brief Device, memory and deferred deletion services used for the swapchain textures. Required.
    VulkanResourceServices resourceServices{};

    /// @brief Fence manager backing `r.Vulkan.CpuWaitForFence`.
    VulkanFenceManager* fenceManager{ nullptr };

    /// @brief Queue graphics work is submitted to; always able to present. Required.
    VulkanQueue* graphicsQueue{ nullptr };

    /// @brief Async compute queue, used for presentation when allowed and supported.
    VulkanQueue* computeQueue{ nullptr };

    /// @brief Submitter whose interrupt processing completes frame events.
    IVulkanPayloadSubmitter* submitter{ nullptr };

    /// @brief Framebuffer cache notified when swapchain images are destroyed.
    VulkanRenderPassManager* renderPassManager{ nullptr };

    /// @brief Reports whether the required services are present.
    /// @return True with valid resource services and a graphics queue.
    [[nodiscard]] bool isValid() const noexcept
    {
        return resourceServices.isValid() && (graphicsQueue != nullptr);
    }
};

}   // namespace gp::rhi::vulkan
