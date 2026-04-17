// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "macros/DetectArchitecture.hpp"

// SSE (x86/x64)
#if GP_ARCHITECTURE_X86_FAMILY
    #if defined(__SSE__) || defined(_M_IX86_FP) || defined(_M_X64)
        #define GP_SIMD_SSE 1
    #endif
    #if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
        #define GP_SIMD_SSE2 1
    #endif
    #if defined(__SSE3__)
        #define GP_SIMD_SSE3 1
    #endif
    #if defined(__SSSE3__)
        #define GP_SIMD_SSSE3 1
    #endif
    #if defined(__SSE4_1__)
        #define GP_SIMD_SSE4_1 1
    #endif
    #if defined(__SSE4_2__)
        #define GP_SIMD_SSE4_2 1
    #endif
    #if defined(__AVX__)
        #define GP_SIMD_AVX 1
    #endif
    #if defined(__AVX2__)
        #define GP_SIMD_AVX2 1
    #endif
    #if defined(__AVX512F__)
        #define GP_SIMD_AVX512 1
    #endif
    #if defined(__FMA__)
        #define GP_SIMD_FMA 1
    #endif
#endif

// NEON (ARM)
#if GP_ARCHITECTURE_ARM_FAMILY
    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define GP_SIMD_NEON 1
    #endif
    #if defined(__ARM_FEATURE_FMA)
        #define GP_SIMD_ARM_FMA 1
    #endif
#endif

// WASM SIMD
#if GP_ARCHITECTURE_WASM
    #if defined(__wasm_simd128__)
        #define GP_SIMD_WASM128 1
    #endif
#endif

// Default values for undefined SIMD macros
#ifndef GP_SIMD_SSE
    #define GP_SIMD_SSE 0
#endif
#ifndef GP_SIMD_SSE2
    #define GP_SIMD_SSE2 0
#endif
#ifndef GP_SIMD_SSE3
    #define GP_SIMD_SSE3 0
#endif
#ifndef GP_SIMD_SSSE3
    #define GP_SIMD_SSSE3 0
#endif
#ifndef GP_SIMD_SSE4_1
    #define GP_SIMD_SSE4_1 0
#endif
#ifndef GP_SIMD_SSE4_2
    #define GP_SIMD_SSE4_2 0
#endif
#ifndef GP_SIMD_AVX
    #define GP_SIMD_AVX 0
#endif
#ifndef GP_SIMD_AVX2
    #define GP_SIMD_AVX2 0
#endif
#ifndef GP_SIMD_AVX512
    #define GP_SIMD_AVX512 0
#endif
#ifndef GP_SIMD_FMA
    #define GP_SIMD_FMA 0
#endif
#ifndef GP_SIMD_NEON
    #define GP_SIMD_NEON 0
#endif
#ifndef GP_SIMD_ARM_FMA
    #define GP_SIMD_ARM_FMA 0
#endif
#ifndef GP_SIMD_WASM128
    #define GP_SIMD_WASM128 0
#endif

// Generic SIMD availability
#define GP_SIMD_AVAILABLE (GP_SIMD_SSE || GP_SIMD_NEON || GP_SIMD_WASM128)
