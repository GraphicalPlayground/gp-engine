// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandListExecutor.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDynamicRHI.hpp"
#include "presentation/VulkanViewportRHI.hpp"
#include "window/NativeWindow.hpp"

namespace gp::rhi::vulkan
{

presentation::ViewportRef VulkanDynamicRHI::createViewport(
    void* const windowHandle,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    if (!m_isInitialized || (windowHandle == nullptr))
    {
        return presentation::ViewportRef{};
    }

    // The RHI interface is window-system agnostic; the HAL hands every backend a `NativeWindowHandle`.
    const gp::hal::NativeWindowHandle* const nativeWindow =
        static_cast<const gp::hal::NativeWindowHandle*>(windowHandle);
    return createVulkanViewport(
        command::getImmediateCommandList(),
        m_viewportServices,
        nativeWindow,
        sizeX,
        sizeY,
        isFullscreen,
        preferredPixelFormat
    );
}

void VulkanDynamicRHI::resizeViewport(
    presentation::Viewport* const viewport,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    resizeVulkanViewport(
        command::getImmediateCommandList(), viewport, sizeX, sizeY, isFullscreen, preferredPixelFormat
    );
}

resource::TextureRef VulkanDynamicRHI::getViewportBackBuffer(presentation::Viewport* const viewport) noexcept
{
    return getVulkanViewportBackBuffer(command::getImmediateCommandList(), viewport);
}

void VulkanDynamicRHI::advanceFrameForGetViewportBackBuffer(presentation::Viewport* const viewport) noexcept
{
    advanceVulkanViewportFrame(viewport);
}

void VulkanDynamicRHI::beginDrawingViewport(
    presentation::Viewport* const viewport, resource::Texture* const renderTarget
) noexcept
{
    beginDrawingVulkanViewport(command::getImmediateCommandList(), viewport, renderTarget);
}

void VulkanDynamicRHI::endDrawingViewport(
    presentation::Viewport* const viewport, const presentation::PresentArgsWithStats& presentArgs
) noexcept
{
    endDrawingVulkanViewport(command::getImmediateCommandList(), viewport, presentArgs);
}

bool VulkanDynamicRHI::getSupportedResolutions(
    gp::Vector<ScreenResolution>& outResolutions, const bool ignoreRefreshRate
) noexcept
{
    return getVulkanSupportedResolutions(outResolutions, ignoreRefreshRate);
}

void VulkanDynamicRHI::getSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept
{
    getVulkanSupportedResolution(inOutWidth, inOutHeight);
}

}   // namespace gp::rhi::vulkan
