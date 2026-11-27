// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "contexts/VulkanContextForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"    // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "platform/VulkanPlatform.hpp"
#include "presentation/PresentationForward.hpp"   // IWYU pragma: keep
#include "presentation/Viewport.hpp"
#include "presentation/VulkanPresentationDesc.hpp"
#include "presentation/VulkanPresentationEnums.hpp"
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "presentation/VulkanPresentationTypes.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIForward.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Window presentation target: a swapchain, the back buffer the renderer draws into and the present logic.
/// @details Two timelines touch a viewport:
///          - The **recording (render) thread** creates it (`initialize()`), resizes it (`resize()`) and reads the
///            back buffer (`getBackBuffer()`). Those calls create textures through the command list they receive.
///          - The **RHI timeline** acquires images (`acquireBackBufferImage()`, triggered by the back buffer the first
///            time it is used in a frame) and presents them (`present()`). Every swapchain recreation (out of date,
///            lost surface, vertical sync change, resize) happens there, under `m_mutex`.
///
///          The renderer always sees the same `VulkanBackBuffer`; each frame it borrows the swapchain image just
///          acquired. Every swapchain image is wrapped by a `VulkanTexture` that owns its views (not its image) and
///          pairs with a "rendering done" semaphore the graphics submission signals and the present waits on.
///
///          When no image can be acquired (minimized window, zero sized surface, repeated failures), the back buffer
///          borrows an off-screen "rendering back buffer" instead, so the frame renders somewhere valid and nothing
///          is presented.
class GP_RHI_VULKAN_API VulkanViewport final : public presentation::Viewport
{
public:
    /// @brief Maximum swapchain recreations attempted for one acquire or present before giving up for the frame.
    static constexpr gp::UInt32 kMaxRecreateAttempts = 4u;

public:
    /// @brief Stores the creation parameters; nothing native is created until `initialize()`.
    /// @param[in] services Device level services; `services.isValid()` must hold.
    /// @param[in] windowContext Window the swapchain presents to.
    /// @param[in] sizeX Requested width in pixels.
    /// @param[in] sizeY Requested height in pixels.
    /// @param[in] isFullscreen True to request exclusive fullscreen when the platform supports it.
    /// @param[in] preferredPixelFormat Requested back buffer format; `Unknown` picks the default format.
    VulkanViewport(
        const VulkanViewportServices& services,
        const VulkanPlatformWindowContext& windowContext,
        gp::UInt32 sizeX,
        gp::UInt32 sizeY,
        bool isFullscreen,
        PixelFormat preferredPixelFormat
    ) noexcept;

    /// @brief Creates the swapchain, wraps its images and creates the back buffers.
    /// @note Recording thread, before the viewport is shared: the swapchain is created on the calling thread.
    /// @param[in,out] cmdList Command list the back buffer textures are created with.
    /// @return False when the swapchain could not be created; the viewport is unusable then.
    [[nodiscard]] bool initialize(command::CommandListBase& cmdList) noexcept;

    /// @brief Changes the size, fullscreen state or format of the viewport.
    /// @details Creates the new back buffers right away (the renderer can use them this frame) and enqueues the
    ///          swapchain recreation on the RHI timeline, after every command already recorded for the old size.
    /// @note Recording thread.
    /// @param[in,out] cmdList Command list the textures are created with and the recreation enqueued on.
    /// @param[in] sizeX New width in pixels.
    /// @param[in] sizeY New height in pixels.
    /// @param[in] isFullscreen New fullscreen state.
    /// @param[in] preferredPixelFormat New format, or `Unknown` to keep the current one.
    void resize(
        command::CommandListBase& cmdList,
        gp::UInt32 sizeX,
        gp::UInt32 sizeY,
        bool isFullscreen,
        PixelFormat preferredPixelFormat
    ) noexcept;

