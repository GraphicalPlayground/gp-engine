// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/generic/system/GenericPaths.hpp"

namespace gp::platform::windows
{

/// @brief Win32 implementation of OS directory discovery and lexical path utilities (ADR-0043).
/// @details Queries Windows Shell Known Folders (`SHGetKnownFolderPath` with `FOLDERID_RoamingAppData`,
///          `FOLDERID_LocalAppData`, `FOLDERID_Documents`, `FOLDERID_Profile`) and Win32 process APIs
///          (`GetModuleFileNameW`, `GetCurrentDirectoryW`, `GetTempPathW`, `GetLongPathNameW`), converting UTF-16
///          kernel strings into lazy static `.bss` UTF-8 caches with normalized forward slashes (`'/'`).
class GP_CORE_API Paths : public gp::platform::generic::Paths
{
public:
    Paths() = delete;

    using gp::platform::generic::Paths::convertRelativePathToFull;
    using gp::platform::generic::Paths::resolve;

    /// @brief Queries the full normalized UTF-8 path to the running Win32 executable via `GetModuleFileNameW`.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable path cache.
    [[nodiscard]] static gp::StringView executablePath() noexcept;

    /// @brief Queries the filename of the running Win32 executable.
    /// @param[in] removeExtension If true (default), strips the `".exe"` extension.
    /// @return Non-owning `StringView` of the executable filename.
    [[nodiscard]] static gp::StringView executableName(const bool removeExtension = true) noexcept;

    /// @brief Queries the directory containing the running Win32 executable, guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable directory cache.
    [[nodiscard]] static gp::StringView executableDir() noexcept;

    /// @brief Queries the engine base directory (defaults to `executableDir()`, overridable via `-basedir=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` base directory cache (guaranteed trailing `'/'`).
    [[nodiscard]] static gp::StringView baseDir() noexcept;

    /// @brief Queries the current Win32 working directory via `GetCurrentDirectoryW`, normalized with a trailing `'/'`.
    /// @return Stack-allocated `PathBuffer` containing the current working directory.
    [[nodiscard]] static PathBuffer workingDir() noexcept;

    /// @brief Queries the initial working directory captured on first invocation during engine startup.
    /// @return Non-owning `StringView` pointing to the static `.bss` launch directory cache.
    [[nodiscard]] static gp::StringView launchDir() noexcept;

    /// @brief Queries the user profile directory via `SHGetKnownFolderPath(FOLDERID_Profile)`.
    /// @return Non-owning `StringView` pointing to the static `.bss` home directory cache (guaranteed trailing `'/'`).
    [[nodiscard]] static gp::StringView homeDir() noexcept;

    /// @brief Queries the roaming AppData directory via `SHGetKnownFolderPath(FOLDERID_RoamingAppData)` (overridable
    ///        via `-appdata=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` AppData directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView appDataDir() noexcept;

    /// @brief Queries the local AppData directory via `SHGetKnownFolderPath(FOLDERID_LocalAppData)`.
    /// @return Non-owning `StringView` pointing to the static `.bss` LocalAppData directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView localAppDataDir() noexcept;

    /// @brief Queries the user configuration directory on Windows (maps to `localAppDataDir()`).
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache.
    [[nodiscard]] static gp::StringView userConfigDir() noexcept;

    /// @brief Alias for `userConfigDir()` matching Unreal Engine `FWindowsPlatformProcess::UserSettingsDir()`.
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache.
    [[nodiscard]] static gp::StringView userSettingsDir() noexcept;

    /// @brief Queries the user's Documents directory via `SHGetKnownFolderPath(FOLDERID_Documents)` (overridable via
    ///        `-userdir=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` Documents directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView documentsDir() noexcept;

    /// @brief Alias for `documentsDir()` matching Unreal Engine `FWindowsPlatformProcess::UserDir()`.
    /// @return Non-owning `StringView` pointing to the static `.bss` Documents directory cache.
    [[nodiscard]] static gp::StringView userDir() noexcept;

    /// @brief Queries the system temporary directory via `GetTempPathW` and `GetLongPathNameW`.
    /// @return Non-owning `StringView` pointing to the static `.bss` temporary directory cache (guaranteed trailing
    ///         `'/'`).
    [[nodiscard]] static gp::StringView tempDir() noexcept;

    /// @brief Resolves `path` against `windows::Paths::baseDir()` and collapses all `"."` and `".."` segments.
    /// @param[in] path Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer resolve(const gp::StringView path) noexcept;

    /// @brief Unreal Engine `FPaths::ConvertRelativePathToFull(InPath)` compatibility overload using
    ///        `windows::Paths::baseDir()`.
    /// @param[in] inPath Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer convertRelativePathToFull(const gp::StringView inPath) noexcept;
};

}   // namespace gp::platform::windows
