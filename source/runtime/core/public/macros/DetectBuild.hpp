// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

// C++ standard detection
#if defined(_MSVC_LANG)
    #define GP_CXX_STANDARD _MSVC_LANG
#elif defined(__cplusplus)
    #define GP_CXX_STANDARD __cplusplus
#else
    #error "[GP] Cannot determine C++ standard version"
#endif

#define GP_INTERNAL_CXX11 (GP_CXX_STANDARD >= 201103L)
#define GP_INTERNAL_CXX14 (GP_CXX_STANDARD >= 201402L)
#define GP_INTERNAL_CXX17 (GP_CXX_STANDARD >= 201703L)
#define GP_INTERNAL_CXX20 (GP_CXX_STANDARD >= 202002L)
#define GP_INTERNAL_CXX23 (GP_CXX_STANDARD >= 202302L)
#define GP_INTERNAL_CXX26 (GP_CXX_STANDARD >= 202602L)

#ifndef GP_ALLOW_OLDER_STANDARDS
    #if !GP_INTERNAL_CXX23
        #error "[GP] C++23 is required."
    #endif
#else
    #pragma message("[GP] Warning: Building with an older C++ standard than C++23 is not officially supported.")
#endif

// Build configuration
#if defined(DEBUG) || defined(_DEBUG)
    #define GP_BUILD_DEBUG 1
    #define GP_BUILD_RELEASE 0
#else
    #define GP_BUILD_DEBUG 0
    #define GP_BUILD_RELEASE 1
#endif