    /// @brief Returns the back buffer the renderer draws into.
    /// @note Recording thread.
    /// @return Back buffer, or `nullptr` before `initialize()`.
    [[nodiscard]] VulkanBackBuffer* getBackBuffer() const noexcept;

    /// @brief Advances the back buffer to the next frame.
    /// @details The back buffer persists across frames and releases its image when presented, so there is nothing
    ///          to rotate; the call exists for the frame protocol and keeps an unpresented image for the next frame.
    /// @note Recording thread.
    void advanceBackBufferFrame() noexcept;

    /// @brief Acquires the next swapchain image and binds it to `backBuffer`.
    /// @details Recreates the swapchain when it is out of date or its surface lost (up to `kMaxRecreateAttempts`
    ///          times), makes the context's next submission wait on the acquire semaphore and clears an image used for
    ///          the first time. When nothing can be acquired, binds the rendering back buffer instead.
    /// @note RHI timeline.
    /// @param[in,out] executingCmdList Command list being executed; its Vulkan context records the wait and clear.
    /// @param[in,out] backBuffer Back buffer receiving the image.
    /// @return True when a swapchain image was bound; false when the fallback was bound.
    bool acquireBackBufferImage(command::CommandListBase& executingCmdList, VulkanBackBuffer& backBuffer) noexcept;

    /// @brief Ends the frame: runs the custom present hook, submits the frame and presents the acquired image.
    /// @details The graphics submission signals the image's "rendering done" semaphore, then the image is presented
    ///          on the present queue, waiting on it. An out of date swapchain or lost surface is recreated (it is
    ///          presented again next frame). A change of vertical sync recreates the swapchain with the matching
    ///          present mode.
    /// @note RHI timeline.
    /// @param[in,out] executingCmdList Command list being executed; its Vulkan context is submitted.
    /// @param[in] presentArgs Whether to present and whether to wait for vertical blank.
    /// @return True when an image was presented.
    bool present(
        command::CommandListBase& executingCmdList, const presentation::PresentArgsWithStats& presentArgs
    ) noexcept;

    /// @copydoc presentation::Viewport::getNativeSwapChain()
    /// @note Returns the `VkSwapchainKHR` (as a pointer-sized value), or `nullptr` when there is none.
    [[nodiscard]] virtual void* getNativeSwapChain() const noexcept override;

    /// @copydoc presentation::Viewport::getNativeBackBufferTexture()
    /// @note Returns the `VkImage` bound to the back buffer this frame.
    [[nodiscard]] virtual void* getNativeBackBufferTexture() const noexcept override;

    /// @copydoc presentation::Viewport::getNativeBackBufferRT()
    /// @note Returns the default `VkImageView` bound to the back buffer this frame.
    [[nodiscard]] virtual void* getNativeBackBufferRT() const noexcept override;

    /// @copydoc presentation::Viewport::getNativeWindow()
    /// @note On X11 the return value is the `Window` id and `outAddParam` receives the `Display*`.
    [[nodiscard]] virtual void* getNativeWindow(void** outAddParam = nullptr) const noexcept override;

    /// @copydoc presentation::Viewport::setCustomPresent()
    /// @note The viewport keeps a reference on the hook.
    virtual void setCustomPresent(presentation::CustomPresent* customPresent) noexcept override;

    /// @copydoc presentation::Viewport::getCustomPresent()
    [[nodiscard]] virtual presentation::CustomPresent* getCustomPresent() const noexcept override;

    /// @copydoc presentation::Viewport::waitForFrameEventCompletion()
    /// @note Waits on the sync point of the last presented submission, then forgets it.
    virtual void waitForFrameEventCompletion() noexcept override;

    /// @copydoc presentation::Viewport::issueFrameEvent()
    /// @note The frame event is the sync point captured by `present()`; there is nothing extra to issue.
    virtual void issueFrameEvent() noexcept override;

