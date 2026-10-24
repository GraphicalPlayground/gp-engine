// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/WindowsThread.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/LinuxThread.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/MacOSThread.hpp"
#else
    #include "platforms/generic/system/GenericThread.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using Thread = gp::platform::windows::Thread;
#elif GP_PLATFORM_LINUX
using Thread = gp::platform::linux::Thread;
#elif GP_PLATFORM_MACOS
using Thread = gp::platform::macos::Thread;
#else
using Thread = gp::platform::generic::Thread;
#endif

}   // namespace gp::platform
