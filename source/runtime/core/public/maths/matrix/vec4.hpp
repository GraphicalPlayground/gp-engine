// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once
#include "Vec4_Scalar.hpp"

#if !defined(GP_FORCE_SCALAR)
    #if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
        #define GP_SIMD_SSE 1
        #include "Vec4_SSE.hpp"
    #elif defined(__ARM_NEON) || defined(_M_ARM64)
        #define GP_SIMD_NEON 1
        #include "Vec4_NEON.hpp"
    #endif
#endif
