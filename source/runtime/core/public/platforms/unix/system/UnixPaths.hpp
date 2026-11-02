// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/generic/system/GenericPaths.hpp"

namespace gp::platform::unix
{

/// @brief Shared POSIX and XDG Base Directory Specification implementation for Unix-derived operating systems
///        (ADR-0043).
/// @details Resolves user directories using `$HOME` (with thread-safe `getpwuid_r(geteuid(), ...)` fallback),
///          `$XDG_DATA_HOME` (`~/.local/share/`), `$XDG_CACHE_HOME` (`~/.cache/`), `$XDG_CONFIG_HOME` (`~/.config/`),
///          `$XDG_DOCUMENTS_DIR` (`~/Documents/`), and `$TMPDIR` (`/var/tmp/`) into lazy static `.bss` caches.
class GP_CORE_API Paths : public gp::platform::generic::Paths
{
public:
    Paths() = delete;

    using gp::platform::generic::Paths::convertRelativePathToFull;
    using gp::platform::generic::Paths::resolve;

    /// @brief Queries the current POSIX working directory via `getcwd`, normalized with a trailing `'/'`.
    /// @return Stack-allocated `PathBuffer` containing the current working directory.
    [[nodiscard]] static PathBuffer workingDir() noexcept;

    /// @brief Queries the initial working directory captured on first invocation during engine startup.
    /// @return Non-owning `StringView` pointing to the static `.bss` launch directory cache.
    [[nodiscard]] static gp::StringView launchDir() noexcept;

    /// @brief Queries the user home directory via `$HOME` or `getpwuid_r(geteuid(), ...)`.
    /// @return Non-owning `StringView` pointing to the static `.bss` home directory cache (guaranteed trailing `'/'`).
    [[nodiscard]] static gp::StringView homeDir() noexcept;

    /// @brief Queries the XDG user data directory via `$XDG_DATA_HOME` or `$HOME/.local/share/` (overridable via
    ///        `-appdata=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` AppData directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView appDataDir() noexcept;

    /// @brief Queries the XDG user cache directory via `$XDG_CACHE_HOME` or `$HOME/.cache/`.
    /// @return Non-owning `StringView` pointing to the static `.bss` LocalAppData directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView localAppDataDir() noexcept;

    /// @brief Queries the XDG user configuration directory via `$XDG_CONFIG_HOME` or `$HOME/.config/`.
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache (guaranteed
    ///         trailing `'/'`).
    [[nodiscard]] static gp::StringView userConfigDir() noexcept;

    /// @brief Alias for `userConfigDir()` matching Unreal Engine `FUnixPlatformProcess::UserSettingsDir()`.
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache.
    [[nodiscard]] static gp::StringView userSettingsDir() noexcept;

    /// @brief Queries the user Documents directory via `$XDG_DOCUMENTS_DIR` or `$HOME/Documents/` (overridable via
    ///        `-userdir=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` Documents directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView documentsDir() noexcept;

    /// @brief Alias for `documentsDir()` matching Unreal Engine `FUnixPlatformProcess::UserDir()`.
    /// @return Non-owning `StringView` pointing to the static `.bss` Documents directory cache.
    [[nodiscard]] static gp::StringView userDir() noexcept;

    /// @brief Queries the POSIX temporary directory via `$TMPDIR` or `"/var/tmp/"`.
    /// @return Non-owning `StringView` pointing to the static `.bss` temporary directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView tempDir() noexcept;
};

}   // namespace gp::platform::unix
