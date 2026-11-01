// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/unix/system/UnixPaths.hpp"

namespace gp::platform::apple
{

/// @brief Shared Apple (Darwin / macOS / iOS) implementation of OS directory discovery and lexical path utilities
///        (ADR-0043).
/// @details Resolves the running binary via dyld `_NSGetExecutablePath` + `realpath` and maps application directories
///          to Apple `~/Library/Application Support/`, `~/Library/Caches/`, `~/Library/Preferences/`, and
///          `~/Documents/` (while honoring explicit `$XDG_*` environment overrides when set for developer tooling).
class GP_CORE_API Paths : public gp::platform::unix::Paths
{
public:
    Paths() = delete;

    using gp::platform::unix::Paths::convertRelativePathToFull;
    using gp::platform::unix::Paths::resolve;

    /// @brief Queries the full normalized UTF-8 path to the running Apple executable via `_NSGetExecutablePath`.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable path cache.
    [[nodiscard]] static gp::StringView executablePath() noexcept;

    /// @brief Queries the filename of the running Apple executable.
    /// @param[in] removeExtension If true (default), strips any file extension.
    /// @return Non-owning `StringView` of the executable filename.
    [[nodiscard]] static gp::StringView executableName(const bool removeExtension = true) noexcept;

    /// @brief Queries the directory containing the running Apple executable, guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable directory cache.
    [[nodiscard]] static gp::StringView executableDir() noexcept;

    /// @brief Queries the engine base directory (defaults to `executableDir()`, overridable via `-basedir=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` base directory cache (guaranteed trailing `'/'`).
    [[nodiscard]] static gp::StringView baseDir() noexcept;

    /// @brief Queries the Apple application support directory (`$XDG_DATA_HOME` if set, else
    ///        `$HOME/Library/Application Support/`).
    /// @return Non-owning `StringView` pointing to the static `.bss` AppData directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView appDataDir() noexcept;

    /// @brief Queries the Apple caches directory (`$XDG_CACHE_HOME` if set, else `$HOME/Library/Caches/`).
    /// @return Non-owning `StringView` pointing to the static `.bss` LocalAppData directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView localAppDataDir() noexcept;

    /// @brief Queries the Apple preferences directory (`$XDG_CONFIG_HOME` if set, else `$HOME/Library/Preferences/`).
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache (guaranteed
    ///         trailing `'/'`).
    [[nodiscard]] static gp::StringView userConfigDir() noexcept;

    /// @brief Alias for `userConfigDir()` matching Unreal Engine `FMacPlatformProcess::UserSettingsDir()`.
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache.
    [[nodiscard]] static gp::StringView userSettingsDir() noexcept;

    /// @brief Resolves `path` against `apple::Paths::baseDir()` and collapses all `"."` and `".."` segments.
    /// @param[in] path Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer resolve(const gp::StringView path) noexcept;

    /// @brief Unreal Engine `FPaths::ConvertRelativePathToFull(InPath)` compatibility overload using
    ///        `apple::Paths::baseDir()`.
    /// @param[in] inPath Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer convertRelativePathToFull(const gp::StringView inPath) noexcept;
};

}   // namespace gp::platform::apple
