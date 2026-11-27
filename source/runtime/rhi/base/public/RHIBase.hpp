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

/// @brief Controls whether hierarchical CPU/GPU RHI breadcrumb scopes and crash markers are compiled in.
#ifndef GP_WITH_RHI_BREADCRUMBS
    #define GP_WITH_RHI_BREADCRUMBS (!GP_BUILD_SHIPPING)
#endif

/// @brief Conditionally compiles expressions that depend on RHI breadcrumb support.
#if GP_WITH_RHI_BREADCRUMBS
    #define GP_RHI_IF_BREADCRUMBS(...) __VA_ARGS__
#else
    #define GP_RHI_IF_BREADCRUMBS(...)
#endif

/// @brief Controls whether the RHI validation wrapper layer and resource barrier tracker are compiled in.
#ifndef GP_ENABLE_RHI_VALIDATION
    #define GP_ENABLE_RHI_VALIDATION (!GP_BUILD_SHIPPING)
#endif

/// @brief Conditionally compiles expressions that depend on RHI validation support.
#if GP_ENABLE_RHI_VALIDATION
    #define GP_RHI_IF_VALIDATION(...) __VA_ARGS__
#else
    #define GP_RHI_IF_VALIDATION(...)
#endif

/// @brief Controls whether GPU draw/dispatch stats and frame profiler streams are compiled in.
#ifndef GP_HAS_GPU_STATS
    #define GP_HAS_GPU_STATS (!GP_BUILD_SHIPPING)
#endif
