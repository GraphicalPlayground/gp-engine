// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platform/VulkanPlatform.hpp"     // IWYU pragma: keep
#include "RHIVulkan.hpp"                   // IWYU pragma: export
#include "VulkanCommon.hpp"                // IWYU pragma: export
#include "VulkanThirdParty.hpp"            // IWYU pragma: keep

/// @brief Base Vulkan API version targeted by the backend during instance creation.
#ifndef GP_VULKAN_API_VERSION
    #define GP_VULKAN_API_VERSION VK_API_VERSION_1_1
#endif

/// @brief Enables Vulkan debugging support in Development builds unless disabled by the active platform.
#ifndef GP_VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT
    #define GP_VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT 1
#endif

/// @brief Master compile-time switch enabling validation layers, debug callbacks, and diagnostic hooks.
#ifndef GP_VULKAN_HAS_DEBUGGING_ENABLED
    #define GP_VULKAN_HAS_DEBUGGING_ENABLED \
        (GP_BUILD_DEBUG || (GP_BUILD_DEVELOPMENT && GP_VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT))
#endif

/// @brief Default severity level for `r.Vulkan.EnableValidation`:
///        0 = disabled, 1 = errors, 2 = errors & warnings, 3 = +perf warnings, 4 = +info, 5 = all messages.
#ifndef GP_VULKAN_VALIDATION_DEFAULT_VALUE
    #define GP_VULKAN_VALIDATION_DEFAULT_VALUE (GP_BUILD_DEBUG ? 2 : 0)
#endif

/// @brief Platform-overridable opt-in for GPU draw markers and object debug names.
#ifndef GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS
    #define GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS 0
#endif

/// @brief Resolved compile-time flag controlling GPU command-buffer draw markers.
#define GP_VULKAN_ENABLE_DRAW_MARKERS GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS

/// @brief Resolved compile-time flag controlling `VK_EXT_debug_utils` object naming.
#ifndef GP_VULKAN_USE_DEBUG_NAMES
    #define GP_VULKAN_USE_DEBUG_NAMES GP_VULKAN_ENABLE_DRAW_MARKERS
#endif

/// @brief Enables hashing descriptor and memory pools by resource type and usage ID.
#ifndef GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID
    #define GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID 1
#endif

/// @brief Forces a dedicated device-memory allocation per Vulkan resource when set to 1 (diagnostics only).
#ifndef GP_VULKAN_SINGLE_ALLOCATION_PER_RESOURCE
    #define GP_VULKAN_SINGLE_ALLOCATION_PER_RESOURCE 0
#endif

/// @brief Platform-overridable opt-in for routing Vulkan CPU allocations through low-level memory tracking.
#ifndef GP_VULKAN_SHOULD_USE_LLM
    #define GP_VULKAN_SHOULD_USE_LLM 0
#endif

/// @brief Resolved compile-time flag enabling low-level memory tracker hooks for Vulkan CPU allocations.
#ifndef GP_VULKAN_USE_LLM
    #if defined(GP_ENABLE_LOW_LEVEL_MEM_TRACKER)
        #define GP_VULKAN_USE_LLM ((GP_ENABLE_LOW_LEVEL_MEM_TRACKER) && GP_VULKAN_SHOULD_USE_LLM)
    #else
        #define GP_VULKAN_USE_LLM 0
    #endif
#endif

/// @brief Enables custom `VkAllocationCallbacks` for Vulkan driver CPU allocations.
#ifndef GP_VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
    #define GP_VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED GP_VULKAN_USE_LLM
#endif

/// @brief Enables high-frequency diagnostic statistics collection inside hot Vulkan paths.
#ifndef GP_VULKAN_ENABLE_AGGRESSIVE_STATS
    #define GP_VULKAN_ENABLE_AGGRESSIVE_STATS 0
#endif

/// @brief Enables pooling and recycling `VkFence` objects instead of recreating them.
#ifndef GP_VULKAN_REUSE_FENCES
    #define GP_VULKAN_REUSE_FENCES 1
#endif

/// @brief Captures CPU callstacks alongside GPU query submissions when set to 1.
#ifndef GP_VULKAN_QUERY_CALLSTACK
    #define GP_VULKAN_QUERY_CALLSTACK 0
#endif

/// @brief Enables desktop HMD extension integration when supported by the platform.
#ifndef GP_VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
    #define GP_VULKAN_ENABLE_DESKTOP_HMD_SUPPORT 0
#endif

/// @brief Diagnostic trap macro invoked in stubbed or unimplemented Vulkan paths.
#ifndef GP_VULKAN_SIGNAL_UNIMPLEMENTED
    #define GP_VULKAN_SIGNAL_UNIMPLEMENTED()
#endif

