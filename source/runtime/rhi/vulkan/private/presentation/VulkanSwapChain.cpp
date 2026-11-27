// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanSwapChain.hpp"
#include "commands/VulkanQueue.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "platforms/base/system/Time.hpp"
#include "presentation/VulkanSwapChainSelection.hpp"
#include "sync/VulkanFence.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "templates/NumericLimits.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Nanoseconds in one second, used to turn a refresh rate into a frame period.
constexpr gp::UInt64 kNanosecondsPerSecond = 1000000000ull;

/// @brief Queries the formats a surface supports.
/// @param[in] physicalDevice Physical device.
/// @param[in] surface Surface to query.
/// @param[out] outFormats Receives the formats.
/// @return Native result.
[[nodiscard]] VkResult querySurfaceFormats(
    const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface, gp::Vector<VkSurfaceFormatKHR>& outFormats
) noexcept
{
    gp::UInt32 count = 0u;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
    if ((result != VK_SUCCESS) || (count == 0u))
    {
        return result;
    }
    outFormats.resize(count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, outFormats.data());
    outFormats.resize(count);
    return result;
}

/// @brief Queries the presentation modes a surface supports.
/// @param[in] physicalDevice Physical device.
/// @param[in] surface Surface to query.
/// @param[out] outModes Receives the modes.
/// @return Native result.
[[nodiscard]] VkResult queryPresentModes(
    const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface, gp::Vector<VkPresentModeKHR>& outModes
) noexcept
{
    gp::UInt32 count = 0u;
    VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr);
    if ((result != VK_SUCCESS) || (count == 0u))
    {
        return result;
    }
    outModes.resize(count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, outModes.data());
    outModes.resize(count);
    return result;
}

/// @brief Reports whether the surface query entry points were loaded.
/// @return True when every query used by swapchain creation is available.
[[nodiscard]] bool hasSurfaceQueryEntryPoints() noexcept
{
    return (vkGetPhysicalDeviceSurfaceCapabilitiesKHR != nullptr) &&
           (vkGetPhysicalDeviceSurfaceFormatsKHR != nullptr) &&
           (vkGetPhysicalDeviceSurfacePresentModesKHR != nullptr) && (vkGetSwapchainImagesKHR != nullptr) &&
           (vkAcquireNextImageKHR != nullptr);
}

}   // namespace

VulkanSwapChain::VulkanSwapChain(const VulkanSwapChainDesc& desc) noexcept
    : m_desc(desc)
{
    GP_CHECKF(m_desc.device != nullptr, "VulkanSwapChain requires a device.");
}

VulkanSwapChain::~VulkanSwapChain() noexcept
{
    if ((m_swapChain != VK_NULL_HANDLE) || (m_surface != VK_NULL_HANDLE))
    {
        destroy(nullptr);
    }
    releaseSyncObjects();
}

bool VulkanSwapChain::initialize(
    const VulkanPlatformWindowContext& windowContext, VulkanSwapChainRecreateInfo* const recreateInfo
) noexcept
{
    m_windowContext = windowContext;
    VulkanDevice& device = *m_desc.device;

    if (!hasSurfaceQueryEntryPoints())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Swapchain entry points are not loaded; was the instance created off screen?");
        return false;
    }

    // Reuse the surface of the retired swapchain when one is handed over; it is still bound to the same window.
    VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE;
    if (recreateInfo != nullptr)
    {
        oldSwapChain = recreateInfo->swapChain;
        recreateInfo->swapChain = VK_NULL_HANDLE;
        if (recreateInfo->surface != VK_NULL_HANDLE)
        {
            m_surface = recreateInfo->surface;
            recreateInfo->surface = VK_NULL_HANDLE;
        }
    }

    if (m_surface == VK_NULL_HANDLE)
    {
        const VkResult surfaceResult =
            VulkanPlatform::createSurface(m_windowContext, device.getInstanceHandle(), &m_surface);
        if (surfaceResult != VK_SUCCESS)
        {
            if (oldSwapChain != VK_NULL_HANDLE)
            {
                VulkanPlatform::destroySwapchainKHR(device.getHandle(), oldSwapChain, GP_VULKAN_CPU_ALLOCATOR);
            }
            // A lost or busy window is not fatal: the owner retries later, so this is a warning, not an error.
            GP_LOG_WARNING(
                LogVulkanRHI, "Creating the window surface failed ({}).", static_cast<gp::Int32>(surfaceResult)
            );
            m_surface = VK_NULL_HANDLE;
            return false;
        }
    }

    const bool created = createSwapChain(oldSwapChain);
    // Passing `oldSwapChain` to `vkCreateSwapchainKHR` only retires it; the application still owns destroying the
    // retired handle whether creation succeeded or failed.
    if (oldSwapChain != VK_NULL_HANDLE)
    {
        VulkanPlatform::destroySwapchainKHR(device.getHandle(), oldSwapChain, GP_VULKAN_CPU_ALLOCATOR);
    }

    if (!created)
    {
        destroy(nullptr);
        return false;
    }

    createSyncObjects();
    return true;
}

