// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

// Android WSI surface extension enablement (must precede `<vulkan/vulkan.h>`).
#ifndef VK_USE_PLATFORM_ANDROID_KHR
    #define VK_USE_PLATFORM_ANDROID_KHR 1
#endif

#ifndef GP_VULKAN_DYNAMICALLY_LOADED
    #define GP_VULKAN_DYNAMICALLY_LOADED 1
#endif

#ifndef GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS
    #define GP_VULKAN_SHOULD_ENABLE_DRAW_MARKERS (GP_BUILD_DEVELOPMENT || GP_BUILD_DEBUG)
#endif

#ifndef GP_VULKAN_SHOULD_USE_LLM
    #define GP_VULKAN_SHOULD_USE_LLM (GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT)
#endif

#ifndef GP_VULKAN_ENABLE_LRU_CACHE
    #define GP_VULKAN_ENABLE_LRU_CACHE 1
#endif

#ifndef GP_VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
    #define GP_VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING 1
#endif

#ifndef GP_VULKAN_PURGE_SHADER_MODULES
    #define GP_VULKAN_PURGE_SHADER_MODULES 0
#endif

#ifndef GP_VULKAN_SUPPORTS_DEDICATED_ALLOCATION
    #define GP_VULKAN_SUPPORTS_DEDICATED_ALLOCATION 0
#endif

#ifndef GP_VULKAN_SUPPORTS_ASTC_DECODE_MODE
    #define GP_VULKAN_SUPPORTS_ASTC_DECODE_MODE 1
#endif

#ifndef GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT
    #define GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT 1
#endif

#ifndef GP_VULKAN_SUPPORTS_TRANSIENT_RESOURCE_ALLOCATOR
    #define GP_VULKAN_SUPPORTS_TRANSIENT_RESOURCE_ALLOCATOR 0
#endif

#ifndef GP_VULKAN_SUPPORTS_DRIVER_PROPERTIES
    #define GP_VULKAN_SUPPORTS_DRIVER_PROPERTIES 0
#endif

#ifndef GP_VULKAN_SUPPORTS_DESCRIPTOR_INDEXING
    #define GP_VULKAN_SUPPORTS_DESCRIPTOR_INDEXING 1
#endif

#ifndef GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH
    #define GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH 0
#endif

#ifndef GP_VULKAN_SUPPORTS_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER
    #define GP_VULKAN_SUPPORTS_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER 1
#endif

#ifndef GP_VULKAN_HAS_DEBUGGING_ENABLED
    #define GP_VULKAN_HAS_DEBUGGING_ENABLED (GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT || GP_BUILD_PROFILE)
#endif

#ifndef GP_VULKAN_API_VERSION
    #define GP_VULKAN_API_VERSION VK_API_VERSION_1_1
#endif

/// @brief Platform-specific base Vulkan entry points on Android.
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

/// @brief Platform-specific required Vulkan instance entry points on Android.
#define GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro)             \
    EnumMacro(PFN_vkCreateAndroidSurfaceKHR, vkCreateAndroidSurfaceKHR) \
    EnumMacro(PFN_vkGetAndroidHardwareBufferPropertiesANDROID, vkGetAndroidHardwareBufferPropertiesANDROID)

/// @brief Optional Android Vulkan instance entry points (`VK_GOOGLE_display_timing`).
#define GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)                \
    EnumMacro(PFN_vkGetRefreshCycleDurationGOOGLE, vkGetRefreshCycleDurationGOOGLE) \
    EnumMacro(PFN_vkGetPastPresentationTimingGOOGLE, vkGetPastPresentationTimingGOOGLE)

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "platform/VulkanGenericPlatform.hpp"

namespace gp::rhi::vulkan
{

/// @brief Android window context managing `ANativeWindow` surface lifetime constraints.
class GP_RHI_VULKAN_API VulkanAndroidPlatformWindowContext : public VulkanGenericPlatformWindowContext
{
public:
    /// @brief Constructs an Android window context from an opaque `ANativeWindow*`.
    /// @param[in] inWindowHandle Native `ANativeWindow*` handle.
    explicit VulkanAndroidPlatformWindowContext(void* const inWindowHandle = nullptr) noexcept
        : VulkanGenericPlatformWindowContext(inWindowHandle)
    {}

