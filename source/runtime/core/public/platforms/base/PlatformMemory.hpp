// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/WindowsPlatformMemory.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/LinuxPlatformMemory.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/MacOSPlatformMemory.hpp"
#else
    #include "platforms/generic/GenericPlatformMemory.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using Memory = gp::platform::windows::Memory;
#elif GP_PLATFORM_LINUX
using Memory = gp::platform::linux::Memory;
#elif GP_PLATFORM_MACOS
using Memory = gp::platform::macos::Memory;
#else
using Memory = gp::platform::generic::Memory;
#endif

}   // namespace gp::platform
