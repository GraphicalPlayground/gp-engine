// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanViewport.hpp"
#include "barriers/VulkanPipelineBarrier.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanQueue.hpp"
#include "commands/VulkanSubmission.hpp"
#include "contexts/IVulkanPayloadSubmitter.hpp"
#include "contexts/VulkanCommandContextProvider.hpp"
#include "contexts/VulkanCommandListContext.hpp"
#include "contexts/VulkanContextEnums.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "presentation/CustomPresent.hpp"
#include "presentation/PresentArgs.hpp"
#include "presentation/VulkanBackBuffer.hpp"
#include "presentation/VulkanPresentationSettings.hpp"
#include "presentation/VulkanSwapChain.hpp"
#include "presentation/VulkanSwapChainSelection.hpp"
#include "presentation/VulkanViewportRegistry.hpp"
#include "renderpass/VulkanRenderPassManager.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanTexture.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "templates/NumericLimits.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Creation flags shared by the back buffer, the rendering back buffer and the swapchain image textures.
constexpr resource::TextureCreateFlags kBackBufferFlags = resource::TextureCreateFlags::RenderTargetable |
                                                          resource::TextureCreateFlags::ShaderResource |
                                                          resource::TextureCreateFlags::ResolveTargetable;

/// @brief Reports whether a status is fixed by recreating the swapchain.
/// @param[in] status Acquire or present status.
/// @return True for `OutOfDate` and `SurfaceLost`.
[[nodiscard]] bool isRecoverableStatus(const VulkanSwapChainStatus status) noexcept
{
    return (status == VulkanSwapChainStatus::OutOfDate) || (status == VulkanSwapChainStatus::SurfaceLost);
}

/// @brief Reports whether `queue` can present to `surface`.
/// @param[in] device Device owning the queue.
/// @param[in] queue Candidate queue, or `nullptr`.
/// @param[in] surface Surface to present to.
/// @return True when the queue family supports presenting to the surface.
[[nodiscard]] bool queueSupportsPresent(
    const VulkanDevice& device, const VulkanQueue* const queue, const VkSurfaceKHR surface
) noexcept
{
    if ((queue == nullptr) || (surface == VK_NULL_HANDLE) || (vkGetPhysicalDeviceSurfaceSupportKHR == nullptr))
    {
        return false;
    }
    VkBool32 supported = VK_FALSE;
    const VkResult result =
        vkGetPhysicalDeviceSurfaceSupportKHR(device.getPhysicalHandle(), queue->getFamilyIndex(), surface, &supported);
    return (result == VK_SUCCESS) && (supported == VK_TRUE);
}

}   // namespace

VulkanViewport::VulkanViewport(
    const VulkanViewportServices& services,
    const VulkanPlatformWindowContext& windowContext,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
    : m_services(services)
    , m_windowContext(windowContext)
    , m_config(resolveVulkanPresentationConfig())
    , m_sizeX(sizeX)
    , m_sizeY(sizeY)
    , m_pixelFormat(
          (preferredPixelFormat != PixelFormat::Unknown) ? preferredPixelFormat : kVulkanDefaultBackBufferFormat
      )
    , m_isFullscreen(isFullscreen)
    , m_lockToVsync(m_config.lockToVsync)
{
    if (m_services.viewportRegistry != nullptr)
    {
        m_services.viewportRegistry->add(this);
    }
}

VulkanViewport::~VulkanViewport() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (m_services.viewportRegistry != nullptr)
    {
        static_cast<void>(m_services.viewportRegistry->remove(this));
        m_services.viewportRegistry = nullptr;
    }
    releaseAcquiredImage();
    if (m_rhiBackBuffer)
    {
        m_rhiBackBuffer->detachViewport();
    }
    if (m_backBuffer)
    {
        m_backBuffer->detachViewport();
    }
    destroySwapChain(nullptr);
}

void VulkanViewport::releaseNativeResources() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    // The registry already dropped this viewport and may die first: never touch it again.
    m_services.viewportRegistry = nullptr;

    releaseAcquiredImage();
    if (m_rhiBackBuffer)
    {
        m_rhiBackBuffer->detachViewport();
    }
    if (m_backBuffer && (m_backBuffer != m_rhiBackBuffer))
    {
        m_backBuffer->detachViewport();
    }
    destroySwapChain(nullptr);

    // The off-screen fallbacks own device memory: drop them now so the RHI shutdown flush destroys them while the
    // device is alive. The back buffers own nothing but are dropped too, leaving the viewport fully inert.
    m_rhiRenderingBackBuffer.reset();
    m_renderingBackBuffer.reset();
    m_rhiBackBuffer.reset();
    m_backBuffer.reset();
    {
        gp::thread::UniqueLock<gp::thread::Mutex> frameLock(m_frameEventMutex);
        m_lastFrameSyncPoint.reset();
    }
}

