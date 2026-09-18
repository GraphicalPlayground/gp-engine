// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/SharedLibrary.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/SharedLibrary.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/SharedLibrary.hpp"
#else
    #include "platforms/generic/system/SharedLibrary.hpp"
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
