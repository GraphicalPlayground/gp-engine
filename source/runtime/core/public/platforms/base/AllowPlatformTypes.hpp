// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

// NOTE: Intentionally omits `#pragma once` so paired Allow/Hide brackets can nest or repeat within a translation unit.

#include "platforms/base/Platform.hpp"   // IWYU pragma: export

#ifdef GP_PLATFORM_ALLOW_PLATFORM_TYPES
    #error "AllowPlatformTypes.hpp already included. Ensure that each Allow/Hide pair is properly nested and balanced."
#endif
#define GP_PLATFORM_ALLOW_PLATFORM_TYPES

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/AllowWindowsPlatformTypes.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/AllowLinuxPlatformTypes.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/apple/AllowApplePlatformTypes.hpp"
#endif
