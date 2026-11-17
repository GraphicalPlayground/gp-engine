// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

// NOTE: Intentionally omits `#pragma once` so paired Allow/Hide brackets can nest or repeat within a translation unit.

#include "platforms/base/Platform.hpp"   // IWYU pragma: export

#ifndef GP_PLATFORM_ALLOW_PLATFORM_TYPES
    #error "AllowPlatformTypes.hpp must be included before HidePlatformTypes.hpp."
#endif
#undef GP_PLATFORM_ALLOW_PLATFORM_TYPES

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/HideWindowsPlatformTypes.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/HideLinuxPlatformTypes.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/apple/HideApplePlatformTypes.hpp"
#endif
