// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"       // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"   // IWYU pragma: keep
#include "platform/VulkanPlatform.hpp"
#include "presentation/VulkanPresentationDesc.hpp"
#include "presentation/VulkanPresentationEnums.hpp"
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "presentation/VulkanPresentationTypes.hpp"
#include "RHIForward.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Platform swapchain: a window surface, the images presented on it and the acquire synchronization.
/// @details Lifecycle of one frame:
///          1. `acquireImageIndex()` asks the presentation engine for an image. The engine may still be reading it, so
///             the call returns at once and an "image acquired" semaphore is signaled when it is really free. One
///             semaphore exists per image and they are used round robin; the graphics submission writing the image
///             waits on the returned one.
///          2. The frame renders into the image.
///          3. `present()` queues the image back, waiting on the "rendering done" semaphore the graphics submission
///             signals.
///
///          Errors are not fatal: an `OutOfDate` or `SurfaceLost` status tells the owner to destroy this swapchain and
///          create another one. `destroy()` can hand the native swapchain and surface to the replacement
///          (`VulkanSwapChainRecreateInfo`), which lets the presentation engine finish showing the old images.
/// @note Not thread-safe; the owning viewport serializes every call on the RHI timeline.
class GP_RHI_VULKAN_API VulkanSwapChain final
{
public:
    /// @brief Stores the creation parameters; nothing native is created until `initialize()`.
    /// @param[in] desc Creation parameters; `desc.device` is required.
    explicit VulkanSwapChain(const VulkanSwapChainDesc& desc) noexcept;

    /// @brief Destroys the swapchain and its surface if `destroy()` was not called.
    ~VulkanSwapChain() noexcept;

    VulkanSwapChain(const VulkanSwapChain&) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;
    VulkanSwapChain(VulkanSwapChain&&) = delete;
    VulkanSwapChain& operator=(VulkanSwapChain&&) = delete;

public:
    /// @brief Creates (or adopts) the surface, then the swapchain and its synchronization objects.
    /// @details Steps: surface (taken from `recreateInfo` or created through the platform), surface queries, color
    ///          space, format, present mode, image count, transform, extent and composite alpha selection,
    ///          `vkCreateSwapchainKHR` (with exclusive fullscreen when available, retried without it), image retrieval
    ///          and one acquire semaphore (and optional fence) per image.
    /// @param[in] windowContext Window the surface is created for.
    /// @param[in,out] recreateInfo Retired swapchain and surface to reuse, or `nullptr`; the surface is taken.
    /// @return True on success. On failure every native object created here is released.
    [[nodiscard]] bool initialize(
        const VulkanPlatformWindowContext& windowContext, VulkanSwapChainRecreateInfo* recreateInfo
    ) noexcept;

    /// @brief Releases the swapchain, its surface and its synchronization objects.
    /// @details Waits for the device to go idle first, since images may still be in flight. With a non-null
    ///          `outRecreateInfo` and `r.Vulkan.KeepSwapChain`, the native swapchain and surface are handed over
    ///          instead of destroyed.
    /// @param[out] outRecreateInfo Receives the retired swapchain and surface, or `nullptr` to destroy them.
    void destroy(VulkanSwapChainRecreateInfo* outRecreateInfo) noexcept;

    /// @brief Acquires the next image to render into.
    /// @details The semaphore index advances before the call and is restored when the swapchain is out of date or the
    ///          surface lost, so a failed acquire leaves no semaphore pending a signal.
    /// @param[out] outImageIndex Receives the acquired image index on success.
    /// @param[out] outSemaphore Receives the semaphore signaled once the image is free, on success.
    /// @return `Healthy` on success, or the reason the image could not be acquired.
    [[nodiscard]] VulkanSwapChainStatus
        acquireImageIndex(gp::UInt32& outImageIndex, VulkanSemaphoreRef& outSemaphore) noexcept;

    /// @brief Queues the acquired image for presentation.
    /// @details Optionally sleeps first to pace frames (`r.Vulkan.CPURHIThreadFramePacer`). Does nothing when no image
    ///          is acquired. The image is released whatever the result.
    /// @param[in] presentQueue Queue to present from.
    /// @param[in] renderingDoneSemaphore Semaphore signaled by the work writing the image, or `nullptr`.
    /// @return `Healthy` on success, or the reason the present failed.
    [[nodiscard]] VulkanSwapChainStatus
        present(VulkanQueue& presentQueue, const VulkanSemaphore* renderingDoneSemaphore) noexcept;

