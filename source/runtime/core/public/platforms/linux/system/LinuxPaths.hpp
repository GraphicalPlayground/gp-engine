// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/unix/system/UnixPaths.hpp"

namespace gp::platform::linux
{

/// @brief Linux implementation of OS directory discovery and lexical path utilities (ADR-0043).
/// @details Inherits XDG Base Directory Specification (`$XDG_DATA_HOME`, `$XDG_CACHE_HOME`, `$XDG_CONFIG_HOME`,
///          `$XDG_DOCUMENTS_DIR`) and `$HOME`/`getpwuid_r` resolution from `gp::platform::unix::Paths`, while
///          implementing Linux kernel procfs executable discovery via `readlink("/proc/self/exe", ...)`.
class GP_CORE_API Paths : public gp::platform::unix::Paths
{
public:
    Paths() = delete;

    using gp::platform::unix::Paths::convertRelativePathToFull;
    using gp::platform::unix::Paths::resolve;

    /// @brief Queries the full normalized UTF-8 path to the running Linux binary via `readlink("/proc/self/exe")`.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable path cache.
    [[nodiscard]] static gp::StringView executablePath() noexcept;

    /// @brief Queries the filename of the running Linux executable.
    /// @param[in] removeExtension If true (default), strips any file extension.
    /// @return Non-owning `StringView` of the executable filename.
    [[nodiscard]] static gp::StringView executableName(const bool removeExtension = true) noexcept;

    /// @brief Queries the directory containing the running Linux executable, guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable directory cache.
    [[nodiscard]] static gp::StringView executableDir() noexcept;

    /// @brief Queries the engine base directory (defaults to `executableDir()`, overridable via `-basedir=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` base directory cache (guaranteed trailing `'/'`).
    [[nodiscard]] static gp::StringView baseDir() noexcept;

    /// @brief Resolves `path` against `linux::Paths::baseDir()` and collapses all `"."` and `".."` segments.
    /// @param[in] path Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer resolve(const gp::StringView path) noexcept;

    /// @brief Unreal Engine `FPaths::ConvertRelativePathToFull(InPath)` compatibility overload using
    ///        `linux::Paths::baseDir()`.
    /// @param[in] inPath Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer convertRelativePathToFull(const gp::StringView inPath) noexcept;
};

}   // namespace gp::platform::linux
