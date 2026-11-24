// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"

// Enable Xlib and Wayland WSI surface extensions when their system headers are present.
#if !defined(VK_USE_PLATFORM_XLIB_KHR) && !defined(GP_VULKAN_DISABLE_XLIB_KHR)
    #if defined(__has_include)
        #if __has_include(<X11/Xlib.h>)
            #define VK_USE_PLATFORM_XLIB_KHR 1
        #endif
    #endif
#endif

#if !defined(VK_USE_PLATFORM_WAYLAND_KHR) && !defined(GP_VULKAN_DISABLE_WAYLAND_KHR)
    #if defined(__has_include)
        #if __has_include(<wayland-client.h>)
            #define VK_USE_PLATFORM_WAYLAND_KHR 1
        #endif
    #endif
#endif

#ifndef GP_VULKAN_DYNAMICALLY_LOADED
    #define GP_VULKAN_DYNAMICALLY_LOADED 1
#endif

#ifndef GP_VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT
    #define GP_VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT 1
#endif

#ifndef GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS
    #define GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS (GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT)
#endif

#ifndef GP_VULKAN_SIGNAL_UNIMPLEMENTED
    #define GP_VULKAN_SIGNAL_UNIMPLEMENTED() \
        GP_CHECKF(false, "Unimplemented Vulkan functionality: %s", __PRETTY_FUNCTION__)
#endif

#ifndef GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT
    #define GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT 1
#endif

#ifndef GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH
    #define GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH 1
#endif

#ifndef GP_VULKAN_API_VERSION
    #define GP_VULKAN_API_VERSION VK_API_VERSION_1_1
#endif

/// @brief Platform-specific base Vulkan entry points loaded directly from `libvulkan.so.1`.
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

/// @brief Platform-specific required Vulkan instance entry points on Linux.
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro)

/// @brief Optional Linux WSI surface creation entry points resolved according to active display servers.
#if defined(VK_USE_PLATFORM_XLIB_KHR) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    #define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)    \
        EnumMacro(PFN_vkCreateXlibSurfaceKHR, vkCreateXlibSurfaceKHR)       \
        EnumMacro(PFN_vkCreateWaylandSurfaceKHR, vkCreateWaylandSurfaceKHR)
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    #define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro) \
        EnumMacro(PFN_vkCreateXlibSurfaceKHR, vkCreateXlibSurfaceKHR)
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    #define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro) \
        EnumMacro(PFN_vkCreateWaylandSurfaceKHR, vkCreateWaylandSurfaceKHR)
#else
    #define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)
#endif

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "platform/VulkanGenericPlatform.hpp"

namespace gp::rhi::vulkan
{

/// @brief Linux window context storing an immutable window identifier captured while the window is alive.
/// @details Prevents deferred surface teardown from dereferencing a recycled or destroyed native window pointer.
class GP_RHI_VULKAN_API VulkanLinuxPlatformWindowContext : public VulkanGenericPlatformWindowContext
{
public:
    /// @brief Constructs a Linux window context from an opaque window pointer and optional stable window ID.
    /// @param[in] inWindowHandle Native window pointer.
    /// @param[in] inWindowId Immutable window identifier captured at construction (`0u` if unavailable).
    explicit VulkanLinuxPlatformWindowContext(
        void* const inWindowHandle = nullptr, const gp::UInt32 inWindowId = 0u
    ) noexcept
        : VulkanGenericPlatformWindowContext(inWindowHandle)
        , m_windowId(inWindowId)
    {}

    /// @brief Constructs a Linux window context from a HAL `NativeWindowHandle` and optional stable window ID.
    /// @param[in] inNativeHandle Native HAL window descriptor (X11 or Wayland).
    /// @param[in] inWindowId Immutable window identifier captured at construction (`0u` if unavailable).
    explicit VulkanLinuxPlatformWindowContext(
        const gp::hal::NativeWindowHandle& inNativeHandle, const gp::UInt32 inWindowId = 0u
    ) noexcept
        : VulkanGenericPlatformWindowContext(inNativeHandle)
        , m_windowId(inWindowId)
    {}

