// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"
#include "platforms/apple/ApplePlatform.hpp"   // IWYU pragma: export

#if __is_target_arch(arm64) || __is_target_arch(arm64e)
    #define GP_PLATFORM_MAC_ARM64                       GP_TRUE
    #define GP_PLATFORM_MAC_X86                         GP_FALSE
#else
    #define GP_PLATFORM_MAC_ARM64                       GP_FALSE
    #define GP_PLATFORM_MAC_X86                         GP_TRUE
#endif

#define _GP_IS_PLATFORM_64BIT (GP_PLATFORM_MAC_ARM64 || GP_PLATFORM_MAC_X86)

#define GP_PLATFORM_SUPPORTS_TBB                        GP_TRUE
#define GP_PLATFORM_SUPPORTS_MIMALLOC                   _GP_IS_PLATFORM_64BIT
#define GP_PLATFORM_SUPPORTS_MESH_SHADERS               GP_TRUE
#define GP_PLATFORM_SUPPORTS_BINDLESS_RENDERING         GP_TRUE
#define GP_PLATFORM_SUPPORTS_GEOMETRY_SHADERS           GP_TRUE

#ifdef GP_PLATFORM_MAC_ARM64
    #define GP_PLATFORM_CACHE_LINE_SIZE 128
#else
    #define GP_PLATFORM_CACHE_LINE_SIZE 64
#endif

#if GP_BUILD_DEBUG
    #define GP_FORCEINLINE inline
#else
    #define GP_FORCEINLINE inline __attribute__ ((always_inline))
#endif
