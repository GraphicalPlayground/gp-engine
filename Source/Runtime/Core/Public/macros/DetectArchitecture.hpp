// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

// Architecture detection
#if defined(_M_X64) || defined(__x86_64__)
    #define GP_ARCHITECTURE_X64 1
#elif defined(_M_IX86) || defined(__i386__)
    #define GP_ARCHITECTURE_X86 1
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(_M_ARM64EC)
    #define GP_ARCHITECTURE_ARM64 1
#elif defined(_M_ARM) || defined(__arm__) || defined(__ARM_NEON)
    #define GP_ARCHITECTURE_ARM32 1
#elif defined(__EMSCRIPTEN__)
    #define GP_ARCHITECTURE_WASM 1
#else
    #error "[GP] Unsupported architecture."
#endif

#ifndef GP_ARCHITECTURE_X86
    #define GP_ARCHITECTURE_X86 0
#endif
#ifndef GP_ARCHITECTURE_X64
    #define GP_ARCHITECTURE_X64 0
#endif
#ifndef GP_ARCHITECTURE_ARM32
    #define GP_ARCHITECTURE_ARM32 0
#endif
#ifndef GP_ARCHITECTURE_ARM64
    #define GP_ARCHITECTURE_ARM64 0
#endif
#ifndef GP_ARCHITECTURE_WASM
    #define GP_ARCHITECTURE_WASM 0
#endif

// Architecture families
#define GP_ARCHITECTURE_X86_FAMILY (GP_ARCHITECTURE_X86 || GP_ARCHITECTURE_X64)
#define GP_ARCHITECTURE_ARM_FAMILY (GP_ARCHITECTURE_ARM32 || GP_ARCHITECTURE_ARM64)
#define GP_ARCHITECTURE_64BIT (GP_ARCHITECTURE_X64 || GP_ARCHITECTURE_ARM64)
#define GP_ARCHITECTURE_32BIT (GP_ARCHITECTURE_X86 || GP_ARCHITECTURE_ARM32)