    /// @brief Changes the vertical sync state used by the frame pacer.
    /// @note The present mode itself only changes when the swapchain is recreated.
    /// @param[in] lockToVsync True to pace on vertical blank.
    void setLockToVsync(const bool lockToVsync) noexcept
    {
        m_desc.lockToVsync = lockToVsync;
    }

    /// @brief Returns the native swapchain.
    /// @return Swapchain handle, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkSwapchainKHR getHandle() const noexcept
    {
        return m_swapChain;
    }

    /// @brief Returns the surface the swapchain presents on.
    /// @return Surface handle, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkSurfaceKHR getSurface() const noexcept
    {
        return m_surface;
    }

    /// @brief Returns the presentable images, owned by the swapchain.
    /// @return Images in swapchain order.
    [[nodiscard]] const gp::Vector<VkImage>& getImages() const noexcept
    {
        return m_images;
    }

    /// @brief Returns the number of presentable images.
    /// @return Image count.
    [[nodiscard]] gp::UInt32 getNumImages() const noexcept
    {
        return static_cast<gp::UInt32>(m_images.size());
    }

    /// @brief Returns the engine pixel format of the images.
    /// @return Pixel format chosen at creation.
    [[nodiscard]] PixelFormat getPixelFormat() const noexcept
    {
        return m_pixelFormat;
    }

    /// @brief Returns the native surface format of the images.
    /// @return Format and color space chosen at creation.
    [[nodiscard]] const VkSurfaceFormatKHR& getSurfaceFormat() const noexcept
    {
        return m_surfaceFormat;
    }

    /// @brief Returns the image extent, which the surface may have imposed.
    /// @return Width and height in pixels.
    [[nodiscard]] const VkExtent2D& getExtent() const noexcept
    {
        return m_extent;
    }

    /// @brief Returns the presentation mode chosen at creation.
    /// @return Present mode.
    [[nodiscard]] VkPresentModeKHR getPresentMode() const noexcept
    {
        return m_presentMode;
    }

    /// @brief Returns the transform the images are presented with.
    /// @return Surface transform.
    [[nodiscard]] VkSurfaceTransformFlagBitsKHR getPreTransform() const noexcept
    {
        return m_preTransform;
    }

    /// @brief Returns the index of the currently acquired image.
    /// @return Image index, or `-1` when none is acquired.
    [[nodiscard]] gp::Int32 getCurrentImageIndex() const noexcept
    {
        return m_currentImageIndex;
    }

    /// @brief Returns the number of successful acquires.
    /// @return Acquire count.
    [[nodiscard]] gp::UInt32 getNumAcquireCalls() const noexcept
    {
        return m_numAcquireCalls;
    }

    /// @brief Returns the number of presents issued.
    /// @return Present count.
    [[nodiscard]] gp::UInt32 getNumPresentCalls() const noexcept
    {
        return m_numPresentCalls;
    }

private:
    /// @brief Creates the native swapchain from the current surface.
    /// @param[in] oldSwapChain Retired swapchain passed as `oldSwapchain`, or `VK_NULL_HANDLE`.
    /// @return True on success.
    [[nodiscard]] bool createSwapChain(VkSwapchainKHR oldSwapChain) noexcept;

    /// @brief Creates one acquire semaphore (and fence) per image.
    void createSyncObjects() noexcept;

    /// @brief Releases the acquire semaphores and fences.
    void releaseSyncObjects() noexcept;

    /// @brief Sleeps until the next paced present when the frame pacer is active.
    void paceFrame() noexcept;

    VulkanSwapChainDesc m_desc{};
    VulkanPlatformWindowContext m_windowContext{};
    VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
    VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
    gp::Vector<VkImage> m_images{};
    gp::Vector<VulkanSemaphoreRef> m_imageAcquiredSemaphores{};
    gp::Vector<VulkanFence*> m_imageAcquiredFences{};
    VkSurfaceFormatKHR m_surfaceFormat{ VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    VkExtent2D m_extent{ 0u, 0u };
    gp::UInt64 m_nextPresentTargetNs{ 0u };
    PixelFormat m_pixelFormat{ PixelFormat::Unknown };
    VkPresentModeKHR m_presentMode{ VK_PRESENT_MODE_FIFO_KHR };
    VkSurfaceTransformFlagBitsKHR m_preTransform{ VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR };
    gp::Int32 m_currentImageIndex{ -1 };
    gp::Int32 m_semaphoreIndex{ 0 };
    gp::UInt32 m_numAcquireCalls{ 0u };
    gp::UInt32 m_numPresentCalls{ 0u };
};

}   // namespace gp::rhi::vulkan