bool VulkanSwapChain::createSwapChain(const VkSwapchainKHR oldSwapChain) noexcept
{
    VulkanDevice& device = *m_desc.device;
    const VkPhysicalDevice physicalDevice = device.getPhysicalHandle();

    VkSurfaceCapabilitiesKHR capabilities{};
    const VkResult capsResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &capabilities);
    if (capsResult != VK_SUCCESS)
    {
        GP_LOG_WARNING(
            LogVulkanRHI, "Querying the surface capabilities failed ({}).", static_cast<gp::Int32>(capsResult)
        );
        return false;
    }

    const VkColorSpaceKHR requestedColorSpace = selectVulkanSwapChainColorSpace(m_desc.config.displayOutputFormat);

    // Some platforms report unreliable surface formats and modes; they get the requested format and FIFO, which
    // every implementation must support.
    gp::Vector<VkSurfaceFormatKHR> formats{};
    gp::Vector<VkPresentModeKHR> presentModes{};
    if (VulkanPlatform::supportsQuerySurfaceProperties())
    {
        static_cast<void>(querySurfaceFormats(physicalDevice, m_surface, formats));
        static_cast<void>(queryPresentModes(physicalDevice, m_surface, presentModes));
    }
    else
    {
        const PixelFormat fallbackFormat =
            (m_desc.pixelFormat != PixelFormat::Unknown) ? m_desc.pixelFormat : kVulkanDefaultBackBufferFormat;
        formats.pushBack(VkSurfaceFormatKHR{ pixelFormatToVkFormat(fallbackFormat, false), requestedColorSpace });
        presentModes.pushBack(VK_PRESENT_MODE_FIFO_KHR);
    }

    VulkanSurfaceFormatSelection formatSelection{};
    if (!selectVulkanSurfaceFormat(formats.view(), m_desc.pixelFormat, requestedColorSpace, formatSelection))
    {
        GP_LOG_ERROR(LogVulkanRHI, "The surface reports no format the engine can present.");
        return false;
    }
    if (formatSelection.isFallback)
    {
        GP_LOG_WARNING(
            LogVulkanRHI,
            "Requested swapchain format {} (color space {}) is not supported; using VkFormat {} (color space {}).",
            static_cast<gp::Int32>(m_desc.pixelFormat),
            static_cast<gp::Int32>(requestedColorSpace),
            static_cast<gp::Int32>(formatSelection.surfaceFormat.format),
            static_cast<gp::Int32>(formatSelection.surfaceFormat.colorSpace)
        );
    }

    m_surfaceFormat = formatSelection.surfaceFormat;
    m_pixelFormat = formatSelection.pixelFormat;
    m_presentMode = selectVulkanPresentMode(presentModes.view(), m_desc.lockToVsync, m_desc.config.presentModeOverride);
    m_preTransform = selectVulkanSurfaceTransform(capabilities, m_desc.config.allowPreTransform);
    m_extent = selectVulkanSwapChainExtent(capabilities, m_desc.width, m_desc.height, m_preTransform);
    if ((m_extent.width == 0u) || (m_extent.height == 0u))
    {
        // A minimized window has a zero extent; no swapchain can be created until it is restored.
        GP_LOG_WARNING(LogVulkanRHI, "The surface has a zero extent; the window is probably minimized.");
        return false;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = selectVulkanSwapChainImageCount(capabilities, m_desc.desiredImageCount);
    createInfo.imageFormat = m_surfaceFormat.format;
    createInfo.imageColorSpace = m_surfaceFormat.colorSpace;
    createInfo.imageExtent = m_extent;
    createInfo.imageArrayLayers = 1u;
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = m_preTransform;
    createInfo.compositeAlpha = selectVulkanCompositeAlpha(capabilities.supportedCompositeAlpha);
    createInfo.presentMode = m_presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapChain;

    VkResult createResult = VK_ERROR_INITIALIZATION_FAILED;
#if GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
    VkSurfaceFullScreenExclusiveInfoEXT fullscreenInfo{};
    if (device.getOptionalExtensions().hasEXTFullscreenExclusive)
    {
        fullscreenInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
        fullscreenInfo.fullScreenExclusive =
            m_desc.isFullscreen ? VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT : VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;
        createInfo.pNext = &fullscreenInfo;
    }
#endif

    createResult = VulkanPlatform::createSwapchainKHR(
        m_windowContext, physicalDevice, device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_swapChain
    );

#if GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
    // Some drivers refuse the exclusive fullscreen hint on windows they do not consider eligible.
    if ((createResult == VK_ERROR_INITIALIZATION_FAILED) && (createInfo.pNext != nullptr))
    {
        GP_LOG_WARNING(LogVulkanRHI, "Swapchain creation with exclusive fullscreen failed; retrying without it.");
        createInfo.pNext = nullptr;
        createResult = VulkanPlatform::createSwapchainKHR(
            m_windowContext, physicalDevice, device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_swapChain
        );
    }
#endif

    if (createResult != VK_SUCCESS)
    {
        GP_LOG_WARNING(LogVulkanRHI, "vkCreateSwapchainKHR failed ({}).", static_cast<gp::Int32>(createResult));
        m_swapChain = VK_NULL_HANDLE;
        return false;
    }

    VulkanPlatform::notifySwapchainPresentTiming(device.getHandle(), m_swapChain, false);

    gp::UInt32 imageCount = 0u;
    VkResult imagesResult = vkGetSwapchainImagesKHR(device.getHandle(), m_swapChain, &imageCount, nullptr);
    if ((imagesResult != VK_SUCCESS) || (imageCount == 0u))
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkGetSwapchainImagesKHR failed ({}).", static_cast<gp::Int32>(imagesResult));
        return false;
    }
    m_images.resize(imageCount);
    imagesResult = vkGetSwapchainImagesKHR(device.getHandle(), m_swapChain, &imageCount, m_images.data());
    if (imagesResult != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkGetSwapchainImagesKHR failed ({}).", static_cast<gp::Int32>(imagesResult));
        m_images.clear();
        return false;
    }

    GP_LOG_INFO(
        LogVulkanRHI,
        "Created swapchain {}x{}, {} images, VkFormat {}, present mode {}.",
        m_extent.width,
        m_extent.height,
        imageCount,
        static_cast<gp::Int32>(m_surfaceFormat.format),
        static_cast<gp::Int32>(m_presentMode)
    );
    return true;
}

