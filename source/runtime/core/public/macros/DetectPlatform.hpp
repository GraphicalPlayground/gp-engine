// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

// Detect the platform and define the corresponding macros.
#if defined(_WIN32) || defined(_WIN64)
    #define GP_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if defined(TARGET_IPHONE_SIMULATOR) || defined(TARGET_OS_IPHONE)
        #define GP_PLATFORM_IOS 1
    #else
        #define GP_PLATFORM_MACOS 1
    #endif
#elif defined(__linux__)
    #define GP_PLATFORM_LINUX 1
#elif defined(__ANDROID__)
    #define GP_PLATFORM_ANDROID 1
#elif defined(__EMSCRIPTEN__)
    #define GP_PLATFORM_WEB 1
#else
    #error "[GP] Unsupported platform."
#endif

// Define all platform macros to 0 if they are not defined.
#ifndef GP_PLATFORM_WINDOWS
    #define GP_PLATFORM_WINDOWS 0
#endif
#ifndef GP_PLATFORM_LINUX
    #define GP_PLATFORM_LINUX 0
#endif
#ifndef GP_PLATFORM_MACOS
    #define GP_PLATFORM_MACOS 0
#endif
#ifndef GP_PLATFORM_IOS
    #define GP_PLATFORM_IOS 0
#endif
#ifndef GP_PLATFORM_ANDROID
    #define GP_PLATFORM_ANDROID 0
#endif
#ifndef GP_PLATFORM_WEB
    #define GP_PLATFORM_WEB 0
#endif

// Windows-specific macros
#if GP_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

// Platform families
#define GP_PLATFORM_DESKTOP (GP_PLATFORM_WINDOWS || GP_PLATFORM_LINUX || GP_PLATFORM_MACOS)
#define GP_PLATFORM_MOBILE (GP_PLATFORM_IOS || GP_PLATFORM_ANDROID)
#define GP_PLATFORM_APPLE (GP_PLATFORM_MACOS || GP_PLATFORM_IOS)
#define GP_PLATFORM_UNIX (GP_PLATFORM_LINUX || GP_PLATFORM_MACOS || GP_PLATFORM_IOS || GP_PLATFORM_ANDROID)