bool VulkanViewport::initialize(command::CommandListBase& cmdList) noexcept
{
    GP_CHECKF(m_services.isValid(), "VulkanViewport requires valid services.");

    gp::UInt32 width = m_sizeX;
    gp::UInt32 height = m_sizeY;
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        if (!createSwapChain(nullptr))
        {
            GP_LOG_ERROR(LogVulkanRHI, "{}", "VulkanViewport: failed to create the swapchain.");
            return false;
        }
        // The surface may impose its own extent (rotated mobile screens) and the format may have fallen back.
        width = m_swapChain->getExtent().width;
        height = m_swapChain->getExtent().height;
        m_pixelFormat = m_swapChain->getPixelFormat();
    }

    VulkanBackBufferRef backBuffer{};
    gp::RefCountedPtr<VulkanTexture> renderingBackBuffer{};
    createBackBuffers(cmdList, width, height, m_pixelFormat, backBuffer, renderingBackBuffer);
    m_backBuffer = backBuffer;
    m_renderingBackBuffer = renderingBackBuffer;

    // Nothing runs on the RHI timeline for this viewport yet, so the RHI side is installed directly.
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    installBackBuffers(backBuffer, renderingBackBuffer, false);
    return true;
}

void VulkanViewport::resize(
    command::CommandListBase& cmdList,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    const PixelFormat newFormat = (preferredPixelFormat != PixelFormat::Unknown) ? preferredPixelFormat : m_pixelFormat;
    if ((sizeX == m_sizeX) && (sizeY == m_sizeY) && (isFullscreen == m_isFullscreen) && (newFormat == m_pixelFormat))
    {
        return;
    }

    {
        // The RHI timeline reads these when recreating the swapchain.
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        m_sizeX = sizeX;
        m_sizeY = sizeY;
        m_isFullscreen = isFullscreen;
        m_pixelFormat = newFormat;
    }

    VulkanBackBufferRef backBuffer{};
    gp::RefCountedPtr<VulkanTexture> renderingBackBuffer{};
    createBackBuffers(cmdList, sizeX, sizeY, newFormat, backBuffer, renderingBackBuffer);
    m_backBuffer = backBuffer;
    m_renderingBackBuffer = renderingBackBuffer;

    // Every command recorded so far still targets the old back buffer, so the switch happens after them.
    const VulkanViewportRef self(this);
    cmdList.enqueueLambda(
        [self, backBuffer, renderingBackBuffer](command::CommandListBase& executingCmdList) -> void
    {
        static_cast<void>(executingCmdList);
        gp::thread::UniqueLock<gp::thread::Mutex> lock(self->m_mutex);
        self->installBackBuffers(backBuffer, renderingBackBuffer, true);
    }
    );
}

VulkanBackBuffer* VulkanViewport::getBackBuffer() const noexcept
{
    return m_backBuffer.get();
}

void VulkanViewport::advanceBackBufferFrame() noexcept
{
    // The back buffer is persistent and borrows a new image on first use each frame; an image acquired but not
    // presented (present skipped) stays bound for the next frame, since Vulkan has no way to give it back unused.
}