/// @brief Enables LRU eviction for Vulkan pipeline caches on memory-constrained platforms.
#ifndef GP_VULKAN_ENABLE_LRU_CACHE
    #define GP_VULKAN_ENABLE_LRU_CACHE 0
#endif

/// @brief Advertises support for `VK_EXT_validation_cache`.
#ifndef GP_VULKAN_SUPPORTS_VALIDATION_CACHE
    #define GP_VULKAN_SUPPORTS_VALIDATION_CACHE 1
#endif

/// @brief Advertises support for `VK_EXT_validation_features`.
#ifndef GP_VULKAN_HAS_VALIDATION_FEATURES
    #define GP_VULKAN_HAS_VALIDATION_FEATURES 1
#endif

/// @brief Advertises support for `VK_KHR_dedicated_allocation`.
#ifndef GP_VULKAN_SUPPORTS_DEDICATED_ALLOCATION
    #define GP_VULKAN_SUPPORTS_DEDICATED_ALLOCATION 1
#endif

/// @brief Advertises support for `VK_GOOGLE_display_timing`.
#ifndef GP_VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
    #define GP_VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING 0
#endif

/// @brief Allocates separate command pools per command buffer usage pattern.
#ifndef GP_VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
    #define GP_VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS 1
#endif

/// @brief Reclaims idle command buffers that remain unused across multiple frames.
#ifndef GP_VULKAN_DELETE_STALE_CMDBUFFERS
    #define GP_VULKAN_DELETE_STALE_CMDBUFFERS 1
#endif

/// @brief Advertises support for `VK_EXT_scalar_block_layout`.
#ifndef GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT
    #define GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT 1
#endif

/// @brief Advertises support for `VK_EXT_memory_budget`.
#ifndef GP_VULKAN_SUPPORTS_MEMORY_BUDGET
    #define GP_VULKAN_SUPPORTS_MEMORY_BUDGET 1
#endif

/// @brief Advertises support for `VK_EXT_memory_priority`.
#ifndef GP_VULKAN_SUPPORTS_MEMORY_PRIORITY
    #define GP_VULKAN_SUPPORTS_MEMORY_PRIORITY 1
#endif

/// @brief Advertises support for `VK_KHR_driver_properties`.
#ifndef GP_VULKAN_SUPPORTS_DRIVER_PROPERTIES
    #define GP_VULKAN_SUPPORTS_DRIVER_PROPERTIES 1
#endif

/// @brief Advertises support for `VK_QCOM_render_pass_shader_resolve`.
#ifndef GP_VULKAN_SUPPORTS_QCOM_RENDERPASS_SHADER_RESOLVE
    #define GP_VULKAN_SUPPORTS_QCOM_RENDERPASS_SHADER_RESOLVE 1
#endif

/// @brief Advertises support for `VK_EXT_full_screen_exclusive`.
#ifndef GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
    #define GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE 0
#endif

/// @brief Advertises support for `VK_EXT_texture_compression_astc_hdr`.
#ifndef GP_VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR
    #define GP_VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR 1
#endif

/// @brief Advertises support for `VK_ANDROID_external_memory_android_hardware_buffer`.
#ifndef GP_VULKAN_SUPPORTS_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER
    #define GP_VULKAN_SUPPORTS_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER 1
#endif

/// @brief Advertises support for `VK_EXT_astc_decode_mode`.
#ifndef GP_VULKAN_SUPPORTS_ASTC_DECODE_MODE
    #define GP_VULKAN_SUPPORTS_ASTC_DECODE_MODE 1
#endif

/// @brief Advertises support for `VK_EXT_shader_viewport_index_layer`.
#ifndef GP_VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER
    #define GP_VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER 1
#endif

/// @brief Advertises support for `VK_EXT_descriptor_indexing`.
#ifndef GP_VULKAN_SUPPORTS_DESCRIPTOR_INDEXING
    #define GP_VULKAN_SUPPORTS_DESCRIPTOR_INDEXING 1
#endif

/// @brief Advertises support for `VK_KHR_ray_tracing_position_fetch`.
#ifndef GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH
    #define GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH 0
#endif

/// @brief Enables tracking live Vulkan objects and memory usage for diagnostic memory dumps.
#ifndef GP_VULKAN_OBJECT_TRACKING
    #define GP_VULKAN_OBJECT_TRACKING 0
#endif

/// @brief Enables purging compiled `VkShaderModule` handles after PSO creation to save host memory.
#ifndef GP_VULKAN_PURGE_SHADER_MODULES
    #define GP_VULKAN_PURGE_SHADER_MODULES 0
#endif

/// @brief Enables transient resource heap suballocation in the Vulkan backend.
#ifndef GP_VULKAN_SUPPORTS_TRANSIENT_RESOURCE_ALLOCATOR
    #define GP_VULKAN_SUPPORTS_TRANSIENT_RESOURCE_ALLOCATOR 1
