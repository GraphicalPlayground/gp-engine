// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/WindowsSharedLibrary.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/LinuxSharedLibrary.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/MacOSSharedLibrary.hpp"
#else
    #include "platforms/generic/system/GenericSharedLibrary.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using SharedLibrary = gp::platform::windows::SharedLibrary;
#elif GP_PLATFORM_LINUX
using SharedLibrary = gp::platform::linux::SharedLibrary;
#elif GP_PLATFORM_MACOS
using SharedLibrary = gp::platform::macos::SharedLibrary;
#else
using SharedLibrary = gp::platform::generic::SharedLibrary;
#endif

}   // namespace gp::platform