void VulkanSwapChain::createSyncObjects() noexcept
{
    VulkanDevice& device = *m_desc.device;
    const bool useFences = m_desc.config.cpuWaitForFence && (m_desc.fenceManager != nullptr);

    m_imageAcquiredSemaphores.reserve(m_images.size());
    for (gp::USize index = 0u; index < m_images.size(); ++index)
    {
        // Signaled by the presentation engine, never by a submission of ours.
        m_imageAcquiredSemaphores.pushBack(
            gp::makeShared<VulkanSemaphore>(device, VulkanSemaphoreFlags::ExternallySignaled)
        );
        if (useFences)
        {
            m_imageAcquiredFences.pushBack(m_desc.fenceManager->allocateFence(true));
        }
    }
    m_semaphoreIndex = 0;
    m_currentImageIndex = -1;
}

void VulkanSwapChain::releaseSyncObjects() noexcept
{
    for (VulkanFence*& fence: m_imageAcquiredFences)
    {
        if ((fence != nullptr) && (m_desc.fenceManager != nullptr))
        {
            m_desc.fenceManager->releaseFence(fence);
        }
    }
    m_imageAcquiredFences.clear();
    m_imageAcquiredSemaphores.clear();
}

void VulkanSwapChain::destroy(VulkanSwapChainRecreateInfo* const outRecreateInfo) noexcept
{
    VulkanDevice& device = *m_desc.device;

    // Images may still be read by the presentation engine or written by in-flight submissions.
    device.waitUntilIdle();

    const bool handOver = (outRecreateInfo != nullptr) && m_desc.config.keepSwapChain;
    if (handOver)
    {
        outRecreateInfo->swapChain = m_swapChain;
        outRecreateInfo->surface = m_surface;
    }
    else
    {
        if (m_swapChain != VK_NULL_HANDLE)
        {
            VulkanPlatform::destroySwapchainKHR(device.getHandle(), m_swapChain, GP_VULKAN_CPU_ALLOCATOR);
        }
        if (m_surface != VK_NULL_HANDLE)
        {
            VulkanPlatform::destroySurface(m_windowContext.getWindowId(), device.getInstanceHandle(), m_surface);
        }
    }

    m_swapChain = VK_NULL_HANDLE;
    m_surface = VK_NULL_HANDLE;
    m_images.clear();
    m_currentImageIndex = -1;
    releaseSyncObjects();
}

