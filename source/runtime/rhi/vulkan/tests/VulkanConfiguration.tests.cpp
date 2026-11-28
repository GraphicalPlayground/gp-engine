// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "VulkanConfiguration.hpp"
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"
#include <gtest/gtest.h>

// Verify platform macro hygiene: `HidePlatformTypes.hpp` must wrap `<vulkan/vulkan.h>` inside `VulkanThirdParty.hpp`.
#if GP_PLATFORM_WINDOWS
    #if defined(min) || defined(max) || defined(GetObject) || defined(CreateDirectory) || defined(DeleteFile) || \
        defined(MemoryBarrier) || defined(SendMessage)
        #error "Win32 macros leaked past VulkanThirdParty.hpp HidePlatformTypes bracket."
    #endif
#elif GP_PLATFORM_LINUX
    #if defined(None) || defined(Bool) || defined(Success) || defined(Always) || defined(Status) || defined(CurrentTime)
        #error "X11 macros leaked past VulkanThirdParty.hpp HidePlatformTypes bracket."
    #endif
#endif

namespace gp::rhi::vulkan::tests
{

TEST(VulkanConfigurationTest, DynamicLoaderPrototypesAndPlatformWSIDefines)
{
    EXPECT_EQ(GP_VULKAN_DYNAMICALLY_LOADED, 1);

#if defined(VK_NO_PROTOTYPES)
    constexpr bool kNoPrototypesDefined = true;
#else
    constexpr bool kNoPrototypesDefined = false;
#endif
    EXPECT_TRUE(kNoPrototypesDefined);

#if GP_PLATFORM_WINDOWS
    #if defined(VK_USE_PLATFORM_WIN32_KHR)
    constexpr bool kHasWin32WSI = true;
    #else
    constexpr bool kHasWin32WSI = false;
    #endif
    EXPECT_TRUE(kHasWin32WSI);

    const VkWin32SurfaceCreateInfoKHR win32SurfaceInfo =
        zeroVulkanStruct<VkWin32SurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR);
    EXPECT_EQ(win32SurfaceInfo.sType, VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR);
    EXPECT_EQ(win32SurfaceInfo.hinstance, nullptr);
    EXPECT_EQ(win32SurfaceInfo.hwnd, nullptr);
#elif GP_PLATFORM_LINUX
    #if defined(VK_USE_PLATFORM_XLIB_KHR)
    const VkXlibSurfaceCreateInfoKHR xlibSurfaceInfo =
        zeroVulkanStruct<VkXlibSurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR);
    EXPECT_EQ(xlibSurfaceInfo.sType, VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR);
    #endif
    #if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    const VkWaylandSurfaceCreateInfoKHR waylandSurfaceInfo =
        zeroVulkanStruct<VkWaylandSurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR);
    EXPECT_EQ(waylandSurfaceInfo.sType, VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR);
    #endif
#elif GP_PLATFORM_MACOS
    #if defined(VK_USE_PLATFORM_METAL_EXT)
    constexpr bool kHasMetalWSI = true;
    #else
    constexpr bool kHasMetalWSI = false;
    #endif
    EXPECT_TRUE(kHasMetalWSI);
#elif defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
    #if defined(VK_USE_PLATFORM_ANDROID_KHR)
    constexpr bool kHasAndroidWSI = true;
    #else
    constexpr bool kHasAndroidWSI = false;
    #endif
    EXPECT_TRUE(kHasAndroidWSI);
#endif

    // Verify Vulkan SDK 1.2.141+ compatibility range constants match exact enum span formulas.
    EXPECT_EQ(VK_DESCRIPTOR_TYPE_BEGIN_RANGE, VK_DESCRIPTOR_TYPE_SAMPLER);
    EXPECT_EQ(VK_DESCRIPTOR_TYPE_END_RANGE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    EXPECT_EQ(VK_DESCRIPTOR_TYPE_RANGE_SIZE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1);
    EXPECT_EQ(VK_IMAGE_VIEW_TYPE_RANGE_SIZE, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY - VK_IMAGE_VIEW_TYPE_1D + 1);
    EXPECT_EQ(VK_FORMAT_RANGE_SIZE, VK_FORMAT_ASTC_12x12_SRGB_BLOCK - VK_FORMAT_UNDEFINED + 1);
}

