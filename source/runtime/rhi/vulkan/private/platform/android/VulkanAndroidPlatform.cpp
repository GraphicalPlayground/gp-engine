// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"   // IWYU pragma: keep

#if defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID

    #include "diagnostics/AssertionMacros.hpp"
    #include "loader/VulkanCommandWrappers.hpp"
    #include "platform/android/VulkanAndroidPlatform.hpp"
    #include "VulkanConfiguration.hpp"
    #include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

VkResult VulkanAndroidPlatform::createSurface(
    const VulkanGenericPlatformWindowContext& windowContext, const VkInstance instance, VkSurfaceKHR* const outSurface
) noexcept
{
    GP_CHECKF(outSurface != nullptr, "createSurface requires a destination surface handle.");
    *outSurface = VK_NULL_HANDLE;

    void* const nativeWindow = windowContext.getWindowHandle();
    if (vkCreateAndroidSurfaceKHR == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (nativeWindow == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = static_cast<ANativeWindow*>(nativeWindow);
    return vkCreateAndroidSurfaceKHR(instance, &createInfo, GP_VULKAN_CPU_ALLOCATOR, outSurface);
}

}   // namespace gp::rhi::vulkan

#endif