#endif

/// @brief Resolves the default `VkAllocationCallbacks*` passed to Vulkan API entry points.
#if GP_VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
    #define GP_VULKAN_CPU_ALLOCATOR gp::rhi::vulkan::getMemoryAllocator(nullptr)
#else
    #define GP_VULKAN_CPU_ALLOCATOR nullptr
#endif

/// @brief Formats a Vulkan enum value as a decimal string when no SDK string helper is provided.
#ifndef GP_VK_TYPE_TO_STRING
    #define GP_VK_TYPE_TO_STRING(Type, Value) gp::rhi::vulkan::formatVulkanValue(static_cast<gp::UInt32>(Value))
#endif

/// @brief Formats a Vulkan bitmask flags value as a decimal string when no SDK string helper is provided.
#ifndef GP_VK_FLAGS_TO_STRING
    #define GP_VK_FLAGS_TO_STRING(Type, Value) gp::rhi::vulkan::formatVulkanValue(static_cast<gp::UInt32>(Value))
#endif

namespace gp::rhi::vulkan
{

/// @brief Formats an unsigned 32-bit Vulkan enum or bitflag value into a decimal `gp::String`.
/// @param[in] value Numeric Vulkan enum or bitmask value.
/// @return Decimal string representation of `value`.
[[nodiscard]] inline gp::String formatVulkanValue(const gp::UInt32 value) noexcept
{
    char buffer[16]{};
    gp::UInt32 count = 0u;
    gp::UInt32 remaining = value;
    do
    {
        buffer[count++] = static_cast<char>('0' + (remaining % 10u));
        remaining /= 10u;
    }
    while (remaining != 0u);

    gp::String result;
    for (gp::UInt32 i = count; i > 0u; --i)
    {
        result.pushBack(buffer[i - 1u]);
    }
    return result;
}

/// @brief Validation verbosity levels matching `r.Vulkan.EnableValidation`.
enum class VulkanValidationLevel : gp::Int32
{
    /// @brief Validation layers disabled.
    Disabled = 0,
    /// @brief Report validation errors only.
    Errors = 1,
    /// @brief Report errors and warnings.
    ErrorsAndWarnings = 2,
    /// @brief Report errors, warnings, and performance warnings.
    ErrorsWarningsAndPerformance = 3,
    /// @brief Report errors, warnings, performance warnings, and informational messages.
    ErrorsWarningsPerfAndInfo = 4,
    /// @brief Report every diagnostic message emitted by validation layers.
    All = 5
};

/// @brief Base Vulkan API version requested during instance initialization (defaults to `VK_API_VERSION_1_1`).
inline constexpr gp::UInt32 kVulkanApiVersion = GP_VULKAN_API_VERSION;

/// @brief Compile-time default validation severity (`ErrorsAndWarnings` in Debug, `Disabled` otherwise).
inline constexpr VulkanValidationLevel kDefaultVulkanValidationLevel =
    static_cast<VulkanValidationLevel>(GP_VULKAN_VALIDATION_DEFAULT_VALUE);

/// @brief True when Vulkan debugging hooks (validation layers, debug messenger) are compiled in.
inline constexpr bool kVulkanHasDebuggingEnabled = (GP_VULKAN_HAS_DEBUGGING_ENABLED != 0);

/// @brief True when command-buffer GPU draw markers are compiled in.
inline constexpr bool kVulkanEnableDrawMarkers = (GP_VULKAN_ENABLE_DRAW_MARKERS != 0);

/// @brief True when Vulkan object debug names are compiled in.
inline constexpr bool kVulkanUseDebugNames = (GP_VULKAN_USE_DEBUG_NAMES != 0);

/// @brief True when `VkFence` handles are pooled and recycled.
inline constexpr bool kVulkanReuseFences = (GP_VULKAN_REUSE_FENCES != 0);

/// @brief True when custom `VkAllocationCallbacks` are enabled for Vulkan host allocations.
inline constexpr bool kVulkanCustomMemoryManagerEnabled = (GP_VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED != 0);

/// @brief Returns the `VkAllocationCallbacks` pointer to supply to Vulkan creation and destruction calls.
/// @param[in] allocator Optional caller-specified callbacks overriding the default allocator.
/// @return `allocator` when non-null, the global engine callbacks when `GP_VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED` is
///         enabled, or `nullptr`.
[[nodiscard]] inline const VkAllocationCallbacks*
    getMemoryAllocator(const VkAllocationCallbacks* const allocator) noexcept
{
#if GP_VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
    extern GP_RHI_VULKAN_API VkAllocationCallbacks g_vulkanAllocationCallbacks;
    return (allocator != nullptr) ? allocator : &g_vulkanAllocationCallbacks;
#else
    return allocator;
#endif
}

}   // namespace gp::rhi::vulkan