bool VulkanViewport::acquireBackBufferImage(
    command::CommandListBase& executingCmdList, VulkanBackBuffer& backBuffer
) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (backBuffer.hasAcquiredImage())
    {
        return m_acquiredImageIndex >= 0;
    }

    // A different back buffer still bound to the previous image (replaced by a resize before it presented) must not
    // keep a reference to a swapchain image it will never present.
    if (m_acquiredBackBuffer && (m_acquiredBackBuffer.get() != &backBuffer))
    {
        releaseAcquiredImage();
    }

    gp::UInt32 imageIndex = 0u;
    VulkanSemaphoreRef acquiredSemaphore{};
    const VulkanSwapChainStatus status = acquireWithRecovery(imageIndex, acquiredSemaphore);
    if ((status != VulkanSwapChainStatus::Healthy) || (imageIndex >= m_images.size()) || !m_images[imageIndex].texture)
    {
        // Nothing to present this frame: render into the off-screen fallback.
        if (m_rhiRenderingBackBuffer)
        {
            backBuffer.bindAcquiredImage(*m_rhiRenderingBackBuffer);
        }
        m_acquiredImageIndex = -1;
        m_acquiredBackBuffer = VulkanBackBufferRef(&backBuffer);
        return false;
    }

    VulkanCommandListContext& context = VulkanCommandContextProvider::getVulkanContext(executingCmdList);
    // The presentation engine may still read the image: every write waits on the acquire semaphore.
    context.addWaitSemaphore(
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, acquiredSemaphore
    );

    SwapChainImage& slot = m_images[imageIndex];
    if (slot.isFirstUse)
    {
        initializeImageOnFirstUse(context, slot.texture->getImage());
        slot.isFirstUse = false;
    }

    backBuffer.bindAcquiredImage(*slot.texture);
    m_acquiredImageIndex = static_cast<gp::Int32>(imageIndex);
    m_acquiredBackBuffer = VulkanBackBufferRef(&backBuffer);
    return true;
}

bool VulkanViewport::present(
    command::CommandListBase& executingCmdList, const presentation::PresentArgsWithStats& presentArgs
) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    VulkanCommandListContext& context = VulkanCommandContextProvider::getVulkanContext(executingCmdList);

    if (m_presentBeginLatencyCallback)
    {
        m_presentBeginLatencyCallback();
    }

    bool needsNativePresent = true;
    if (m_customPresent)
    {
        gp::Int32 syncInterval = presentArgs.isLockedToVsync() ? 1 : 0;
        needsNativePresent = m_customPresent->needsNativePresent();
        if (needsNativePresent)
        {
            needsNativePresent = m_customPresent->present(this, context, syncInterval);
        }
    }

    const bool hasImage =
        (m_acquiredImageIndex >= 0) && (static_cast<gp::USize>(m_acquiredImageIndex) < m_images.size());
    const bool willPresent = presentArgs.shouldPresent() && needsNativePresent && hasImage && m_swapChain;
    const VulkanSemaphore* renderingDoneSemaphore = nullptr;
    if (willPresent)
    {
        const VulkanSemaphoreRef& semaphore =
            m_images[static_cast<gp::USize>(m_acquiredImageIndex)].renderingDoneSemaphore;
        context.addSignalSemaphore(semaphore);
        renderingDoneSemaphore = semaphore.get();
    }

    {
        // The sync point of this submission is the frame event waited on by `waitForFrameEventCompletion()`.
        const VulkanSyncPointRef& frameSyncPoint = context.getContextSyncPoint();
        gp::thread::UniqueLock<gp::thread::Mutex> frameLock(m_frameEventMutex);
        m_lastFrameSyncPoint = frameSyncPoint;
    }

    if ((m_services.resourceServices.contextProvider != nullptr) && (m_services.submitter != nullptr))
    {
        // Texture creation and staged uploads record into the shared upload context; submit them before the frame's
        // graphics work so layout transitions and copies are visible on the queue.
        VulkanPlatformCommandList uploadCommandList{};
        if (m_services.resourceServices.contextProvider->finalizeUploads(uploadCommandList))
        {
            m_services.submitter->submitPayloads(uploadCommandList.payloads);
        }
    }

    // The present must reach the queue after the submission signaling the semaphore.
    static_cast<void>(context.flushCommands(VulkanFlushFlags::WaitForSubmission));

    bool presented = false;
    if (willPresent)
    {
        VulkanQueue& presentQueue = (m_presentQueue != nullptr) ? *m_presentQueue : context.getQueue();
        const VulkanSwapChainStatus status = m_swapChain->present(presentQueue, renderingDoneSemaphore);
        presented = true;
        ++m_presentCount;
        releaseAcquiredImage();

        if (isRecoverableStatus(status) && VulkanPlatform::recreateSwapchainOnFail())
        {
            // The image already went to the presentation engine (or was dropped); the next acquire uses the new one.
            static_cast<void>(recreateSwapChain(status == VulkanSwapChainStatus::SurfaceLost));
        }
        else if (status == VulkanSwapChainStatus::Failed)
        {
            GP_LOG_ERROR(LogVulkanRHI, "{}", "VulkanViewport: vkQueuePresentKHR failed.");
        }
    }
    else if (m_acquiredImageIndex < 0)
    {
        // The fallback binding is per frame; a real image stays bound until it is presented.
        releaseAcquiredImage();
    }

    if (m_customPresent)
    {
        m_customPresent->postPresent();
    }

    if (m_presentEndLatencyCallback)
    {
        m_presentEndLatencyCallback();
    }

    if ((presentArgs.isLockedToVsync() != m_lockToVsync) && (m_acquiredImageIndex < 0))
    {
        // The present mode is fixed at creation: a vertical sync change needs a new swapchain. With an image still
        // acquired (present skipped), the change waits for the frame that presents it.
        m_lockToVsync = presentArgs.isLockedToVsync();
        static_cast<void>(recreateSwapChain(false));
    }

    return presented;
}

