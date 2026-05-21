// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"

#define GP_SKIP_PLATFORM_DETECTION (GP_PLATFORM_WINDOWS || GP_PLATFORM_LINUX || GP_PLATFORM_MACOS || GP_PLATFORM_IOS || GP_PLATFORM_ANDROID)

#if !GP_SKIP_PLATFORM_DETECTION
    #if defined(_WIN32) || defined(_WIN64)
        #define GP_PLATFORM_WINDOWS GP_TRUE
    #elif defined(__APPLE__) && defined(__MACH__)
        #include <TargetConditionals.h>
        #if TARGET_OS_MAC
            #define GP_PLATFORM_MACOS GP_TRUE
        #else
            #error "Unsupported Apple platform"
        #endif
    #elif defined(__linux__) && !defined(__ANDROID__)
        #define GP_PLATFORM_LINUX GP_TRUE
    #else
        #error "Unsupported platform"
    #endif
#endif

#define GP_SKIP_ARCHITECTURE_DETECTION (GP_ARCHITECTURE_X86 || GP_ARCHITECTURE_X64 || GP_ARCHITECTURE_ARM32 || GP_ARCHITECTURE_ARM64)

#if !GP_SKIP_ARCHITECTURE_DETECTION
    #if defined(_M_X64) || defined(__x86_64__)
        #define GP_ARCHITECTURE_X64 GP_TRUE
    #elif defined(_M_IX86) || defined(__i386__)
        #define GP_ARCHITECTURE_X86 GP_TRUE
    #elif defined(_M_ARM64) || defined(__aarch64__) || defined(_M_ARM64EC)
        #define GP_ARCHITECTURE_ARM64 GP_TRUE
    #elif defined(_M_ARM) || defined(__arm__) || defined(__ARM_NEON)
        #define GP_ARCHITECTURE_ARM32 GP_TRUE
    #else
        #error "Unsupported architecture"
    #endif
#endif

#define GP_SKIP_COMPILER_DETECTION (GP_COMPILER_MSVC || GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL)

#if !GP_SKIP_COMPILER_DETECTION
    // clang-cl sets both _MSC_VER and __clang__, so MSVC must be checked independently.
    #if defined(_MSC_VER) && !defined(__clang__)
        #define GP_COMPILER_MSVC GP_TRUE
    #endif
    // ICX (Intel's modern compiler) is LLVM/Clang-based and sets __clang__ too.
    // Detect Intel first so GP_COMPILER_CLANG stays false for ICX.
    #if defined(__INTEL_COMPILER) || defined(__ICC)
        #define GP_COMPILER_INTEL GP_TRUE
    #elif defined(__clang__)
        #define GP_COMPILER_CLANG GP_TRUE
    // Clang defines __GNUC__ for ABI compatibility; exclude it from pure-GCC detection.
    #elif defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
        #define GP_COMPILER_GCC GP_TRUE
    #endif
#endif