TEST(VulkanConfigurationTest, ApiVersionValidationAndFeatureDefaults)
{
    EXPECT_EQ(kVulkanApiVersion, VK_API_VERSION_1_1);
    EXPECT_EQ(VK_API_VERSION_MAJOR(kVulkanApiVersion), 1u);
    EXPECT_EQ(VK_API_VERSION_MINOR(kVulkanApiVersion), 1u);

    EXPECT_EQ(static_cast<gp::Int32>(VulkanValidationLevel::Disabled), 0);
    EXPECT_EQ(static_cast<gp::Int32>(VulkanValidationLevel::Errors), 1);
    EXPECT_EQ(static_cast<gp::Int32>(VulkanValidationLevel::ErrorsAndWarnings), 2);
    EXPECT_EQ(static_cast<gp::Int32>(VulkanValidationLevel::ErrorsWarningsAndPerformance), 3);
    EXPECT_EQ(static_cast<gp::Int32>(VulkanValidationLevel::ErrorsWarningsPerfAndInfo), 4);
    EXPECT_EQ(static_cast<gp::Int32>(VulkanValidationLevel::All), 5);

#if GP_BUILD_DEBUG
    EXPECT_EQ(kDefaultVulkanValidationLevel, VulkanValidationLevel::ErrorsAndWarnings);
#else
    EXPECT_EQ(kDefaultVulkanValidationLevel, VulkanValidationLevel::Disabled);
#endif

    EXPECT_EQ(kVulkanHasDebuggingEnabled, GP_VULKAN_HAS_DEBUGGING_ENABLED != 0);
    EXPECT_EQ(kVulkanEnableDrawMarkers, GP_VULKAN_ENABLE_DRAW_MARKERS != 0);
    EXPECT_EQ(kVulkanUseDebugNames, kVulkanEnableDrawMarkers);
    EXPECT_TRUE(kVulkanReuseFences);
    EXPECT_FALSE(kVulkanCustomMemoryManagerEnabled);

    // Core configuration defaults.
    EXPECT_EQ(GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID, 1);
    EXPECT_EQ(GP_VULKAN_SINGLE_ALLOCATION_PER_RESOURCE, 0);
    EXPECT_EQ(GP_VULKAN_ENABLE_AGGRESSIVE_STATS, 0);
    EXPECT_EQ(GP_VULKAN_QUERY_CALLSTACK, 0);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_VALIDATION_CACHE, 1);
    EXPECT_EQ(GP_VULKAN_HAS_VALIDATION_FEATURES, 1);
    EXPECT_EQ(GP_VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS, 1);
    EXPECT_EQ(GP_VULKAN_DELETE_STALE_CMDBUFFERS, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_MEMORY_BUDGET, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_MEMORY_PRIORITY, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_QCOM_RENDERPASS_SHADER_RESOLVE, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_ASTC_DECODE_MODE, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_DESCRIPTOR_INDEXING, 1);
    EXPECT_EQ(GP_VULKAN_OBJECT_TRACKING, 0);
    EXPECT_EQ(GP_VULKAN_PURGE_SHADER_MODULES, 0);

    // Enum / bitflag fallback formatting helpers.
    EXPECT_EQ(GP_VK_TYPE_TO_STRING(VkFormat, VK_FORMAT_UNDEFINED).view(), "0");
    EXPECT_EQ(GP_VK_TYPE_TO_STRING(VkFormat, VK_FORMAT_R8G8B8A8_UNORM).view(), "37");
    EXPECT_EQ(GP_VK_FLAGS_TO_STRING(VkImageUsageFlags, 0x10u).view(), "16");
}

TEST(VulkanConfigurationTest, MemoryAllocatorAndLogCategories)
{
    EXPECT_EQ(getMemoryAllocator(nullptr), nullptr);
    EXPECT_EQ(GP_VULKAN_CPU_ALLOCATOR, nullptr);

    VkAllocationCallbacks customCallbacks{};
    EXPECT_EQ(getMemoryAllocator(&customCallbacks), &customCallbacks);

    // Ensure exported Vulkan log categories are properly linked and named.
    EXPECT_EQ(gp::LogVulkanRHI.getName(), "LogVulkanRHI");
    EXPECT_EQ(gp::LogVulkan.getName(), "LogVulkan");
}

}   // namespace gp::rhi::vulkan::tests
