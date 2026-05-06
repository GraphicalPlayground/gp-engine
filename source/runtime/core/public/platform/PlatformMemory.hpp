// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: export
#if GP_PLATFORM_WINDOWS
    #include "platform/windows/WindowsMemoryPlatform.hpp"
#elif GP_PLATFORM_LINUX
    #include "platform/linux/LinuxMemoryPlatform.hpp"
#elif GP_PLATFORM_MACOS || GP_PLATFORM_IOS
    #include "platform/apple/AppleMemoryPlatform.hpp"
#else
    #include "platform/generic/GenericMemoryPlatform.hpp"
#endif

namespace gp::memory
{

#if GP_PLATFORM_WINDOWS
/// @brief Defines the platform-specific memory management implementation for Windows.
using PlatformMemory = gp::memory::impl::WindowsMemoryPlatform;
#elif GP_PLATFORM_LINUX
/// @brief Defines the platform-specific memory management implementation for Linux.
using PlatformMemory = gp::memory::impl::LinuxMemoryPlatform;
#elif GP_PLATFORM_MACOS || GP_PLATFORM_IOS
/// @brief Defines the platform-specific memory management implementation for macOS.
using PlatformMemory = gp::memory::impl::AppleMemoryPlatform;
#else
/// @brief Defines the generic memory management implementation.
using PlatformMemory = gp::memory::impl::GenericMemoryPlatform;
#endif

}   // namespace gp::memory
