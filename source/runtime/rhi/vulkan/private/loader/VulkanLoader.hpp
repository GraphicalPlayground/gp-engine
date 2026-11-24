// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "loader/VulkanEntryPoints.hpp"      // IWYU pragma: export
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"              // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Dynamic Vulkan shared-library loader and multi-tier entry-point resolver.
/// @details Uses `gp::platform::SharedLibrary` to probe and load the platform Vulkan loader (`vulkan-1.dll`,
///          `libvulkan.so.1`, `libvulkan.1.dylib`, or `libvulkan.so`) without linking directly against import
///          libraries, letting `IDynamicRHIModule::isSupported()` probe Vulkan cleanly when drivers are absent.
///          Populates global function pointers in `gp::rhi::vulkan::dynamic_api` across base, instance, surface,
///          optional, platform, and device-direct dispatch tiers.
class GP_RHI_VULKAN_API VulkanLoader final
{
public:
    VulkanLoader() = delete;

    /// @brief Loads the Vulkan loader shared library and resolves base, optional-base, and platform-base entry points.
    /// @details When `explicitLibraryPath` is empty, caches the load attempt so repeated calls avoid redundant OS
    ///          library probes until `freeLibrary()` is invoked. If any required base entry point is missing, the
    ///          library is unloaded immediately and all pointers are cleared.
    /// @param[in] explicitLibraryPath Optional explicit library filename or path overriding platform candidates.
    /// @return True when the Vulkan loader library is loaded and all required base entry points were resolved.
    [[nodiscard]] static bool loadLibrary(gp::StringView explicitLibraryPath = {}) noexcept;

    /// @brief Unloads the Vulkan shared library, clears all cached entry-point pointers, and resets load state.
    static void freeLibrary() noexcept;

    /// @brief Reports whether the Vulkan shared library is currently loaded.
    /// @return True when a valid OS library handle is held.
    [[nodiscard]] static bool isLibraryLoaded() noexcept;

    /// @brief Returns the raw OS shared-library handle currently held by the loader.
    /// @return Opaque library handle returned by `gp::platform::SharedLibrary::getHandle()`, or `nullptr`.
    [[nodiscard]] static void* getLibraryHandle() noexcept;

    /// @brief Resolves instance, surface, optional, ray tracing, and platform instance entry points for `instance`.
    /// @details Uses `dynamic_api::vkGetInstanceProcAddr` to populate every function pointer. When
    ///          `allowMissingSurfaceFunctions` is false (and `-RenderOffScreen` is not set), missing required instance
    ///          or surface entry points cause this call to fail.
    /// @param[in] instance Active `VkInstance` handle.
    /// @param[in] allowMissingSurfaceFunctions True to allow headless execution when WSI surface functions are absent.
    /// @return True when all required entry points were resolved.
    [[nodiscard]] static bool
        loadInstanceFunctions(VkInstance instance, bool allowMissingSurfaceFunctions = false) noexcept;

    /// @brief Upgrades device-dispatchable entry points in `dynamic_api` using `vkGetDeviceProcAddr(device, ...)`.
    /// @details Bypasses Vulkan loader trampolines by replacing device, queue, and command-buffer function pointers
    ///          with direct ICD addresses. Instance-level and physical-device pointers (where `vkGetDeviceProcAddr`
    ///          returns `nullptr`) are left intact.
    /// @param[in] device Active `VkDevice` handle.
    /// @return True when `vkGetDeviceProcAddr` is available and `device` is non-null; false otherwise.
    [[nodiscard]] static bool loadDeviceFunctions(VkDevice device) noexcept;

    /// @brief Checks whether every ray tracing extension entry point in `GP_ENUM_VK_ENTRYPOINTS_RAYTRACING` is bound.
    /// @return True when all ray tracing function pointers are non-null.
    [[nodiscard]] static bool checkRayTracingInstanceFunctions() noexcept;

    /// @brief Resets instance, surface, optional, and platform-instance entry points back to `nullptr`.
    /// @details Preserves base loader functions (`vkCreateInstance`, `vkGetInstanceProcAddr`, etc.) so a new
    ///          `VkInstance` can be created without reloading the shared library.
    static void clearInstanceFunctions() noexcept;

    /// @brief Resets every entry point in `GP_ENUM_VK_ENTRYPOINTS_ALL` back to `nullptr`.
    static void clearAllFunctions() noexcept;

    /// @brief Resolves a single Vulkan instance or global procedure via `dynamic_api::vkGetInstanceProcAddr`.
    /// @param[in] instance Target `VkInstance` (or `VK_NULL_HANDLE` for global commands).
    /// @param[in] procName Null-terminated Vulkan function name.
    /// @return Resolved `PFN_vkVoidFunction`, or `nullptr` if unavailable.
    [[nodiscard]] static PFN_vkVoidFunction getInstanceProcAddr(VkInstance instance, const char* procName) noexcept;

    /// @brief Resolves a single Vulkan device-dispatchable procedure via `dynamic_api::vkGetDeviceProcAddr`.
    /// @param[in] device Target `VkDevice`.
    /// @param[in] procName Null-terminated Vulkan function name.
    /// @return Resolved `PFN_vkVoidFunction`, or `nullptr` if unavailable.
    [[nodiscard]] static PFN_vkVoidFunction getDeviceProcAddr(VkDevice device, const char* procName) noexcept;
};

}   // namespace gp::rhi::vulkan
