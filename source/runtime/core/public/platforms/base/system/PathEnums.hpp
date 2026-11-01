// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::platform
{

/// @brief Bitmask flags controlling lexical path inspection and extension extraction behavior (ADR-0043).
enum class PathFlags : gp::UInt8
{
    /// @brief Default behavior: extracts only the final extension without a leading dot (e.g., `"cpp"`).
    None = 0x00,

    /// @brief Includes the leading dot in the extracted extension view (e.g., `".cpp"`).
    IncludeDot = 0x01,

    /// @brief Extracts compound extensions starting from the first dot in the clean filename (e.g., `"tests.cpp"` or
    ///        `".tests.cpp"`).
    AllowCompoundExtension = 0x02
};

/// @brief Explicit diagnostic error codes returned by `Paths::validatePath()` (ADR-0043).
enum class PathValidationError : gp::UInt8
{
    /// @brief The path is lexically valid across all supported host and console filesystems.
    None = 0,

    /// @brief The provided path string view is empty.
    EmptyPath = 1,

    /// @brief The path length exceeds `kMaxPathLength` (`1024` UTF-8 bytes).
    ExceedsMaxLength = 2,

    /// @brief The path contains an illegal filesystem character (`\"*:<>?|`, ASCII control character `< 32`).
    InvalidCharacter = 3,

    /// @brief A path component matches a reserved Windows/DOS device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1`..`COM9`,
    ///        `LPT1`..`LPT9`, `CLOCK$`).
    ReservedDeviceName = 4,

    /// @brief A directory or file component ends with a trailing space or trailing period (illegal on Win32/NTFS).
    TrailingDotOrSpace = 5,

    /// @brief The path begins with a drive letter followed by a colon without a root slash (e.g., `"C:relative/file"`).
    UnrootedDriveSpecifier = 6
};

}   // namespace gp::platform

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::platform::PathFlags);