void* VulkanViewport::getNativeSwapChain() const noexcept
{
    return m_swapChain ? reinterpret_cast<void*>(m_swapChain->getHandle()) : nullptr;
}

void* VulkanViewport::getNativeBackBufferTexture() const noexcept
{
    return m_rhiBackBuffer ? reinterpret_cast<void*>(m_rhiBackBuffer->getImage()) : nullptr;
}

void* VulkanViewport::getNativeBackBufferRT() const noexcept
{
    return m_rhiBackBuffer ? reinterpret_cast<void*>(m_rhiBackBuffer->getDefaultView()) : nullptr;
}

void* VulkanViewport::getNativeWindow(void** const outAddParam) const noexcept
{
    const gp::hal::NativeWindowHandle& nativeHandle = m_windowContext.getNativeWindowHandle();
    if (nativeHandle.backend == gp::hal::NativeWindowBackend::X11)
    {
        if (outAddParam != nullptr)
        {
            *outAddParam = nativeHandle.x11.display;
        }
        return reinterpret_cast<void*>(nativeHandle.x11.window);
    }
    if (outAddParam != nullptr)
    {
        *outAddParam = nullptr;
    }
    return m_windowContext.getWindowHandle();
}

void VulkanViewport::setCustomPresent(presentation::CustomPresent* const customPresent) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_customPresent = presentation::CustomPresentRef(customPresent);
}

presentation::CustomPresent* VulkanViewport::getCustomPresent() const noexcept
{
    return m_customPresent.get();
}

void VulkanViewport::waitForFrameEventCompletion() noexcept
{
    VulkanSyncPointRef syncPoint{};
    {
        gp::thread::UniqueLock<gp::thread::Mutex> frameLock(m_frameEventMutex);
        syncPoint = m_lastFrameSyncPoint;
        m_lastFrameSyncPoint = nullptr;
    }
    if (!syncPoint || syncPoint->isComplete())
    {
        return;
    }
    if (m_services.submitter != nullptr)
    {
        static_cast<void>(m_services.submitter->waitForSyncPoint(*syncPoint, gp::NumericLimits<gp::UInt64>::max()));
        return;
    }
    syncPoint->wait();
}

void VulkanViewport::issueFrameEvent() noexcept
{
    // `present()` records the frame event (the sync point of the presenting submission).
}

VulkanSwapChain* VulkanViewport::getSwapChain() const noexcept
{
    return m_swapChain.get();
}

resource::TextureCreateDesc VulkanViewport::makeBackBufferDesc(
    const gp::StringView debugName,
    const gp::UInt32 width,
    const gp::UInt32 height,
    const PixelFormat format,
    const bool isSwapChainImage
) noexcept
{
    resource::TextureCreateDesc desc = resource::TextureCreateDesc::create2D(
        debugName, static_cast<gp::Int32>(width), static_cast<gp::Int32>(height), format
    );
    resource::TextureCreateFlags flags = kBackBufferFlags;
    if (isSwapChainImage)
    {
        flags = flags | resource::TextureCreateFlags::Presentable;
    }
    desc.setFlags(flags);
    desc.setInitialState(sync::ResourceAccess::Present);
    return desc;
}

