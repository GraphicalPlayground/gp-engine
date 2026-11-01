// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"           // IWYU pragma: keep
#include "platforms/base/system/PathEnums.hpp"   // IWYU pragma: export
#include "platforms/base/system/PathTypes.hpp"   // IWYU pragma: export

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/WindowsPaths.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/LinuxPaths.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/MacOSPaths.hpp"
#elif GP_PLATFORM_APPLE
    #include "platforms/apple/system/ApplePaths.hpp"
#elif GP_PLATFORM_UNIX
    #include "platforms/unix/system/UnixPaths.hpp"
#else
    #include "platforms/generic/system/GenericPaths.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using Paths = gp::platform::windows::Paths;
#elif GP_PLATFORM_LINUX
using Paths = gp::platform::linux::Paths;
#elif GP_PLATFORM_MACOS
using Paths = gp::platform::macos::Paths;
#elif GP_PLATFORM_APPLE
using Paths = gp::platform::apple::Paths;
#elif GP_PLATFORM_UNIX
using Paths = gp::platform::unix::Paths;
#else
using Paths = gp::platform::generic::Paths;
#endif

}   // namespace gp::platform