    /// @brief Constructs an Android window context from a HAL `NativeWindowHandle`.
    /// @param[in] inNativeHandle Native HAL window descriptor carrying `android.window`.
    explicit VulkanAndroidPlatformWindowContext(const gp::hal::NativeWindowHandle& inNativeHandle) noexcept
        : VulkanGenericPlatformWindowContext(inNativeHandle)
    {}

    /// @brief Android recreates swapchains through surface lifecycle callbacks rather than during present.
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool canCreateSwapchainOnDemand() noexcept
    {
        return false;
    }

    /// @brief Reports whether the wrapped `ANativeWindow` pointer is non-null.
    /// @return True when a native window is attached.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return m_windowHandle != nullptr;
    }
};

/// @brief Android platform abstraction layer for mobile Vulkan feature limits, compression formats, and WSI.
class GP_RHI_VULKAN_API VulkanAndroidPlatform : public VulkanGenericPlatform
{
public:
    VulkanAndroidPlatform() = delete;

    /// @brief Returns the Android Vulkan loader shared object filename (`libvulkan.so`).
    /// @return Null-terminated shared library name.
    [[nodiscard]] static constexpr const char* getDefaultLibraryName() noexcept
    {
        return "libvulkan.so";
    }

    /// @brief Appends Android-required Vulkan instance extension names (`VK_KHR_android_surface`).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformInstanceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
        outExtensions.pushBack(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
    }