bool VulkanViewport::createSwapChain(VulkanSwapChainRecreateInfo* const recreateInfo) noexcept
{
    VulkanDevice& device = *m_services.resourceServices.device;

    VulkanSwapChainDesc desc{};
    desc.device = &device;
    desc.fenceManager = m_services.fenceManager;
    desc.pixelFormat = m_pixelFormat;
    desc.width = m_sizeX;
    desc.height = m_sizeY;
    desc.isFullscreen = m_isFullscreen;
    desc.lockToVsync = m_lockToVsync;
    desc.config = m_config;
    desc.config.lockToVsync = m_lockToVsync;

    gp::UniquePtr<VulkanSwapChain> swapChain = gp::makeUnique<VulkanSwapChain>(desc);
    if (!swapChain->initialize(m_windowContext, recreateInfo))
    {
        return false;
    }

    const VkSurfaceKHR surface = swapChain->getSurface();
    m_presentQueue = selectVulkanPresentQueue(
        m_services.graphicsQueue,
        queueSupportsPresent(device, m_services.graphicsQueue, surface),
        m_services.computeQueue,
        queueSupportsPresent(device, m_services.computeQueue, surface),
        m_config.allowPresentOnComputeQueue
    );
    if (m_presentQueue == nullptr)
    {
        // Surface support could not be queried; graphics queues present on every desktop and mobile driver.
        m_presentQueue = m_services.graphicsQueue;
    }

    const VkExtent2D& extent = swapChain->getExtent();
    const gp::Vector<VkImage>& images = swapChain->getImages();
    m_images.clear();
    m_images.reserve(images.size());
    for (const VkImage image: images)
    {
        SwapChainImage slot{};
        slot.texture = gp::RefCountedPtr<VulkanTexture>(new VulkanTexture(
            m_services.resourceServices,
            makeBackBufferDesc("SwapChainImage", extent.width, extent.height, swapChain->getPixelFormat(), true),
            image
        ));
        slot.renderingDoneSemaphore = gp::makeShared<VulkanSemaphore>(device, VulkanSemaphoreFlags::ExternallyWaited);
        m_images.pushBack(std::move(slot));
    }

    m_swapChain = std::move(swapChain);
    m_acquiredImageIndex = -1;
    return true;
}

void VulkanViewport::destroySwapChain(VulkanSwapChainRecreateInfo* const outRecreateInfo) noexcept
{
    if (!m_swapChain)
    {
        m_images.clear();
        return;
    }

    VulkanDevice& device = *m_services.resourceServices.device;
    // Acquired images and their views may still be in flight.
    device.waitUntilIdle();

    // Dropping the last reference only marks a texture for deferred deletion, so its views would outlive the
    // swapchain. Release them explicitly: the views are queued (and the cached framebuffers dropped) right now.
    for (const SwapChainImage& slot: m_images)
    {
        if (!slot.texture)
        {
            continue;
        }
        if ((slot.texture->getServices().renderPassManager == nullptr) && (m_services.renderPassManager != nullptr))
        {
            m_services.renderPassManager->notifyDeletedImage(slot.texture->getImage(), slot.texture->getDesc().flags);
        }
        slot.texture->releaseExternalImage();
    }
    m_images.clear();

    // The image views must be gone before the swapchain destroys the images they reference.
    if (m_services.resourceServices.deferredDeletionQueue != nullptr)
    {
        m_services.resourceServices.deferredDeletionQueue->releaseResources(true);
    }

    m_swapChain->destroy(outRecreateInfo);
    m_swapChain.reset();
    m_presentQueue = nullptr;
    m_acquiredImageIndex = -1;
}

bool VulkanViewport::recreateSwapChain(const bool recreateSurface) noexcept
{
    releaseAcquiredImage();

    VulkanSwapChainRecreateInfo recreateInfo{};
    // A lost surface cannot be reused; anything else hands the old swapchain over for a smoother transition.
    destroySwapChain(recreateSurface ? nullptr : &recreateInfo);
    const bool created = createSwapChain(recreateSurface ? nullptr : &recreateInfo);
    ++m_recreateCount;

    // Destroy any retired swapchain or un-adopted surface not already consumed during `createSwapChain`.
    VulkanDevice& device = *m_services.resourceServices.device;
    if (recreateInfo.swapChain != VK_NULL_HANDLE)
    {
        VulkanPlatform::destroySwapchainKHR(device.getHandle(), recreateInfo.swapChain, GP_VULKAN_CPU_ALLOCATOR);
        recreateInfo.swapChain = VK_NULL_HANDLE;
    }
    if (recreateInfo.surface != VK_NULL_HANDLE)
    {
        VulkanPlatform::destroySurface(m_windowContext.getWindowId(), device.getInstanceHandle(), recreateInfo.surface);
        recreateInfo.surface = VK_NULL_HANDLE;
    }

    if (!created)
    {
        return false;
    }

    const VkExtent2D& extent = m_swapChain->getExtent();
    const gp::UInt32 backBufferWidth =
        static_cast<gp::UInt32>(m_rhiBackBuffer ? m_rhiBackBuffer->getDesc().extent.x : 0);
    const gp::UInt32 backBufferHeight =
        static_cast<gp::UInt32>(m_rhiBackBuffer ? m_rhiBackBuffer->getDesc().extent.y : 0);
    if (m_rhiBackBuffer && ((extent.width != backBufferWidth) || (extent.height != backBufferHeight)))
    {
        GP_LOG_WARNING(
            LogVulkanRHI,
            "VulkanViewport: recreated swapchain extent {}x{} differs from the back buffer; resize the viewport.",
            extent.width,
            extent.height
        );
    }
    return true;
}

