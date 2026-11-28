// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

/// @brief Controls whether Vulkan entry points are loaded dynamically at runtime rather than linked statically.
#ifndef GP_VULKAN_DYNAMICALLY_LOADED
    #define GP_VULKAN_DYNAMICALLY_LOADED 1
#endif

#if GP_VULKAN_DYNAMICALLY_LOADED && !defined(VK_NO_PROTOTYPES)
    #define VK_NO_PROTOTYPES
#endif

// Ensure the host platform WSI surface extension macros are enabled before including `<vulkan/vulkan.h>`.
#if GP_PLATFORM_WINDOWS
    #ifndef VK_USE_PLATFORM_WIN32_KHR
        #define VK_USE_PLATFORM_WIN32_KHR 1
    #endif
#elif GP_PLATFORM_LINUX
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
#elif GP_PLATFORM_MACOS || (defined(GP_PLATFORM_IOS) && GP_PLATFORM_IOS)
    #ifndef VK_USE_PLATFORM_METAL_EXT
        #define VK_USE_PLATFORM_METAL_EXT 1
    #endif
#elif defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
    #ifndef VK_USE_PLATFORM_ANDROID_KHR
        #define VK_USE_PLATFORM_ANDROID_KHR 1
    #endif
#endif

// clang-format off
#include "platforms/base/AllowPlatformTypes.hpp"
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    #include <windows.h>
#endif
GP_THIRD_PARTY_INCLUDES_START
#include <vulkan/vulkan.h>
GP_THIRD_PARTY_INCLUDES_END
#include "platforms/base/HidePlatformTypes.hpp"
// clang-format on

#if defined(VK_HEADER_VERSION) && VK_HEADER_VERSION >= 141
    // Restore range constants removed from Vulkan headers starting with SDK 1.2.141.
    #ifndef VK_DESCRIPTOR_TYPE_BEGIN_RANGE
        #define VK_DESCRIPTOR_TYPE_BEGIN_RANGE (VK_DESCRIPTOR_TYPE_SAMPLER)
    #endif
    #ifndef VK_DESCRIPTOR_TYPE_END_RANGE
        #define VK_DESCRIPTOR_TYPE_END_RANGE (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
    #endif
    #ifndef VK_DESCRIPTOR_TYPE_RANGE_SIZE
        #define VK_DESCRIPTOR_TYPE_RANGE_SIZE (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1)
    #endif
    #ifndef VK_IMAGE_VIEW_TYPE_RANGE_SIZE
        #define VK_IMAGE_VIEW_TYPE_RANGE_SIZE (VK_IMAGE_VIEW_TYPE_CUBE_ARRAY - VK_IMAGE_VIEW_TYPE_1D + 1)
    #endif
    #ifndef VK_FORMAT_RANGE_SIZE
        #define VK_FORMAT_RANGE_SIZE (VK_FORMAT_ASTC_12x12_SRGB_BLOCK - VK_FORMAT_UNDEFINED + 1)
    #endif
#endif

#if defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
    #if defined(__has_include)
        #if __has_include(<android/hardware_buffer.h>)
            #include <android/hardware_buffer.h>
        #endif
    #endif
#endif
