// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"

// macOS / Apple Metal WSI surface extension enablement (must precede `<vulkan/vulkan.h>`).
#ifndef VK_USE_PLATFORM_METAL_EXT
    #define VK_USE_PLATFORM_METAL_EXT 1
#endif

#ifndef GP_VULKAN_DYNAMICALLY_LOADED
    #define GP_VULKAN_DYNAMICALLY_LOADED 1
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

#ifndef GP_VULKAN_API_VERSION
    #define GP_VULKAN_API_VERSION VK_API_VERSION_1_1
#endif

/// @brief Platform-specific base Vulkan entry points loaded from `libvulkan.1.dylib`.
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

/// @brief Platform-specific Vulkan instance entry points on macOS (`VK_EXT_metal_surface`).
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro) \
    EnumMacro(PFN_vkCreateMetalSurfaceEXT, vkCreateMetalSurfaceEXT)

/// @brief Optional platform-specific Vulkan instance entry points on macOS.
#define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "platform/VulkanGenericPlatform.hpp"

namespace gp::rhi::vulkan
{

/// @brief macOS platform abstraction layer for Vulkan over MoltenVK / portability subsets and Metal surfaces.
class GP_RHI_VULKAN_API VulkanMacPlatform : public VulkanGenericPlatform
{
public:
    VulkanMacPlatform() = delete;

    /// @brief Returns the standard macOS Vulkan loader dynamic library filename (`libvulkan.1.dylib`).
    /// @return Null-terminated dynamic library name.
    [[nodiscard]] static constexpr const char* getDefaultLibraryName() noexcept
    {
        return "libvulkan.1.dylib";
    }

    /// @brief Appends fallback macOS Vulkan / MoltenVK loader filenames.
    /// @param[in,out] outFallbacks Destination list of fallback dynamic library filenames.
    static void getFallbackLibraryNames(gp::Vector<const char*>& outFallbacks) noexcept
    {
        outFallbacks.pushBack("libvulkan.dylib");
        outFallbacks.pushBack("libMoltenVK.dylib");
    }

    /// @brief Appends macOS-required Vulkan instance extension names (`VK_EXT_metal_surface` and portability
    ///        enumeration).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformInstanceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
        outExtensions.pushBack(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#if defined(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
        outExtensions.pushBack(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
    }

    /// @brief Reports whether the host Mac uses unified CPU/GPU memory (true on Apple Silicon ARM64).
    /// @return True on ARM64; false on x64.
    [[nodiscard]] static constexpr bool hasUnifiedMemory() noexcept
    {
        return static_cast<bool>(GP_ARCHITECTURE_ARM64);
    }
};

}   // namespace gp::rhi::vulkan
