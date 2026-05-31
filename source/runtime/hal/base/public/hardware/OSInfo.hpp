// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Information about the operating system.
struct OSInfo
{
    /// @brief The name of the operating system.
    /// @example "Windows 11", "Ubuntu Linux", "macOS Ventura"
    char osName[64]{ 0 };

    /// @brief The major version number of the operating system.
    UInt32 majorVersion{ 0 };

    /// @brief The minor version number of the operating system.
    UInt32 minorVersion{ 0 };

    /// @brief The build number of the operating system.
    UInt32 buildNumber{ 0 };

    /// @brief Whether the operating system is 64-bit.
    bool is64Bit{ true };
};

}   // namespace gp::hal
