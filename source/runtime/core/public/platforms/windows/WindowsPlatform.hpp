// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"

// Include the <sal.h> header for Windows-specific annotations and macros.
// Guarded so that cross-compilation toolchains (e.g. clang on Linux targeting Win32)
// do not error when the Windows SDK is not present in the sysroot.
#if defined(_WIN32) || defined(_WIN64)
    #include <sal.h>
#endif

#if defined(_WIN64)
    #define _GP_IS_PLATFORM_64BIT GP_TRUE
#else
    #define _GP_IS_PLATFORM_64BIT GP_FALSE
#endif

#define GP_PLATFORM_SUPPORTS_TBB                        GP_TRUE
#define GP_PLATFORM_SUPPORTS_MIMALLOC                   _GP_IS_PLATFORM_64BIT
#define GP_PLATFORM_SUPPORTS_NAMED_PIPES                GP_TRUE
#define GP_PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING      GP_TRUE
#define GP_PLATFORM_SUPPORTS_MESH_SHADERS               GP_TRUE
#define GP_PLATFORM_SUPPORTS_WORKGRAPH_SHADERS          GP_TRUE
#define GP_PLATFORM_SUPPORTS_BINDLESS_RENDERING         GP_TRUE
#define GP_PLATFORM_USE_ALIGNED_MALLOC                  GP_TRUE
#define GP_PLATFORM_SUPPORTS_BORDERLESS_WINDOW          GP_TRUE

#define GP_PLATFORM_BREAK() (__nop(), __debugbreak())

#define GP_PLATFORM_HAS_128BIT_ATOMICS                  (_GP_IS_PLATFORM_64BIT && (WINVER >= 0x602))

#ifdef CDECL
    #undef CDECL
#endif

#define GP_VARARGS       __cdecl
#define GP_CDECL         __cdecl
#define GP_STDCALL       __stdcall
#define GP_FORCEINLINE   __forceinline
#define GP_FORCENOINLINE __declspec(noinline)

#ifdef _MSC_VER
    // C4481: nonstandard extension used: override specifier 'override' (pre-C++11 MSVC warning).
    // Suppressed globally because the engine uses 'override' throughout.
    #pragma warning(disable : 4481)
#endif

#define GP_ABSTRACT abstract

#define GP_PLATFORM_CACHE_LINE_SIZE 64

#define GP_DLLEXPORT __declspec(dllexport)
#define GP_DLLIMPORT __declspec(dllimport)
