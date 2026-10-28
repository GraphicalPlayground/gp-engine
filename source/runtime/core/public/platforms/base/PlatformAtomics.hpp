// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/WindowsPlatformAtomics.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/LinuxPlatformAtomics.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/MacOSPlatformAtomics.hpp"
#else
    #include "platforms/generic/GenericPlatformAtomics.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using Atomics = gp::platform::windows::Atomics;
#elif GP_PLATFORM_LINUX
using Atomics = gp::platform::linux::Atomics;
#elif GP_PLATFORM_MACOS
using Atomics = gp::platform::macos::Atomics;
#else
using Atomics = gp::platform::generic::Atomics;
#endif

}   // namespace gp::platform