    /// @brief Appends Android-specific Vulkan device extension names (display timing, ASTC decode mode, external AHB).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformDeviceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
#if GP_VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
        outExtensions.pushBack(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
#endif
#if GP_VULKAN_SUPPORTS_ASTC_DECODE_MODE
        outExtensions.pushBack(VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME);
#endif
        outExtensions.pushBack(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME);
#if GP_VULKAN_SUPPORTS_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER
        outExtensions.pushBack(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
#endif
    }

    /// @brief Creates an Android (`VK_KHR_android_surface`) `VkSurfaceKHR` for `windowContext`.
    /// @param[in] windowContext Native window context carrying the `ANativeWindow*`.
    /// @param[in] instance Owning `VkInstance`.
    /// @param[out] outSurface Receives the created surface, or `VK_NULL_HANDLE` on failure.
    /// @return `VK_SUCCESS` on success, or the failing `VkResult`.
    [[nodiscard]] static VkResult createSurface(
        const VulkanGenericPlatformWindowContext& windowContext, VkInstance instance, VkSurfaceKHR* outSurface
    ) noexcept;

    /// @brief Reports whether BC1..BC7 texture formats are supported on Android (false by default on mobile GPUs).
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool supportsBCTextureFormats() noexcept
    {
        return false;
    }

    /// @brief Reports whether ASTC texture compression is supported on Android (always true).
    /// @return Always `true`.
    [[nodiscard]] static constexpr bool supportsASTCTextureFormats() noexcept
    {
        return true;
    }

    /// @brief Reports whether ETC2 texture compression is supported on Android (always true).
    /// @return Always `true`.
    [[nodiscard]] static constexpr bool supportsETC2TextureFormats() noexcept
    {
        return true;
    }

    /// @brief Reports whether `R16_UNorm` texture formats are supported (false on Android; falls back to `R16F`).
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool supportsR16UnormTextureFormat() noexcept
    {
        return false;
    }

    /// @brief Reports whether surface properties can be queried reliably on Android.
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool supportsQuerySurfaceProperties() noexcept
    {
        return false;
    }

    /// @brief Reports whether Android defaults to the mobile rendering path (`FeatureLevel::ES3_1`).
    /// @return Always `true`.
    [[nodiscard]] static constexpr bool requiresMobileRenderer() noexcept
    {
        return true;
    }

    /// @brief Reports whether rendering directly to 3D volume textures is supported on Android.
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool supportsVolumeTextureRendering() noexcept
    {
        return false;
    }

    /// @brief Reports whether Android drivers require the present layout access-flag workaround.
    /// @return Always `true`.
    [[nodiscard]] static constexpr bool requiresPresentLayoutFix() noexcept
    {
        return true;
    }

    /// @brief Reports whether Android devices share unified CPU/GPU memory.
    /// @return Always `true`.
    [[nodiscard]] static constexpr bool hasUnifiedMemory() noexcept
    {
        return true;
    }

    /// @brief Reports whether parallel command recording tasks are enabled on mobile devices.
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool supportParallelRenderingTasks() noexcept
    {
        return false;
    }

    /// @brief Reports whether Android swapchain images require `VK_IMAGE_LAYOUT_GENERAL` as their initial layout.
    /// @return Always `true`.
    [[nodiscard]] static constexpr bool requiresSwapchainGeneralInitialLayout() noexcept
    {
        return true;
    }

    /// @brief Reports whether Android waits on a frame completion event between frames (disabled by default on mobile).
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool requiresWaitingForFrameCompletionEvent() noexcept
    {
        return false;
    }

    /// @brief Reports whether Android graphics pipelines may omit a fragment shader (false on mobile drivers).
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool supportsNullPixelShader() noexcept
    {
        return false;
    }

    /// @brief Reports whether swapchain recreation is attempted immediately on present/acquire failure on Android.
    /// @return Always `false`.
    [[nodiscard]] static constexpr bool recreateSwapchainOnFail() noexcept
    {
        return false;
    }

    /// @brief Returns the preferred 5551 packed pixel format and two fallback formats for Android GPUs.
    /// @param[out] outFallback1 First fallback format (`VK_FORMAT_A1R5G5B5_UNORM_PACK16`).
    /// @param[out] outFallback2 Second fallback format (`VK_FORMAT_B8G8R8A8_UNORM`).
    /// @return Preferred format (`VK_FORMAT_R5G5B5A1_UNORM_PACK16`).
    [[nodiscard]] static constexpr VkFormat
        getPlatform5551FormatWithFallback(VkFormat& outFallback1, VkFormat& outFallback2) noexcept
    {
        outFallback1 = VK_FORMAT_A1R5G5B5_UNORM_PACK16;
        outFallback2 = VK_FORMAT_B8G8R8A8_UNORM;
        return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
    }

    /// @brief Builds the Android-specific Vulkan profile name (inserting `_Android` before any `_RT` suffix).
    /// @param[in] featureLevel Target rendering feature level.
    /// @param[in] raytracing True to append the `_RT` suffix.
    /// @return Android Vulkan profile name (e.g., `"VP_GP_Vulkan_ES3_1_Android_RT"`).
    [[nodiscard]] static gp::String
        getVulkanProfileNameForFeatureLevel(const FeatureLevel featureLevel, const bool raytracing) noexcept
    {
        gp::String profileName = VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(featureLevel, false);
        profileName.append("_Android");
        if (raytracing)
        {
            profileName.append("_RT");
        }
        return profileName;
    }

    /// @brief Restricts required subgroup wave-op stages to fragment and compute on Android GPUs.
    /// @param[in] deviceShaderStageFlags Subgroup stage flags reported by the physical device.
    /// @return `VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT`.
    [[nodiscard]] static constexpr VkShaderStageFlags
        requiredWaveOpsShaderStageFlags(const VkShaderStageFlags deviceShaderStageFlags) noexcept
    {
        static_cast<void>(deviceShaderStageFlags);
        return VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    }

    /// @brief Returns `VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR` matching Android monotonic clock queries.
    /// @return Calibrated timestamp domain for Android.
    [[nodiscard]] static constexpr VkTimeDomainKHR getTimeDomain() noexcept
    {
        return VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR;
    }

    /// @brief Converts an Android `CLOCK_MONOTONIC` nanosecond timestamp into CPU raw performance counter units.
    /// @param[in] timestamp Raw nanosecond timestamp from `vkGetCalibratedTimestampsKHR`.
    /// @return Identical `timestamp` in 1 ns ticks.
    [[nodiscard]] static constexpr gp::UInt64 calibratedTimestampToCycles64(const gp::UInt64 timestamp) noexcept
    {
        return timestamp;
    }
};

}   // namespace gp::rhi::vulkan
