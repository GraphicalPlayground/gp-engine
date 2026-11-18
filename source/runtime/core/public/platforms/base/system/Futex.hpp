// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/WindowsFutex.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/LinuxFutex.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/MacOSFutex.hpp"
#elif GP_PLATFORM_APPLE
    #include "platforms/apple/system/AppleFutex.hpp"
#elif GP_PLATFORM_UNIX
    #include "platforms/unix/system/UnixFutex.hpp"
#else
    #include "platforms/generic/system/GenericFutex.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using Futex = gp::platform::windows::Futex;
#elif GP_PLATFORM_LINUX
using Futex = gp::platform::linux::Futex;
#elif GP_PLATFORM_MACOS
using Futex = gp::platform::macos::Futex;
#elif GP_PLATFORM_APPLE
using Futex = gp::platform::apple::Futex;
#elif GP_PLATFORM_UNIX
using Futex = gp::platform::unix::Futex;
#else
using Futex = gp::platform::generic::Futex;
#endif

}   // namespace gp::platform
