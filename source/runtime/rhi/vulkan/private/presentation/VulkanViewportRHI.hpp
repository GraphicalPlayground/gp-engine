// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "device/DeviceTypes.hpp"
#include "maths/color/PixelFormatEnums.hpp"   // IWYU pragma: keep
#include "presentation/PresentationForward.hpp"
#include "presentation/VulkanPresentationDesc.hpp"
#include "RHIForward.hpp"
#include "RHIVulkan.hpp"
#include "window/NativeWindow.hpp"

/// @file VulkanViewportRHI.hpp
/// @brief Viewport entry points of the Vulkan dynamic RHI.
/// @details Free functions until the dynamic RHI object exists; its `createViewport()`, `resizeViewport()`,
///          `getViewportBackBuffer()`, `advanceFrameForGetViewportBackBuffer()`, `beginDrawingViewport()`,
///          `endDrawingViewport()` and `getSupportedResolution*()` overrides forward here with the immediate command
///          list. Each function records on the recording thread and defers native work to the RHI timeline through
///          `cmdList`.

namespace gp::rhi::vulkan
{

/// @brief Creates a viewport presenting to a native window.
/// @details The window is passed as a `hal::NativeWindowHandle` (plain handles, no window object lifetime) because
///          some window systems need two handles to build a surface (X11 display and window, Wayland display and
///          surface).
/// @param[in,out] cmdList Recording command list the back buffers are created with.
/// @param[in] services Device level services.
/// @param[in] windowHandle Native window, or `nullptr`.
/// @param[in] sizeX Width in pixels.
/// @param[in] sizeY Height in pixels.
/// @param[in] isFullscreen True to request exclusive fullscreen.
/// @param[in] preferredPixelFormat Back buffer format, or `Unknown` for the default.
/// @return The viewport, or null when the handle or services are invalid or the swapchain could not be created.
[[nodiscard]] GP_RHI_VULKAN_API presentation::ViewportRef createVulkanViewport(
    command::CommandListBase& cmdList,
    const VulkanViewportServices& services,
    const gp::hal::NativeWindowHandle* windowHandle,
    gp::UInt32 sizeX,
    gp::UInt32 sizeY,
    bool isFullscreen,
    PixelFormat preferredPixelFormat
) noexcept;

/// @brief Resizes a viewport; the swapchain is recreated on the RHI timeline.
/// @param[in,out] cmdList Recording command list.
/// @param[in,out] viewport Viewport created by `createVulkanViewport()`, or `nullptr`.
/// @param[in] sizeX New width in pixels.
/// @param[in] sizeY New height in pixels.
/// @param[in] isFullscreen New fullscreen state.
/// @param[in] preferredPixelFormat New format, or `Unknown` to keep the current one.
GP_RHI_VULKAN_API void resizeVulkanViewport(
    command::CommandListBase& cmdList,
    presentation::Viewport* viewport,
    gp::UInt32 sizeX,
    gp::UInt32 sizeY,
    bool isFullscreen,
    PixelFormat preferredPixelFormat = PixelFormat::Unknown
) noexcept;

/// @brief Returns the back buffer of a viewport and schedules the acquire of its image.
/// @details The acquire runs on the RHI timeline before any command recorded after this call, so every use of the
///          returned texture finds an image bound.
/// @param[in,out] cmdList Recording command list the acquire is enqueued on.
/// @param[in,out] viewport Viewport created by `createVulkanViewport()`, or `nullptr`.
/// @return Back buffer texture, or null.
[[nodiscard]] GP_RHI_VULKAN_API resource::TextureRef
    getVulkanViewportBackBuffer(command::CommandListBase& cmdList, presentation::Viewport* viewport) noexcept;

/// @brief Advances the back buffer of a viewport to the next frame.
/// @param[in,out] viewport Viewport created by `createVulkanViewport()`, or `nullptr`.
GP_RHI_VULKAN_API void advanceVulkanViewportFrame(presentation::Viewport* viewport) noexcept;

/// @brief Starts drawing a viewport.
/// @details Nothing to do up front: the image is acquired lazily the first time the back buffer is used.
/// @param[in,out] cmdList Recording command list.
/// @param[in,out] viewport Viewport about to be drawn, or `nullptr`.
/// @param[in] renderTarget Render target the frame draws into, or `nullptr` for the back buffer.
GP_RHI_VULKAN_API void beginDrawingVulkanViewport(
    command::CommandListBase& cmdList, presentation::Viewport* viewport, resource::Texture* renderTarget
) noexcept;

/// @brief Finishes drawing a viewport and enqueues its present on the RHI timeline.
/// @param[in,out] cmdList Recording command list.
/// @param[in,out] viewport Viewport drawn this frame, or `nullptr`.
/// @param[in] presentArgs Whether to present and whether to wait for vertical blank.
GP_RHI_VULKAN_API void endDrawingVulkanViewport(
    command::CommandListBase& cmdList,
    presentation::Viewport* viewport,
    const presentation::PresentArgsWithStats& presentArgs
) noexcept;

/// @brief Lists the fullscreen resolutions of the display.
/// @details Vulkan has no portable display mode enumeration (`VK_KHR_display` targets direct to display only), so
///          the list stays empty and the caller falls back to windowed sizes.
/// @param[out] outResolutions Cleared.
/// @param[in] ignoreRefreshRate Unused.
/// @return False: no resolution list is available.
[[nodiscard]] GP_RHI_VULKAN_API bool
    getVulkanSupportedResolutions(gp::Vector<ScreenResolution>& outResolutions, bool ignoreRefreshRate) noexcept;

/// @brief Snaps a requested fullscreen resolution to a supported one.
/// @details Without a display mode list, any size is accepted unchanged.
/// @param[in,out] inOutWidth Requested width; unchanged.
/// @param[in,out] inOutHeight Requested height; unchanged.
GP_RHI_VULKAN_API void getVulkanSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept;

}   // namespace gp::rhi::vulkan
