// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"   // IWYU pragma: keep

#if GP_PLATFORM_WINDOWS

    #include "diagnostics/AssertionMacros.hpp"
    #include "loader/VulkanCommandWrappers.hpp"
    #include "platform/windows/VulkanWindowsPlatform.hpp"
    #include "VulkanConfiguration.hpp"
    #include "VulkanThirdParty.hpp"   // IWYU pragma: keep
    #include "window/NativeWindow.hpp"

namespace gp::rhi::vulkan
{

VkResult VulkanWindowsPlatform::createSurface(
    const VulkanGenericPlatformWindowContext& windowContext, const VkInstance instance, VkSurfaceKHR* const outSurface
) noexcept
{
    GP_CHECKF(outSurface != nullptr, "createSurface requires a destination surface handle.");
    *outSurface = VK_NULL_HANDLE;

    if (vkCreateWin32SurfaceKHR == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void* const windowHandle = windowContext.getWindowHandle();
    if (windowHandle == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // A HAL descriptor carries the owning module instance; a raw `HWND` falls back to the executable module,
    // which is the instance every window class of the process is registered against by default.
    const gp::hal::NativeWindowHandle& nativeHandle = windowContext.getNativeWindowHandle();
    HINSTANCE moduleInstance = nullptr;
    if (nativeHandle.backend == gp::hal::NativeWindowBackend::Win32 && nativeHandle.win32.hinstance != nullptr)
    {
        moduleInstance = static_cast<HINSTANCE>(nativeHandle.win32.hinstance);
    }
    else
    {
        moduleInstance = ::GetModuleHandleW(nullptr);
    }

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = moduleInstance;
    createInfo.hwnd = static_cast<HWND>(windowHandle);
    return vkCreateWin32SurfaceKHR(instance, &createInfo, GP_VULKAN_CPU_ALLOCATOR, outSurface);
}

}   // namespace gp::rhi::vulkan

#endif