VulkanSwapChainStatus
    VulkanSwapChain::acquireImageIndex(gp::UInt32& outImageIndex, VulkanSemaphoreRef& outSemaphore) noexcept
{
    if ((m_swapChain == VK_NULL_HANDLE) || m_imageAcquiredSemaphores.isEmpty())
    {
        return VulkanSwapChainStatus::OutOfDate;
    }

    VulkanDevice& device = *m_desc.device;
    const gp::Int32 previousSemaphoreIndex = m_semaphoreIndex;
    m_semaphoreIndex = (m_semaphoreIndex + 1) % static_cast<gp::Int32>(m_imageAcquiredSemaphores.size());
    const gp::USize semaphoreSlot = static_cast<gp::USize>(m_semaphoreIndex);

    VulkanFence* const fence = m_imageAcquiredFences.isEmpty() ? nullptr : m_imageAcquiredFences[semaphoreSlot];
    if (fence != nullptr)
    {
        m_desc.fenceManager->resetFence(fence);
    }

    gp::UInt32 imageIndex = 0u;
    const VkResult result = vkAcquireNextImageKHR(
        device.getHandle(),
        m_swapChain,
        gp::NumericLimits<gp::UInt64>::max(),
        m_imageAcquiredSemaphores[semaphoreSlot]->getHandle(),
        (fence != nullptr) ? fence->getHandle() : VK_NULL_HANDLE,
        &imageIndex
    );

    const VulkanSwapChainStatus status = translateVulkanAcquireResult(result);
    if (status != VulkanSwapChainStatus::Healthy)
    {
        // Nothing will signal the semaphore; give the slot back so the next acquire reuses an unsignaled one.
        m_semaphoreIndex = previousSemaphoreIndex;
        if (status == VulkanSwapChainStatus::Failed)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkAcquireNextImageKHR failed ({}).", static_cast<gp::Int32>(result));
        }
        return status;
    }

    ++m_numAcquireCalls;
    m_currentImageIndex = static_cast<gp::Int32>(imageIndex);
    outImageIndex = imageIndex;
    outSemaphore = m_imageAcquiredSemaphores[semaphoreSlot];

    if (fence != nullptr)
    {
        static_cast<void>(m_desc.fenceManager->waitForFence(fence, gp::NumericLimits<gp::UInt64>::max()));
    }
    return VulkanSwapChainStatus::Healthy;
}

void VulkanSwapChain::paceFrame() noexcept
{
    const bool shouldPace =
        m_desc.config.cpuFramePacer && (m_desc.lockToVsync || m_desc.config.forcePacingWithoutVSync);
    if (!shouldPace || (m_desc.config.framePacerRefreshRate == 0u))
    {
        return;
    }

    const gp::UInt64 periodNs = kNanosecondsPerSecond / m_desc.config.framePacerRefreshRate;
    const gp::UInt64 nowNs = gp::platform::Time::nowNanoseconds();
    const gp::UInt64 sleepNs =
        computeVulkanFramePacerSleep(nowNs, m_nextPresentTargetNs, periodNs, m_nextPresentTargetNs);
    if (sleepNs > 0u)
    {
        gp::platform::Time::sleepNanoseconds(sleepNs);
    }
}

VulkanSwapChainStatus
    VulkanSwapChain::present(VulkanQueue& presentQueue, const VulkanSemaphore* const renderingDoneSemaphore) noexcept
{
    if ((m_currentImageIndex < 0) || (m_swapChain == VK_NULL_HANDLE))
    {
        return VulkanSwapChainStatus::Healthy;
    }

    const VkSemaphore waitSemaphore =
        (renderingDoneSemaphore != nullptr) ? renderingDoneSemaphore->getHandle() : VK_NULL_HANDLE;
    const gp::UInt32 imageIndex = static_cast<gp::UInt32>(m_currentImageIndex);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = (waitSemaphore != VK_NULL_HANDLE) ? 1u : 0u;
    presentInfo.pWaitSemaphores = (waitSemaphore != VK_NULL_HANDLE) ? &waitSemaphore : nullptr;
    presentInfo.swapchainCount = 1u;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &imageIndex;

    // A platform with its own pacing (display timing extensions) takes over; otherwise the CPU pacer may sleep.
    if (!VulkanPlatform::framePace(
            *m_desc.device, m_windowContext.getWindowHandle(), m_swapChain, m_numPresentCalls, presentInfo
        ))
    {
        paceFrame();
    }

    const VkResult result = VulkanPlatform::present(presentQueue.getHandle(), presentInfo);
    m_currentImageIndex = -1;
    ++m_numPresentCalls;

    const VulkanSwapChainStatus status = translateVulkanPresentResult(result);
    if (status == VulkanSwapChainStatus::Failed)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkQueuePresentKHR failed ({}).", static_cast<gp::Int32>(result));
    }
    return status;
}

}   // namespace gp::rhi::vulkan
