// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "GenericHAL.hpp"   // IWYU pragma: export

#if GP_IS_MONOLITHIC
    #define GP_HAL_SDL3_API
#else
    #if defined(GP_HAL_SDL3_EXPORTS)
        #define GP_HAL_SDL3_API GP_DLLEXPORT
    #else
        #define GP_HAL_SDL3_API GP_DLLIMPORT
    #endif
#endif
