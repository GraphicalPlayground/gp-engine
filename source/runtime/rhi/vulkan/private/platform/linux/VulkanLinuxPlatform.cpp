// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"   // IWYU pragma: keep

#if GP_PLATFORM_LINUX

    #include "diagnostics/AssertionMacros.hpp"
    #include "loader/VulkanCommandWrappers.hpp"
    #include "platform/linux/VulkanLinuxPlatform.hpp"
    #include "VulkanConfiguration.hpp"
    #include "VulkanThirdParty.hpp"   // IWYU pragma: keep
    #include "window/NativeWindow.hpp"

namespace gp::rhi::vulkan
{

VkResult VulkanLinuxPlatform::createSurface(
    const VulkanGenericPlatformWindowContext& windowContext, const VkInstance instance, VkSurfaceKHR* const outSurface
) noexcept
{
    GP_CHECKF(outSurface != nullptr, "createSurface requires a destination surface handle.");
    *outSurface = VK_NULL_HANDLE;
    static_cast<void>(instance);

    // Linux exposes two window systems; the HAL descriptor tells which one owns the window, so the matching WSI
    // extension is selected at runtime instead of compile time.
    const gp::hal::NativeWindowHandle& nativeHandle = windowContext.getNativeWindowHandle();
    switch (nativeHandle.backend)
    {
    case gp::hal::NativeWindowBackend::X11:
    {
    #if defined(VK_USE_PLATFORM_XLIB_KHR)
        if (vkCreateXlibSurfaceKHR == nullptr)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        VkXlibSurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        createInfo.dpy = static_cast<Display*>(nativeHandle.x11.display);
        createInfo.window = static_cast<Window>(nativeHandle.x11.window);
        return vkCreateXlibSurfaceKHR(instance, &createInfo, GP_VULKAN_CPU_ALLOCATOR, outSurface);
    #else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    #endif
    }
    case gp::hal::NativeWindowBackend::Wayland:
    {
    #if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        if (vkCreateWaylandSurfaceKHR == nullptr)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        VkWaylandSurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        createInfo.display = static_cast<wl_display*>(nativeHandle.wayland.display);
        createInfo.surface = static_cast<wl_surface*>(nativeHandle.wayland.surface);
        return vkCreateWaylandSurfaceKHR(instance, &createInfo, GP_VULKAN_CPU_ALLOCATOR, outSurface);
    #else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    #endif
    }
    default:
        break;
    }

    return VK_ERROR_INITIALIZATION_FAILED;
}

}   // namespace gp::rhi::vulkan

#endif
