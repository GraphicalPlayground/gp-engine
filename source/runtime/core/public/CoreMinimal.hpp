// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/BuildDefines.hpp"   // IWYU pragma: export
#include "platforms/base/Platform.hpp"      // IWYU pragma: export

#if GP_IS_MONOLITHIC
    #define GP_CORE_API
#else
    #ifdef GP_CORE_API_EXPORTS
        #define GP_CORE_API GP_DLLEXPORT
    #else
        #define GP_CORE_API GP_DLLIMPORT
    #endif
#endif
