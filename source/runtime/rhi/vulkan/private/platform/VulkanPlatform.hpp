// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

#if GP_PLATFORM_WINDOWS
    #include "platform/windows/VulkanWindowsPlatform.hpp"
#elif GP_PLATFORM_LINUX
    #include "platform/linux/VulkanLinuxPlatform.hpp"
#elif GP_PLATFORM_MACOS
    #include "platform/macos/VulkanMacPlatform.hpp"
#elif defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
    #include "platform/android/VulkanAndroidPlatform.hpp"
#else
    #ifndef GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE
        #define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)
    #endif
    #ifndef GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE
        #define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro)
    #endif
    #ifndef GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE
        #define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)
    #endif
    #include "platform/VulkanGenericPlatform.hpp"
#endif

namespace gp::rhi::vulkan
{

#if GP_PLATFORM_WINDOWS
/// @brief Active host platform Vulkan abstraction type.
using VulkanPlatform = VulkanWindowsPlatform;

/// @brief Active host platform Vulkan native window context type.
using VulkanPlatformWindowContext = VulkanGenericPlatformWindowContext;
#elif GP_PLATFORM_LINUX
/// @brief Active host platform Vulkan abstraction type.
using VulkanPlatform = VulkanLinuxPlatform;

/// @brief Active host platform Vulkan native window context type.
using VulkanPlatformWindowContext = VulkanLinuxPlatformWindowContext;
#elif GP_PLATFORM_MACOS
/// @brief Active host platform Vulkan abstraction type.
using VulkanPlatform = VulkanMacPlatform;

/// @brief Active host platform Vulkan native window context type.
using VulkanPlatformWindowContext = VulkanGenericPlatformWindowContext;
#elif defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
/// @brief Active host platform Vulkan abstraction type.
using VulkanPlatform = VulkanAndroidPlatform;

/// @brief Active host platform Vulkan native window context type.
using VulkanPlatformWindowContext = VulkanAndroidPlatformWindowContext;
#else
/// @brief Active host platform Vulkan abstraction type.
using VulkanPlatform = VulkanGenericPlatform;

/// @brief Active host platform Vulkan native window context type.
using VulkanPlatformWindowContext = VulkanGenericPlatformWindowContext;
#endif

}   // namespace gp::rhi::vulkan
