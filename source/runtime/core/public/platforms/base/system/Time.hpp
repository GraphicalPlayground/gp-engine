// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/WindowsTime.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/LinuxTime.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/MacOSTime.hpp"
#elif GP_PLATFORM_APPLE
    #include "platforms/apple/system/AppleTime.hpp"
#elif GP_PLATFORM_UNIX
    #include "platforms/unix/system/UnixTime.hpp"
#else
    #include "platforms/generic/system/GenericTime.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using Time = gp::platform::windows::Time;
#elif GP_PLATFORM_LINUX
using Time = gp::platform::linux::Time;
#elif GP_PLATFORM_MACOS
using Time = gp::platform::macos::Time;
#elif GP_PLATFORM_APPLE
using Time = gp::platform::apple::Time;
#elif GP_PLATFORM_UNIX
using Time = gp::platform::unix::Time;
#else
using Time = gp::platform::generic::Time;
#endif

}   // namespace gp::platform
