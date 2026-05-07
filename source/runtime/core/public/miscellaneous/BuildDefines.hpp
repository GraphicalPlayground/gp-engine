// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"

/// @section Defines for build configurations.

#ifndef GP_BUILD_DEBUG
    #define GP_BUILD_DEBUG                  GP_FALSE
#endif
#ifndef GP_BUILD_DEVELOPMENT
    #define GP_BUILD_DEVELOPMENT            GP_FALSE
#endif
#ifndef GP_BUILD_TEST
    #define GP_BUILD_TEST                   GP_FALSE
#endif
#ifndef GP_BUILD_SHIPPING
    #define GP_BUILD_SHIPPING               GP_FALSE
#endif

#if (GP_BUILD_DEBUG + GP_BUILD_DEVELOPMENT + GP_BUILD_TEST + GP_BUILD_SHIPPING != 1)
    #error "Exactly one build configuration must be defined as true."
#endif

/// @section Mandatory bridge options definitions (Provided by GPBT).

/// @brief Indicates whether the build includes the editor module.
#ifndef GP_WITH_EDITOR
    #define GP_WITH_EDITOR                  GP_FALSE
#endif

/// @brief Indicates whether the build includes the engine module.
#ifndef GP_WITH_ENGINE
    #define GP_WITH_ENGINE                  GP_FALSE
#endif

/// @brief Indicates whether the build includes the plugin support.
#ifndef GP_WITH_PLUGIN_SUPPORT
    #define GP_WITH_PLUGIN_SUPPORT          GP_FALSE
#endif

/// @brief Indicates whether the build is a monolithic build (no DLLs) or a modular build (DLLs).
#ifndef GP_IS_MONOLITHIC
    #define GP_IS_MONOLITHIC                GP_FALSE
#endif

/// @brief Indicates whether hot reload is supported in this build configuration.
#ifndef GP_HAS_HOT_RELOAD
    #define GP_HAS_HOT_RELOAD               (!GP_IS_MONOLITHIC && !GP_BUILD_SHIPPING && !GP_BUILD_TEST)
#endif

/// @section Optional bridge options definitions (Not necessarily provided by GPBT).

/// @brief Indicates whether to use the null RHI.
#ifndef GP_USE_NULL_RHI
    #define GP_USE_NULL_RHI                 GP_FALSE
#endif

/// @brief Indicates whether to log to the console in shipping builds.
#ifndef GP_USE_LOGGING_IN_SHIPPING
    #define GP_USE_LOGGING_IN_SHIPPING      GP_FALSE
#endif

/// @brief Indicates whether to include checks in shipping builds.
#ifndef GP_USE_CHECKS_IN_SHIPPING
    #define GP_USE_CHECKS_IN_SHIPPING       GP_FALSE
#endif

/// @brief Indicates whether to include ensures in shipping builds.
/// @note If not defined, it defaults to the value of GP_USE_CHECKS_IN_SHIPPING, as ensures are a type of check.
#ifndef GP_USE_ENSURES_IN_SHIPPING
    #define GP_USE_ENSURES_IN_SHIPPING      GP_USE_CHECKS_IN_SHIPPING
#endif

/// @brief Indicates whether to allow console access in shipping builds.
#ifndef GP_ALLOW_CONSOLE_IN_SHIPPING
    #define GP_ALLOW_CONSOLE_IN_SHIPPING    GP_FALSE
#endif

/// @brief Indicates whether to force the use of stats in shipping builds.
#ifndef GP_FORCE_USE_STATS
    #define GP_FORCE_USE_STATS              GP_FALSE
#endif

/// @brief Indicates whether to force the use of the ANSI allocator instead of redirecting to memory subsystem.
#ifndef GP_FORCE_ANSI_ALLOCATOR
    #define GP_FORCE_ANSI_ALLOCATOR        GP_FALSE
#endif

/// @section Baisc options that by default depend on the build configuration.

#if GP_BUILD_DEBUG
    #ifndef GP_DO_GUARD_SLOW
        #define GP_DO_GUARD_SLOW            GP_TRUE
    #endif
    #ifndef GP_DO_CHECK
        #define GP_DO_CHECK                 GP_TRUE
    #endif
    #ifndef GP_DO_ENSURE
        #define GP_DO_ENSURE                GP_TRUE
    #endif
    #ifndef GP_ALLOW_DEBUG_FILES
        #define GP_ALLOW_DEBUG_FILES        GP_TRUE
    #endif
    #ifndef GP_ALLOW_CONSOLE
        #define GP_ALLOW_CONSOLE            GP_TRUE
    #endif
    #ifndef GP_NO_LOGGING
        #define GP_NO_LOGGING               GP_FALSE
    #endif
#elif GP_BUILD_DEVELOPMENT
    #ifndef GP_DO_GUARD_SLOW
        #define GP_DO_GUARD_SLOW            GP_FALSE
    #endif
    #ifndef GP_DO_CHECK
        #define GP_DO_CHECK                 GP_TRUE
    #endif
    #ifndef GP_DO_ENSURE
        #define GP_DO_ENSURE                GP_TRUE
    #endif
    #ifndef GP_ALLOW_DEBUG_FILES
        #define GP_ALLOW_DEBUG_FILES        GP_TRUE
    #endif
    #ifndef GP_ALLOW_CONSOLE
        #define GP_ALLOW_CONSOLE            GP_TRUE
    #endif
    #ifndef GP_NO_LOGGING
        #define GP_NO_LOGGING               GP_FALSE
    #endif
#elif GP_BUILD_TEST
    #ifndef GP_DO_GUARD_SLOW
        #define GP_DO_GUARD_SLOW            GP_FALSE
    #endif
    #ifndef GP_DO_CHECK
        #define GP_DO_CHECK                 GP_USE_CHECKS_IN_SHIPPING
    #endif
    #ifndef GP_DO_ENSURE
        #define GP_DO_ENSURE                GP_USE_ENSURES_IN_SHIPPING
    #endif
    #ifndef GP_ALLOW_DEBUG_FILES
        #define GP_ALLOW_DEBUG_FILES        GP_TRUE
    #endif
    #ifndef GP_ALLOW_CONSOLE
        #define GP_ALLOW_CONSOLE            GP_TRUE
    #endif
    #ifndef GP_NO_LOGGING
        #define GP_NO_LOGGING               !GP_USE_LOGGING_IN_SHIPPING
    #endif
#elif GP_BUILD_SHIPPING
    #ifndef GP_DO_GUARD_SLOW
        #define GP_DO_GUARD_SLOW            GP_FALSE
    #endif
    #ifndef GP_DO_CHECK
        #define GP_DO_CHECK                 GP_USE_CHECKS_IN_SHIPPING
    #endif
    #ifndef GP_DO_ENSURE
        #define GP_DO_ENSURE                GP_USE_ENSURES_IN_SHIPPING
    #endif
    #ifndef GP_ALLOW_DEBUG_FILES
        #define GP_ALLOW_DEBUG_FILES        GP_WITH_EDITOR
    #endif
    #ifndef GP_ALLOW_CONSOLE
        #define GP_ALLOW_CONSOLE            GP_ALLOW_CONSOLE_IN_SHIPPING
    #endif
    #ifndef GP_NO_LOGGING
        #define GP_NO_LOGGING               !GP_USE_LOGGING_IN_SHIPPING
    #endif
#else
    #error "Exactly one build configuration must be defined as true."
#endif
