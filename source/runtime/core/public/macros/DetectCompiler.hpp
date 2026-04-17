// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

// Compiler detection
#if defined(_MSC_VER)
    #define GP_COMPILER_MSVC 1
    #define GP_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
    #define GP_COMPILER_CLANG 1
    #define GP_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
    #define GP_COMPILER_GCC 1
    #define GP_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(__EMSCRIPTEN__)
    #define GP_COMPILER_EMSCRIPTEN 1
    #define GP_COMPILER_VERSION __EMSCRIPTEN__
#elif defined(__INTEL_COMPILER) || defined(__ICC)
    #define GP_COMPILER_INTEL 1
    #define GP_COMPILER_VERSION __INTEL_COMPILER
#else
    #error "[GP] Unsupported compiler."
#endif

// Define missing compiler macros to 0
#ifndef GP_COMPILER_MSVC
    #define GP_COMPILER_MSVC 0
#endif
#ifndef GP_COMPILER_CLANG
    #define GP_COMPILER_CLANG 0
#endif
#ifndef GP_COMPILER_GCC
    #define GP_COMPILER_GCC 0
#endif
#ifndef GP_COMPILER_EMSCRIPTEN
    #define GP_COMPILER_EMSCRIPTEN 0
#endif
#ifndef GP_COMPILER_INTEL
    #define GP_COMPILER_INTEL 0
#endif
