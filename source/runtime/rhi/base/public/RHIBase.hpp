// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

#if GP_IS_MONOLITHIC
    #define GP_RHI_API
#else
    #if defined(GP_RHI_BASE_EXPORTS)
        #define GP_RHI_API GP_DLLEXPORT
    #else
        #define GP_RHI_API GP_DLLIMPORT
    #endif
#endif

/// @brief Controls whether `gp::rhi::resource::Resource` tracks owner names, active instances, and `ResourceInfo`.
/// @details Enabled by default in non-shipping builds so memory snapshots and leak diagnostics remain available.
#ifndef GP_RHI_WANT_RESOURCE_INFO
    #define GP_RHI_WANT_RESOURCE_INFO (!GP_BUILD_SHIPPING)
#endif

/// @brief Forces RHI resource tracking and info metadata off regardless of `GP_RHI_WANT_RESOURCE_INFO`.
#ifndef GP_RHI_FORCE_DISABLE_RESOURCE_INFO
    #define GP_RHI_FORCE_DISABLE_RESOURCE_INFO 0
#endif

/// @brief Final compile-time switch governing RHI resource metadata and instance tracking.
#define GP_RHI_ENABLE_RESOURCE_INFO (GP_RHI_WANT_RESOURCE_INFO && !GP_RHI_FORCE_DISABLE_RESOURCE_INFO)

/// @brief Controls whether shader objects retain debug names, uniform buffer names, and reflection validation tables.
#ifndef GP_RHI_INCLUDE_SHADER_DEBUG_DATA
    #define GP_RHI_INCLUDE_SHADER_DEBUG_DATA (!GP_BUILD_SHIPPING)
#endif

/// @brief Conditionally compiles expressions that depend on shader debug metadata.
#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
    #define GP_RHI_IF_SHADER_DEBUG_DATA(...) __VA_ARGS__
#else
    #define GP_RHI_IF_SHADER_DEBUG_DATA(...)
#endif
