// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

// Detect the platform and define the corresponding macros.
#if defined(_WIN32) || defined(_WIN64)
    #define GP_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS
        #define GP_PLATFORM_IOS 1
    #elif TARGET_OS_TV
        #define GP_PLATFORM_TVOS 1
    #elif TARGET_OS_WATCH
        #define GP_PLATFORM_WATCHOS 1
    #elif TARGET_OS_MAC
        #define GP_PLATFORM_MACOS 1
    #else
        #error "[GP] Unsupported Apple platform."
    #endif
#elif defined(__ANDROID__)
    #define GP_PLATFORM_ANDROID 1
#elif defined(__linux__)
    #define GP_PLATFORM_LINUX 1
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
#ifndef GP_PLATFORM_TVOS
    #define GP_PLATFORM_TVOS 0
#endif
#ifndef GP_PLATFORM_WATCHOS
    #define GP_PLATFORM_WATCHOS 0
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
#define GP_PLATFORM_DESKTOP   (GP_PLATFORM_WINDOWS || GP_PLATFORM_LINUX || GP_PLATFORM_MACOS)
#define GP_PLATFORM_MOBILE    (GP_PLATFORM_IOS || GP_PLATFORM_ANDROID || GP_PLATFORM_TVOS || GP_PLATFORM_WATCHOS)
#define GP_PLATFORM_APPLE     (GP_PLATFORM_MACOS || GP_PLATFORM_IOS || GP_PLATFORM_TVOS || GP_PLATFORM_WATCHOS)
#define GP_PLATFORM_UNIX      (GP_PLATFORM_LINUX || GP_PLATFORM_APPLE || GP_PLATFORM_ANDROID)
#define GP_PLATFORM_POSIX     (GP_PLATFORM_UNIX || GP_PLATFORM_WEB)
#define GP_PLATFORM_MICROSOFT (GP_PLATFORM_WINDOWS)

// ANSI Relevant macros
#if GP_PLATFORM_ANDROID
    #define GP_PLATFORM_USE_ANSI_POSIX_MALLOC 1
#elif GP_PLATFORM_APPLE
    #define GP_PLATFORM_USE_ANSI_POSIX_MALLOC 1
    #define GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE 1
#elif GP_PLATFORM_UNIX
    #define GP_PLATFORM_USE_ANSI_POSIX_MALLOC 1
    #define GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE 1
#elif GP_PLATFORM_WINDOWS
    #define GP_PLATFORM_USE_ALIGNED_MALLOC 1
    #define GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE 1
#endif

// Default values for ANSI malloc macros if not defined by platform detection.
#ifndef GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    #define GP_PLATFORM_USE_ANSI_POSIX_MALLOC  0
#endif
#ifndef GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE
    #define GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE 0
#endif
#ifndef GP_PLATFORM_USE_ALIGNED_MALLOC
    #define GP_PLATFORM_USE_ALIGNED_MALLOC 0
#endif
