// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

// Define export/import macros for the HAL module
#define GP_HAL_API GP_API

// HAL-specific build options
#ifndef GP_HAL_SDL3_SUPPORT
    #define GP_HAL_SDL3_SUPPORT 1
#endif

// Enable high-precision input polling
#ifndef GP_HAL_HIGH_PRECISION_INPUT
    #define GP_HAL_HIGH_PRECISION_INPUT 1
#endif

// Maximum number of simultaneous windows
#ifndef GP_HAL_MAX_WINDOWS
    #define GP_HAL_MAX_WINDOWS 16
#endif

// Maximum number of gamepads
#ifndef GP_HAL_MAX_GAMEPADS
    #define GP_HAL_MAX_GAMEPADS 8
#endif

// Event queue size (power of 2 for efficiency)
#ifndef GP_HAL_EVENT_QUEUE_SIZE
    #define GP_HAL_EVENT_QUEUE_SIZE 1024
#endif

namespace GP::Build
{

namespace HAL
{

inline constexpr bool HasSDL3 = GP_HAL_SDL3_SUPPORT;
inline constexpr bool HighPrecisionInput = GP_HAL_HIGH_PRECISION_INPUT;
inline constexpr int MaxWindows = GP_HAL_MAX_WINDOWS;
inline constexpr int MaxGamepads = GP_HAL_MAX_GAMEPADS;
inline constexpr int EventQueueSize = GP_HAL_EVENT_QUEUE_SIZE;

}   // namespace HAL

}   // namespace GP::Build