void VulkanViewport::createBackBuffers(
    command::CommandListBase& cmdList,
    const gp::UInt32 width,
    const gp::UInt32 height,
    const PixelFormat format,
    VulkanBackBufferRef& outBackBuffer,
    gp::RefCountedPtr<VulkanTexture>& outRenderingBackBuffer
) noexcept
{
    // A minimized window may report a zero extent; textures need at least one texel.
    const gp::UInt32 safeWidth = (width != 0u) ? width : 1u;
    const gp::UInt32 safeHeight = (height != 0u) ? height : 1u;

    outBackBuffer = VulkanBackBufferRef(new VulkanBackBuffer(
        m_services.resourceServices, *this, makeBackBufferDesc("BackBuffer", safeWidth, safeHeight, format, true)
    ));
    outRenderingBackBuffer = gp::RefCountedPtr<VulkanTexture>(new VulkanTexture(
        m_services.resourceServices,
        cmdList,
        makeBackBufferDesc("RenderingBackBuffer", safeWidth, safeHeight, format, false)
    ));
}

void VulkanViewport::installBackBuffers(
    const VulkanBackBufferRef& backBuffer,
    const gp::RefCountedPtr<VulkanTexture>& renderingBackBuffer,
    const bool recreate
) noexcept
{
    releaseAcquiredImage();
    if (m_rhiBackBuffer && (m_rhiBackBuffer != backBuffer))
    {
        m_rhiBackBuffer->detachViewport();
    }
    m_rhiBackBuffer = backBuffer;
    m_rhiRenderingBackBuffer = renderingBackBuffer;

    if (recreate)
    {
        static_cast<void>(recreateSwapChain(false));
        if (m_customPresent)
        {
            m_customPresent->onBackBufferResize();
        }
    }
}

VulkanSwapChainStatus
    VulkanViewport::acquireWithRecovery(gp::UInt32& outImageIndex, VulkanSemaphoreRef& outSemaphore) noexcept
{
    VulkanSwapChainStatus status =
        m_swapChain ? m_swapChain->acquireImageIndex(outImageIndex, outSemaphore) : VulkanSwapChainStatus::OutOfDate;

    gp::UInt32 attempts = 0u;
    while (isRecoverableStatus(status) && VulkanPlatform::recreateSwapchainOnFail() &&
           (attempts < kMaxRecreateAttempts))
    {
        ++attempts;
        if (!recreateSwapChain(status == VulkanSwapChainStatus::SurfaceLost))
        {
            // Typically a zero sized (minimized) window: try again next frame.
            return VulkanSwapChainStatus::NotReady;
        }
        status = m_swapChain->acquireImageIndex(outImageIndex, outSemaphore);
    }

    if (status == VulkanSwapChainStatus::Failed)
    {
        GP_LOG_ERROR(LogVulkanRHI, "{}", "VulkanViewport: vkAcquireNextImageKHR failed.");
    }
    return status;
}

void VulkanViewport::initializeImageOnFirstUse(VulkanCommandListContext& context, const VkImage image) noexcept
{
    VulkanCommandBuffer* const cmdBuffer = context.getCommandBuffer();
    if ((cmdBuffer == nullptr) || (image == VK_NULL_HANDLE))
    {
        return;
    }

    const VkImageSubresourceRange range = VulkanPipelineBarrier::makeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vulkanSetImageLayout(*cmdBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

    // Undefined content is garbage on screen: clear it once so a frame that skips the image shows black.
    VkClearColorValue clearColor{};
    vkCmdClearColorImage(cmdBuffer->getHandle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u, &range);
    vulkanSetImageLayout(
        *cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, range
    );
}

void VulkanViewport::releaseAcquiredImage() noexcept
{
    if (m_acquiredBackBuffer)
    {
        m_acquiredBackBuffer->releaseAcquiredImage();
        m_acquiredBackBuffer = VulkanBackBufferRef{};
    }
    m_acquiredImageIndex = -1;
}

}   // namespace gp::rhi::vulkan