    /// @brief Returns the swapchain, owned by the viewport.
    /// @note RHI timeline; the swapchain changes on every recreation.
    /// @return Swapchain, or `nullptr` while none exists (zero sized window, failed creation).
    [[nodiscard]] VulkanSwapChain* getSwapChain() const noexcept;

    /// @brief Returns the requested width.
    /// @return Width in pixels.
    [[nodiscard]] gp::UInt32 getSizeX() const noexcept
    {
        return m_sizeX;
    }

    /// @brief Returns the requested height.
    /// @return Height in pixels.
    [[nodiscard]] gp::UInt32 getSizeY() const noexcept
    {
        return m_sizeY;
    }

    /// @brief Reports whether exclusive fullscreen is requested.
    /// @return True when fullscreen.
    [[nodiscard]] bool isFullscreen() const noexcept
    {
        return m_isFullscreen;
    }

    /// @brief Returns the format the back buffer was created with.
    /// @return Back buffer pixel format.
    [[nodiscard]] PixelFormat getPixelFormat() const noexcept
    {
        return m_pixelFormat;
    }

    /// @brief Returns the index of the swapchain image bound to the back buffer.
    /// @return Image index, or `-1` when none is acquired.
    [[nodiscard]] gp::Int32 getAcquiredImageIndex() const noexcept
    {
        return m_acquiredImageIndex;
    }

    /// @brief Returns the number of `present()` calls that reached the swapchain.
    /// @return Present count.
    [[nodiscard]] gp::UInt32 getPresentCount() const noexcept
    {
        return m_presentCount;
    }

    /// @brief Returns the number of swapchain recreations since creation.
    /// @return Recreation count.
    [[nodiscard]] gp::UInt32 getRecreateCount() const noexcept
    {
        return m_recreateCount;
    }

protected:
    /// @brief Releases the back buffer binding, the swapchain and its surface.
    virtual ~VulkanViewport() noexcept override;

private:
    /// @brief Per swapchain image state.
    struct SwapChainImage
    {
        /// @brief Texture wrapping the image; owns the views, not the image.
        gp::RefCountedPtr<VulkanTexture> texture{};

        /// @brief Signaled by the submission that wrote the image; waited on by the present.
        VulkanSemaphoreRef renderingDoneSemaphore{};

        /// @brief True until the image is acquired once (its content and layout are undefined).
        bool isFirstUse{ true };
    };

    /// @brief Builds the description of the back buffer and swapchain image textures.
    /// @param[in] debugName Texture name.
    /// @param[in] width Width in pixels.
    /// @param[in] height Height in pixels.
    /// @param[in] format Pixel format.
    /// @param[in] isSwapChainImage True to mark the texture presentable.
    /// @return Texture description.
    [[nodiscard]] static resource::TextureCreateDesc makeBackBufferDesc(
        gp::StringView debugName, gp::UInt32 width, gp::UInt32 height, PixelFormat format, bool isSwapChainImage
    ) noexcept;

    /// @brief Creates the swapchain, wraps its images and picks the present queue.
    /// @param[in,out] recreateInfo Retired swapchain and surface to reuse, or `nullptr`.
    /// @return True on success.
    [[nodiscard]] bool createSwapChain(VulkanSwapChainRecreateInfo* recreateInfo) noexcept;

    /// @brief Waits for the device, drops the image textures and destroys (or retires) the swapchain.
    /// @param[out] outRecreateInfo Receives the retired swapchain and surface, or `nullptr` to destroy them.
    void destroySwapChain(VulkanSwapChainRecreateInfo* outRecreateInfo) noexcept;

    /// @brief Destroys and recreates the swapchain with the current parameters.
    /// @param[in] recreateSurface True when the surface was lost and must be created again.
    /// @return True when the new swapchain was created.
    bool recreateSwapChain(bool recreateSurface) noexcept;

