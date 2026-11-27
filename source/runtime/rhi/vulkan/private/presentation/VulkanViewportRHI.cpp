// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanViewportRHI.hpp"
#include "commands/CommandListBase.hpp"
#include "CoreMinimal.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "platform/VulkanPlatform.hpp"
#include "presentation/PresentArgs.hpp"
#include "presentation/Viewport.hpp"
#include "presentation/VulkanBackBuffer.hpp"
#include "presentation/VulkanPresentationForward.hpp"
#include "presentation/VulkanViewport.hpp"
#include "resources/Texture.hpp"   // IWYU pragma: keep
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Recovers the Vulkan viewport behind an RHI viewport.
/// @details Every viewport the Vulkan RHI hands out is a `VulkanViewport`, so the downcast is static.
/// @param[in] viewport RHI viewport, or `nullptr`.
/// @return Vulkan viewport, or `nullptr`.
[[nodiscard]] VulkanViewport* toVulkanViewport(presentation::Viewport* const viewport) noexcept
{
    return static_cast<VulkanViewport*>(viewport);
}

}   // namespace

presentation::ViewportRef createVulkanViewport(
    command::CommandListBase& cmdList,
    const VulkanViewportServices& services,
    const gp::hal::NativeWindowHandle* const windowHandle,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    if ((windowHandle == nullptr) || (windowHandle->backend == gp::hal::NativeWindowBackend::Unknown))
    {
        GP_LOG_ERROR(LogVulkanRHI, "{}", "createVulkanViewport: invalid native window handle.");
        return presentation::ViewportRef{};
    }
    if (!services.isValid())
    {
        GP_LOG_ERROR(LogVulkanRHI, "{}", "createVulkanViewport: invalid services.");
        return presentation::ViewportRef{};
    }

    const VulkanPlatformWindowContext windowContext(*windowHandle);
    const VulkanViewportRef viewport(
        new VulkanViewport(services, windowContext, sizeX, sizeY, isFullscreen, preferredPixelFormat)
    );
    if (!viewport->initialize(cmdList))
    {
        return presentation::ViewportRef{};
    }
    return presentation::ViewportRef(viewport.get());
}

void resizeVulkanViewport(
    command::CommandListBase& cmdList,
    presentation::Viewport* const viewport,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    VulkanViewport* const vulkanViewport = toVulkanViewport(viewport);
    if (vulkanViewport != nullptr)
    {
        vulkanViewport->resize(cmdList, sizeX, sizeY, isFullscreen, preferredPixelFormat);
    }
}

resource::TextureRef
    getVulkanViewportBackBuffer(command::CommandListBase& cmdList, presentation::Viewport* const viewport) noexcept
{
    VulkanViewport* const vulkanViewport = toVulkanViewport(viewport);
    if ((vulkanViewport == nullptr) || (vulkanViewport->getBackBuffer() == nullptr))
    {
        return resource::TextureRef{};
    }

    const VulkanBackBufferRef backBuffer(vulkanViewport->getBackBuffer());
    cmdList.enqueueLambda(
        [backBuffer](command::CommandListBase& executingCmdList) -> void
    {
        backBuffer->onGetBackBufferImage(executingCmdList);
    }
    );
    return resource::TextureRef(backBuffer.get());
}

void advanceVulkanViewportFrame(presentation::Viewport* const viewport) noexcept
{
    VulkanViewport* const vulkanViewport = toVulkanViewport(viewport);
    if (vulkanViewport != nullptr)
    {
        vulkanViewport->advanceBackBufferFrame();
    }
}

void beginDrawingVulkanViewport(
    command::CommandListBase& cmdList, presentation::Viewport* const viewport, resource::Texture* const renderTarget
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(viewport);
    static_cast<void>(renderTarget);
}

void endDrawingVulkanViewport(
    command::CommandListBase& cmdList,
    presentation::Viewport* const viewport,
    const presentation::PresentArgsWithStats& presentArgs
) noexcept
{
    VulkanViewport* const vulkanViewport = toVulkanViewport(viewport);
    if (vulkanViewport == nullptr)
    {
        return;
    }

    const VulkanViewportRef viewportRef(vulkanViewport);
    cmdList.enqueueLambda(
        [viewportRef, presentArgs](command::CommandListBase& executingCmdList) -> void
    {
        static_cast<void>(viewportRef->present(executingCmdList, presentArgs));
    }
    );
}

bool getVulkanSupportedResolutions(gp::Vector<ScreenResolution>& outResolutions, const bool ignoreRefreshRate) noexcept
{
    static_cast<void>(ignoreRefreshRate);
    outResolutions.clear();
    return false;
}

void getVulkanSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept
{
    static_cast<void>(inOutWidth);
    static_cast<void>(inOutHeight);
}

}   // namespace gp::rhi::vulkan