    /// @brief Returns the immutable window identifier captured when this context was constructed.
    /// @return Stable window identifier (`0u` if unset).
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getWindowId() const noexcept
    {
        return m_windowId;
    }

    /// @brief Reports whether the underlying Linux window handle is still valid.
    /// @details Validates both display and surface/window handles when constructed from an X11 or Wayland
    ///          `NativeWindowHandle`.
    /// @return True when the wrapped window handle is valid for presentation.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        if (m_nativeHandle.backend == gp::hal::NativeWindowBackend::X11)
        {
            return m_nativeHandle.x11.display != nullptr && m_nativeHandle.x11.window != 0u;
        }
        if (m_nativeHandle.backend == gp::hal::NativeWindowBackend::Wayland)
        {
            return m_nativeHandle.wayland.display != nullptr && m_nativeHandle.wayland.surface != nullptr;
        }
        return m_windowHandle != nullptr;
    }

private:
    /// @brief Immutable window ID captured at construction time.
    gp::UInt32 m_windowId{ 0u };
};

/// @brief Linux platform abstraction layer for Vulkan loader, Xlib/Wayland WSI extensions, and monotonic timestamps.
class GP_RHI_VULKAN_API VulkanLinuxPlatform : public VulkanGenericPlatform
{
public:
    VulkanLinuxPlatform() = delete;

    /// @brief Returns the standard Linux Vulkan loader library filename (`libvulkan.so.1`).
    /// @return Null-terminated shared library name.
    [[nodiscard]] static constexpr const char* getDefaultLibraryName() noexcept
    {
        return "libvulkan.so.1";
    }

    /// @brief Appends fallback Linux Vulkan loader library filenames (`libvulkan.so`).
    /// @param[in,out] outFallbacks Destination list of fallback shared library filenames.
    static void getFallbackLibraryNames(gp::Vector<const char*>& outFallbacks) noexcept
    {
        outFallbacks.pushBack("libvulkan.so");
    }

    /// @brief Appends available Linux WSI instance extension names (Xlib and/or Wayland).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformInstanceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        outExtensions.pushBack(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        outExtensions.pushBack(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
        static_cast<void>(outExtensions);
    }

    /// @brief Appends Linux-specific Vulkan device extension names (POSIX file-descriptor external memory/semaphore).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformDeviceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
        outExtensions.pushBack(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
        outExtensions.pushBack(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    }

    /// @brief Returns `VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR` matching Linux monotonic clock queries.
    /// @return Calibrated timestamp domain for Linux.
    [[nodiscard]] static constexpr VkTimeDomainKHR getTimeDomain() noexcept
    {
        return VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR;
    }

    /// @brief Converts a Linux `CLOCK_MONOTONIC` nanosecond timestamp into CPU raw performance counter units.
    /// @details `gp::platform::unix::Time::rawPerformanceCounter()` reports `CLOCK_MONOTONIC` in 1 ns ticks, so the
    ///          conversion is an identity mapping.
    /// @param[in] timestamp Raw nanosecond timestamp from `vkGetCalibratedTimestampsKHR`.
    /// @return Identical `timestamp` in 1 ns ticks.
    [[nodiscard]] static constexpr gp::UInt64 calibratedTimestampToCycles64(const gp::UInt64 timestamp) noexcept
    {
        return timestamp;
    }

    /// @brief Reports whether ASTC texture formats are supported (enabled on Linux ARM64).
    /// @return True on ARM64 Linux; false on x86/x64 Linux.
    [[nodiscard]] static constexpr bool supportsASTCTextureFormats() noexcept
    {
        return static_cast<bool>(GP_ARCHITECTURE_ARM64);
    }

    /// @brief Reports whether ETC2 texture formats are supported (enabled on Linux ARM64).
    /// @return True on ARM64 Linux; false on x86/x64 Linux.
    [[nodiscard]] static constexpr bool supportsETC2TextureFormats() noexcept
    {
        return static_cast<bool>(GP_ARCHITECTURE_ARM64);
    }
};

}   // namespace gp::rhi::vulkan