    /// @brief Creates the back buffer and the rendering back buffer.
    /// @param[in,out] cmdList Command list the rendering back buffer is created with.
    /// @param[in] width Width in pixels; zero is raised to one.
    /// @param[in] height Height in pixels; zero is raised to one.
    /// @param[in] format Pixel format.
    /// @param[out] outBackBuffer Receives the back buffer.
    /// @param[out] outRenderingBackBuffer Receives the off-screen fallback.
    void createBackBuffers(
        command::CommandListBase& cmdList,
        gp::UInt32 width,
        gp::UInt32 height,
        PixelFormat format,
        VulkanBackBufferRef& outBackBuffer,
        gp::RefCountedPtr<VulkanTexture>& outRenderingBackBuffer
    ) noexcept;

    /// @brief Installs back buffers on the RHI timeline, recreating the swapchain when its parameters changed.
    /// @param[in] backBuffer New back buffer.
    /// @param[in] renderingBackBuffer New off-screen fallback.
    /// @param[in] recreate True to recreate the swapchain.
    void installBackBuffers(
        const VulkanBackBufferRef& backBuffer,
        const gp::RefCountedPtr<VulkanTexture>& renderingBackBuffer,
        bool recreate
    ) noexcept;

    /// @brief Acquires an image, recreating the swapchain on recoverable failures.
    /// @param[out] outImageIndex Receives the image index.
    /// @param[out] outSemaphore Receives the acquire semaphore.
    /// @return Final status.
    [[nodiscard]] VulkanSwapChainStatus
        acquireWithRecovery(gp::UInt32& outImageIndex, VulkanSemaphoreRef& outSemaphore) noexcept;

    /// @brief Moves a never used image from `UNDEFINED` to `PRESENT_SRC_KHR`, cleared to black.
    /// @param[in,out] context Context recording the transition.
    /// @param[in] image Swapchain image.
    void initializeImageOnFirstUse(VulkanCommandListContext& context, VkImage image) noexcept;

    /// @brief Releases the image bound to the back buffer that acquired it.
    void releaseAcquiredImage() noexcept;

    VulkanViewportServices m_services{};
    VulkanPlatformWindowContext m_windowContext;
    VulkanPresentationConfig m_config{};

    /// @brief Guards every swapchain and RHI side member.
    gp::thread::Mutex m_mutex{};

    /// @brief Guards `m_lastFrameSyncPoint`.
    gp::thread::Mutex m_frameEventMutex{};

    gp::UniquePtr<VulkanSwapChain> m_swapChain{};
    gp::Vector<SwapChainImage> m_images{};
    VulkanQueue* m_presentQueue{ nullptr };

    /// @brief Recording side back buffer, returned by `getBackBuffer()`.
    VulkanBackBufferRef m_backBuffer{};

    /// @brief Recording side off-screen fallback, kept alive with the back buffer.
    gp::RefCountedPtr<VulkanTexture> m_renderingBackBuffer{};

    /// @brief RHI side back buffer, installed by `installBackBuffers()`.
    VulkanBackBufferRef m_rhiBackBuffer{};

    /// @brief RHI side off-screen fallback.
    gp::RefCountedPtr<VulkanTexture> m_rhiRenderingBackBuffer{};

    /// @brief Back buffer the current image is bound to, released by `present()`.
    VulkanBackBufferRef m_acquiredBackBuffer{};

    presentation::CustomPresentRef m_customPresent{};
    VulkanSyncPointRef m_lastFrameSyncPoint{};

    gp::UInt32 m_sizeX{ 0u };
    gp::UInt32 m_sizeY{ 0u };
    PixelFormat m_pixelFormat{ PixelFormat::Unknown };
    gp::Int32 m_acquiredImageIndex{ -1 };
    gp::UInt32 m_presentCount{ 0u };
    gp::UInt32 m_recreateCount{ 0u };
    bool m_isFullscreen{ false };
    bool m_lockToVsync{ false };
};

}   // namespace gp::rhi::vulkan
