// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"

// Windows WSI surface extension enablement (must precede `<vulkan/vulkan.h>`).
#ifndef VK_USE_PLATFORM_WIN32_KHR
    #define VK_USE_PLATFORM_WIN32_KHR 1
#endif

#ifndef GP_VULKAN_DYNAMICALLY_LOADED
    #define GP_VULKAN_DYNAMICALLY_LOADED 1
#endif

#ifndef GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS
    #define GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS (GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT)
#endif

#ifndef GP_VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
    #define GP_VULKAN_ENABLE_DESKTOP_HMD_SUPPORT 1
#endif

#ifndef GP_VULKAN_SIGNAL_UNIMPLEMENTED
    #define GP_VULKAN_SIGNAL_UNIMPLEMENTED() GP_CHECKF(false, "Unimplemented Vulkan functionality: %s", __FUNCTION__)
#endif

#ifndef GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT
    #define GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT 1
#endif

#ifndef GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH
    #define GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH 1
#endif

#ifndef GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
    #define GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE 1
#endif

#ifndef GP_VULKAN_API_VERSION
    #define GP_VULKAN_API_VERSION VK_API_VERSION_1_1
#endif

#ifndef GP_VULKAN_SHOULD_USE_LLM
    #define GP_VULKAN_SHOULD_USE_LLM (GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT)
#endif

/// @brief Platform-specific base Vulkan entry points loaded directly from `vulkan-1.dll`.
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

/// @brief Platform-specific Vulkan instance entry points resolved via `vkGetInstanceProcAddr` on Windows.
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro) \
    EnumMacro(PFN_vkCreateWin32SurfaceKHR, vkCreateWin32SurfaceKHR)

/// @brief Optional platform-specific Vulkan instance entry points on Windows.
#define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "platform/VulkanGenericPlatform.hpp"

namespace gp::rhi::vulkan
{

/// @brief Windows platform abstraction layer for Vulkan loader, WSI extensions, and QPC timestamp calibration.
class GP_RHI_VULKAN_API VulkanWindowsPlatform : public VulkanGenericPlatform
{
public:
    VulkanWindowsPlatform() = delete;

    /// @brief Returns the Windows Vulkan loader DLL filename (`vulkan-1.dll`).
    /// @return Null-terminated library name.
    [[nodiscard]] static constexpr const char* getDefaultLibraryName() noexcept
    {
        return "vulkan-1.dll";
    }

    /// @brief Appends Windows-required Vulkan instance extension names (`VK_KHR_win32_surface`).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformInstanceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
        outExtensions.pushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    }

    /// @brief Appends Windows-specific Vulkan device extension names (external memory/semaphore and fullscreen
    ///        exclusive).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformDeviceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
#if GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
        outExtensions.pushBack(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
#endif
        outExtensions.pushBack(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
        outExtensions.pushBack(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    }

    /// @brief Returns `VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR`, matching Win32 QPC ticks directly.
    /// @return Calibrated timestamp domain for Windows.
    [[nodiscard]] static constexpr VkTimeDomainKHR getTimeDomain() noexcept
    {
        return VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR;
    }

    /// @brief Converts a Win32 QPC calibrated timestamp to CPU raw performance counter units (identity mapping).
    /// @param[in] timestamp Raw QPC value returned by `vkGetCalibratedTimestampsKHR`.
    /// @return Identical `timestamp` in QPC cycles.
    [[nodiscard]] static constexpr gp::UInt64 calibratedTimestampToCycles64(const gp::UInt64 timestamp) noexcept
    {
        return timestamp;
    }
};

}   // namespace gp::rhi::vulkan
