// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"            // IWYU pragma: keep
#include "platforms/base/system/FileEnums.hpp"    // IWYU pragma: export
#include "platforms/base/system/FileHandle.hpp"   // IWYU pragma: export
#include "platforms/base/system/FileTypes.hpp"    // IWYU pragma: export

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/WindowsFileSystem.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/LinuxFileSystem.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/MacOSFileSystem.hpp"
#elif GP_PLATFORM_APPLE
    #include "platforms/apple/system/AppleFileSystem.hpp"
#elif GP_PLATFORM_UNIX
    #include "platforms/unix/system/UnixFileSystem.hpp"
#else
    #include "platforms/generic/system/GenericFileSystem.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using FileSystem = gp::platform::windows::FileSystem;
#elif GP_PLATFORM_LINUX
using FileSystem = gp::platform::linux::FileSystem;
#elif GP_PLATFORM_MACOS
using FileSystem = gp::platform::macos::FileSystem;
#elif GP_PLATFORM_APPLE
using FileSystem = gp::platform::apple::FileSystem;
#elif GP_PLATFORM_UNIX
using FileSystem = gp::platform::unix::FileSystem;
#else
using FileSystem = gp::platform::generic::FileSystem;
#endif

}   // namespace gp::platform
