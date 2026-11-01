// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathEnums.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "templates/Enums.hpp"
#include "templates/Optional.hpp"
#include <type_traits>
#include <utility>   // IWYU pragma: keep

namespace gp::platform::generic
{

/// @brief Platform-independent lexical path manipulation engine and fallback OS directory provider (ADR-0043).
/// @details Provides zero-allocation `constexpr` path slicing (`gp::StringView`), stack-allocated normalization and
///          relative-directory collapsing (`gp::platform::PathBuffer`), extension manipulation, path validation, and
///          fallback directory queries. Derived platform classes (`windows::Paths`, `unix::Paths`, `linux::Paths`,
///          `apple::Paths`, `macos::Paths`) inherit all lexical algorithms while specializing OS-specific known-folder
///          discovery.
class GP_CORE_API Paths
{
public:
    Paths() = delete;
    ~Paths() = delete;
    Paths(const Paths&) = delete;
    Paths& operator=(const Paths&) = delete;
    Paths(Paths&&) = delete;
    Paths& operator=(Paths&&) = delete;

public:
    /// @section OS Directory & Executable Discovery (Fallback / Base Contract)

    /// @brief Queries the full normalized UTF-8 path to the currently running executable binary.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable path cache.
    [[nodiscard]] static gp::StringView executablePath() noexcept;

    /// @brief Queries the filename of the currently running executable binary.
    /// @param[in] removeExtension If true (default), strips the file extension (e.g., `".exe"`).
    /// @return Non-owning `StringView` of the executable filename.
    [[nodiscard]] static gp::StringView executableName(const bool removeExtension = true) noexcept;

    /// @brief Queries the normalized directory containing the running executable, guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` executable directory cache.
    [[nodiscard]] static gp::StringView executableDir() noexcept;

    /// @brief Queries the engine base directory (defaults to `executableDir()`, overridable via `-basedir=<path>`).
    /// @return Non-owning `StringView` pointing to the static `.bss` base directory cache (guaranteed trailing `'/'`).
    [[nodiscard]] static gp::StringView baseDir() noexcept;

    /// @brief Queries the read-only application bundle resources directory.
    /// @details On macOS `.app` bundles (`MyGame.app/Contents/MacOS/`), resolves to `MyGame.app/Contents/Resources/`.
    ///          On all other platforms (or non-bundle builds), returns `baseDir()`. Guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` resources directory cache.
    [[nodiscard]] static gp::StringView appResourcesDir() noexcept;

    /// @brief Queries the current OS working directory of the calling process, normalized with a trailing `'/'`.
    /// @note Unlike known folders, the OS working directory is mutable at runtime (`chdir` / `SetCurrentDirectoryW`),
    ///       so this function returns a stack-allocated `PathBuffer` snapshot rather than a cached view.
    /// @return Normalized `PathBuffer` containing the current working directory.
    [[nodiscard]] static PathBuffer workingDir() noexcept;

    /// @brief Queries the initial working directory captured on first invocation during engine startup.
    /// @return Non-owning `StringView` pointing to the static `.bss` launch directory cache (guaranteed trailing
    /// `'/'`).
    [[nodiscard]] static gp::StringView launchDir() noexcept;

    /// @brief Queries the current user's home/profile directory, normalized with a trailing `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` home directory cache.
    [[nodiscard]] static gp::StringView homeDir() noexcept;

    /// @brief Queries the roaming/per-user application data directory (overridable via `-appdata=<path>`).
    /// @details Maps to `FOLDERID_RoamingAppData` on Windows, `$XDG_DATA_HOME` (`~/.local/share/`) on Linux, and
    ///          `~/Library/Application Support/` on macOS. Guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` AppData directory cache.
    [[nodiscard]] static gp::StringView appDataDir() noexcept;

    /// @brief Queries the machine-local application data and shader/DDC cache directory.
    /// @details Maps to `FOLDERID_LocalAppData` on Windows, `$XDG_CACHE_HOME` (`~/.cache/`) on Linux, and
    ///          `~/Library/Caches/` on macOS. Guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` LocalAppData directory cache.
    [[nodiscard]] static gp::StringView localAppDataDir() noexcept;

    /// @brief Queries the per-user configuration/preferences directory.
    /// @details Maps to `FOLDERID_LocalAppData` on Windows, `$XDG_CONFIG_HOME` (`~/.config/`) on Linux, and
    ///          `~/Library/Preferences/` on macOS. Guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache.
    [[nodiscard]] static gp::StringView userConfigDir() noexcept;

    /// @brief Alias for `userConfigDir()` matching Unreal Engine `FPlatformProcess::UserSettingsDir()` semantics.
    /// @return Non-owning `StringView` pointing to the static `.bss` user configuration directory cache.
    [[nodiscard]] static gp::StringView userSettingsDir() noexcept;

    /// @brief Queries the user's Documents directory for save games, screenshots, and user mods (overridable via
    ///        `-userdir=<path>`).
    /// @details Maps to `FOLDERID_Documents` on Windows, `$XDG_DOCUMENTS_DIR` (`~/Documents/`) on Linux, and
    ///          `~/Documents/` on macOS. Guaranteed to end with `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` Documents directory cache.
    [[nodiscard]] static gp::StringView documentsDir() noexcept;

    /// @brief Alias for `documentsDir()` matching Unreal Engine `FPlatformProcess::UserDir()` semantics.
    /// @return Non-owning `StringView` pointing to the static `.bss` Documents directory cache.
    [[nodiscard]] static gp::StringView userDir() noexcept;

    /// @brief Queries the system temporary directory, normalized with a trailing `'/'`.
    /// @return Non-owning `StringView` pointing to the static `.bss` temporary directory cache.
    [[nodiscard]] static gp::StringView tempDir() noexcept;

public:
    /// @section Deterministic Testing & Tooling Overrides

    /// @brief Overrides the cached executable path (and derived executable directory/name) for unit testing.
    /// @param[in] path Replacement executable file path (normalized automatically), or empty view to clear.
    static void setExecutablePathForTesting(const gp::StringView path) noexcept;

    /// @brief Overrides the cached base directory for unit testing.
    /// @param[in] dir Replacement base directory path (normalized with trailing `'/'`), or empty view to clear.
    static void setBaseDirForTesting(const gp::StringView dir) noexcept;

    /// @brief Overrides the cached roaming AppData directory for unit testing.
    /// @param[in] dir Replacement AppData directory path (normalized with trailing `'/'`), or empty view to clear.
    static void setAppDataDirForTesting(const gp::StringView dir) noexcept;

    /// @brief Overrides the cached user Documents directory for unit testing.
    /// @param[in] dir Replacement Documents directory path (normalized with trailing `'/'`), or empty view to clear.
    static void setDocumentsDirForTesting(const gp::StringView dir) noexcept;

    /// @brief Clears all testing overrides and invalidates all lazy `.bss` OS path caches so subsequent calls re-query.
    static void resetCachesForTesting() noexcept;

public:
    /// @section Character & Separator Classification (`constexpr`)

    /// @brief Checks whether a character is a recognized path directory separator (`'/'` or `'\\'`).
    /// @param[in] ch The character to inspect.
    /// @return True if `ch` is `'/'` or `'\\'`, false otherwise.
    [[nodiscard]] static constexpr bool isSeparator(const char ch) noexcept
    {
        return ch == kPreferredSeparator || ch == kAltSeparator;
    }

    /// @brief Checks whether a character is an ASCII alphabetic letter (`'A'`..`'Z'` or `'a'`..`'z'`).
    /// @param[in] ch The character to inspect.
    /// @return True if `ch` is an ASCII letter, false otherwise.
    [[nodiscard]] static constexpr bool isAsciiAlpha(const char ch) noexcept
    {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    }

    /// @brief Checks whether a character is an ASCII alphanumeric character (`'A'`..`'Z'`, `'a'`..`'z'`, `'0'`..`'9'`).
    /// @param[in] ch The character to inspect.
    /// @return True if `ch` is ASCII alphanumeric, false otherwise.
    [[nodiscard]] static constexpr bool isAsciiAlphaNumeric(const char ch) noexcept
    {
        return isAsciiAlpha(ch) || (ch >= '0' && ch <= '9');
    }

    /// @brief Converts an ASCII uppercase character to lowercase without locale dependency.
    /// @param[in] ch The character to convert.
    /// @return Lowercase ASCII equivalent if `ch` is `'A'`..`'Z'`, otherwise `ch` unchanged.
    [[nodiscard]] static constexpr char toLowerAscii(const char ch) noexcept
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            return static_cast<char>(ch + ('a' - 'A'));
        }
        return ch;
    }

    /// @brief Converts an ASCII lowercase character to uppercase without locale dependency.
    /// @param[in] ch The character to convert.
    /// @return Uppercase ASCII equivalent if `ch` is `'a'`..`'z'`, otherwise `ch` unchanged.
    [[nodiscard]] static constexpr char toUpperAscii(const char ch) noexcept
    {
        if (ch >= 'a' && ch <= 'z')
        {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return ch;
    }

    /// @brief Compares two string views lexicographically using ASCII case-insensitive character matching.
    /// @param[in] lhs Left-hand string view.
    /// @param[in] rhs Right-hand string view.
    /// @return Negative if `lhs < rhs`, zero if equal, positive if `lhs > rhs`.
    [[nodiscard]] static constexpr gp::Int32
        compareCaseInsensitive(const gp::StringView lhs, const gp::StringView rhs) noexcept
    {
        const gp::USize minLen = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
        for (gp::USize i = 0u; i < minLen; ++i)
        {
            const gp::UInt8 a = static_cast<gp::UInt8>(toLowerAscii(lhs[i]));
            const gp::UInt8 b = static_cast<gp::UInt8>(toLowerAscii(rhs[i]));
            if (a != b)
            {
                return (a < b) ? -1 : 1;
            }
        }

        if (lhs.size() < rhs.size())
        {
            return -1;
        }
        if (lhs.size() > rhs.size())
        {
            return 1;
        }
        return 0;
    }

public:
    /// @section Zero-Allocation `StringView` Slicing & Inspection (`constexpr`)

    /// @brief Splits a path into its leading volume/scheme/UNC prefix and the remaining path view.
    /// @details Recognizes Windows drive letters (`"C:"`), VFS URI schemes (`"engine:"` in `"engine://..."`), and
    ///          UNC network share server roots (`"//server"` or `"\\\\server"`).
    /// @param[in] path Input path view to decompose.
    /// @param[out] outVolume Receives the volume or scheme prefix (empty if none is present).
    /// @param[out] outRemainder Receives the remainder of `path` following `outVolume`.
    static constexpr void splitVolumeSpecifier(
        const gp::StringView path, gp::StringView& outVolume, gp::StringView& outRemainder
    ) noexcept
    {
        outVolume = {};
        outRemainder = path;

        if (path.size() >= 2u && isAsciiAlpha(path[0]) && path[1] == ':')
        {
            outVolume = path.substr(0u, 2u);
            outRemainder = path.substr(2u);
            return;
        }

        // Check for VFS URI scheme prefix (e.g., "engine://", "game://", "user://")
        if (path.size() >= 4u && isAsciiAlpha(path[0]))
        {
            gp::USize idx = 1u;
            while (idx < path.size() &&
                   (isAsciiAlphaNumeric(path[idx]) || path[idx] == '+' || path[idx] == '-' || path[idx] == '.'))
            {
                ++idx;
            }

            if (idx > 1u && (idx + 2u) < path.size() && path[idx] == ':' && isSeparator(path[idx + 1u]) &&
                isSeparator(path[idx + 2u]))
            {
                outVolume = path.substr(0u, idx + 1u);
                outRemainder = path.substr(idx + 1u);
                return;
            }
        }

        // Check for UNC network share prefix ("//server/share" or "\\server\share", or bare "//server")
        if (path.size() >= 2u && isSeparator(path[0]) && isSeparator(path[1]))
        {
            if (path.size() == 2u || isSeparator(path[2]))
            {
                outVolume = path.substr(0u, 2u);
                outRemainder = path.substr(2u);
                return;
            }

            gp::USize serverEnd = 2u;
            while (serverEnd < path.size() && !isSeparator(path[serverEnd]))
            {
                ++serverEnd;
            }

            // If a share component immediately follows the server name (e.g. "//server/share"), include it in the
            // UNC volume root so relative ".." segments cannot climb out of the share into the host name.
            if (serverEnd < path.size() && isSeparator(path[serverEnd]))
            {
                const gp::USize shareStart = serverEnd + 1u;
                if (shareStart < path.size() && !isSeparator(path[shareStart]))
                {
                    gp::USize shareEnd = shareStart;
                    while (shareEnd < path.size() && !isSeparator(path[shareEnd]))
                    {
                        ++shareEnd;
                    }
                    outVolume = path.substr(0u, shareEnd);
                    outRemainder = path.substr(shareEnd);
                    return;
                }
            }

            outVolume = path.substr(0u, serverEnd);
            outRemainder = path.substr(serverEnd);
        }
    }

    /// @brief Checks whether the path ends with a redundant trailing separator (ignoring root slashes like `"/"` or
    ///        `"C:/"`).
    /// @param[in] path Input path view to inspect.
    /// @return True if the path ends with a non-root separator, false otherwise.
    [[nodiscard]] static constexpr bool hasRedundantTerminatingSeparator(const gp::StringView path) noexcept
    {
        if (path.isEmpty() || !isSeparator(path.back()))
        {
            return false;
        }

        gp::StringView volume{};
        gp::StringView remainder{};
        splitVolumeSpecifier(path, volume, remainder);

        if (remainder.isEmpty())
        {
            return false;
        }

        // If the remainder after the volume consists solely of a single root slash (e.g., "/" or "C:/"), it is not
        // redundant. Multiple trailing slashes (e.g., "//" or "C://") or a slash after a component are redundant.
        if (!volume.isEmpty() && remainder.size() == 2u && isSeparator(remainder[0]) && isSeparator(remainder[1]) &&
            volume.back() == ':')
        {
            // URI scheme root "engine://" has 2 slashes after ':' which are part of the scheme root
            if (volume.size() > 2u)
            {
                return false;
            }
        }

        gp::USize nonSlashPos = remainder.size();
        while (nonSlashPos > 0u && isSeparator(remainder[nonSlashPos - 1u]))
        {
            --nonSlashPos;
        }

        if (nonSlashPos == 0u)
        {
            // All characters in remainder are slashes; 1 slash is the root, >1 slashes are redundant.
            return remainder.size() > 1u;
        }

        return true;
    }

    /// @brief Extracts the filename and extension portion after the last path separator.
    /// @details Matches Unreal Engine `FPaths::GetCleanFilename`: if `path` ends with a directory slash (e.g.,
    ///          `"Engine/Content/"`), returns an empty `StringView`.
    /// @param[in] path Input path view.
    /// @return Sub-view containing the filename and extension.
    [[nodiscard]] static constexpr gp::StringView getCleanFilename(const gp::StringView path) noexcept
    {
        if (path.isEmpty())
        {
            return {};
        }

        for (gp::USize i = path.size(); i > 0u; --i)
        {
            if (isSeparator(path[i - 1u]))
            {
                return path.substr(i);
            }
        }

        gp::StringView volume{};
        gp::StringView remainder{};
        splitVolumeSpecifier(path, volume, remainder);
        return remainder;
    }

    /// @brief Extracts the final path component, stripping any trailing directory separators first.
    /// @details Matches Unreal Engine `FPathViews::GetPathLeaf`: unlike `getCleanFilename()`, `"Engine/Content/"`
    ///          returns `"Content"`.
    /// @param[in] path Input path view.
    /// @return Sub-view containing the leaf component name.
    [[nodiscard]] static constexpr gp::StringView getPathLeaf(const gp::StringView path) noexcept
    {
        gp::StringView volume{};
        gp::StringView remainder{};
        splitVolumeSpecifier(path, volume, remainder);

        gp::USize endPos = remainder.size();
        while (endPos > 0u && isSeparator(remainder[endPos - 1u]))
        {
            --endPos;
        }

        if (endPos == 0u)
        {
            return {};
        }

        const gp::StringView trimmed = remainder.substr(0u, endPos);
        for (gp::USize i = trimmed.size(); i > 0u; --i)
        {
            if (isSeparator(trimmed[i - 1u]))
            {
                return trimmed.substr(i);
            }
        }

        return trimmed;
    }

    /// @brief Extracts the directory path preceding the last separator.
    /// @details Preserves the root slash for root-level files (`"/file.txt"` -> `"/"`, `"C:/file.txt"` -> `"C:/"`),
    ///          and returns an empty view when given a bare root (`"/"`, `"C:/"`) or a filename without directories.
    /// @param[in] path Input path view.
    /// @return Sub-view of the parent directory path.
    [[nodiscard]] static constexpr gp::StringView getPath(const gp::StringView path) noexcept
    {
        if (path.isEmpty())
        {
            return {};
        }

        gp::StringView volume{};
        gp::StringView remainder{};
        splitVolumeSpecifier(path, volume, remainder);

        // Trim trailing separators unless the entire remainder is just a single root separator
        gp::USize searchEnd = path.size();
        if (hasRedundantTerminatingSeparator(path))
        {
            while (searchEnd > volume.size() && isSeparator(path[searchEnd - 1u]))
            {
                --searchEnd;
            }
        }
        else if (
            remainder.size() <= 1u || (volume.size() > 2u && volume.back() == ':' && remainder.size() == 2u &&
                                       isSeparator(remainder[0]) && isSeparator(remainder[1]))
        )
        {
            // Bare root ("/", "C:/", "C:", "engine://") has no parent path
            return {};
        }

        const gp::StringView activeView = path.substr(0u, searchEnd);
        gp::USize lastSep = gp::StringView::npos;
        for (gp::USize i = activeView.size(); i > volume.size(); --i)
        {
            if (isSeparator(activeView[i - 1u]))
            {
                lastSep = i - 1u;
                break;
            }
        }

        if (lastSep == gp::StringView::npos)
        {
            return volume;
        }

        // Collapse consecutive separators immediately preceding lastSep
        gp::USize dirEnd = lastSep;
        while (dirEnd > volume.size() && isSeparator(activeView[dirEnd - 1u]))
        {
            --dirEnd;
        }

        // If the separator was at the root boundary (e.g., "/file.txt" or "C:/file.txt" or "engine://file.txt"),
        // keep the root separator(s) so the returned directory remains rooted.
        if (dirEnd == volume.size())
        {
            if (volume.size() > 2u && volume.back() == ':' && (volume.size() + 2u) <= activeView.size() &&
                isSeparator(activeView[volume.size()]) && isSeparator(activeView[volume.size() + 1u]))
            {
                return activeView.substr(0u, volume.size() + 2u);
            }
            return activeView.substr(0u, volume.size() + 1u);
        }

        return activeView.substr(0u, dirEnd);
    }

    /// @brief Extracts the file extension from a path view.
    /// @details Ignores dots in parent directory names and treats leading dotfiles without a second dot (e.g.,
    ///          `".gitignore"`) as filenames without an extension.
    /// @param[in] path Input path view.
    /// @param[in] flags Combination of `PathFlags::IncludeDot` and/or `PathFlags::AllowCompoundExtension`.
    /// @return Sub-view of the file extension, or empty view if no extension is present.
    [[nodiscard]] static constexpr gp::StringView
        getExtension(const gp::StringView path, const PathFlags flags = PathFlags::None) noexcept
    {
        const gp::StringView cleanName = getCleanFilename(path);
        if (cleanName.size() <= 1u || cleanName == "..")
        {
            return {};
        }

        const bool allowCompound = gp::enums::hasAllFlags(flags, PathFlags::AllowCompoundExtension);
        const bool includeDot = gp::enums::hasAllFlags(flags, PathFlags::IncludeDot);

        gp::USize dotPos = gp::StringView::npos;
        if (allowCompound)
        {
            // Search forward from index 1 so a leading dot on a hidden file (".config.json") is skipped
            for (gp::USize i = 1u; i < cleanName.size(); ++i)
            {
                if (cleanName[i] == '.')
                {
                    dotPos = i;
                    break;
                }
            }
        }
        else
        {
            // Search backward down to index 1 so ".gitignore" (dot at index 0) is treated as a stem
            for (gp::USize i = cleanName.size() - 1u; i >= 1u; --i)
            {
                if (cleanName[i] == '.')
                {
                    dotPos = i;
                    break;
                }
            }
        }

        if (dotPos == gp::StringView::npos)
        {
            return {};
        }

        // A trailing dot with nothing after it (e.g., "file.") has no extension characters
        if ((dotPos + 1u) >= cleanName.size())
        {
            return includeDot ? cleanName.substr(dotPos) : gp::StringView{};
        }

        return cleanName.substr(includeDot ? dotPos : (dotPos + 1u));
    }

    /// @brief Overload of `getExtension()` accepting a boolean `includeDot` flag for `FPaths::GetExtension` parity.
    /// @param[in] path Input path view.
    /// @param[in] includeDot If true, includes the leading `'.'` in the returned extension view.
    /// @return Sub-view of the file extension, or empty view if none exists.
    [[nodiscard]] static constexpr gp::StringView
        getExtension(const gp::StringView path, const bool includeDot) noexcept
    {
        return getExtension(path, includeDot ? PathFlags::IncludeDot : PathFlags::None);
    }

    /// @brief Extracts the base filename without its extension.
    /// @param[in] path Input path view.
    /// @param[in] removePath If true (default), strips the directory prefix and returns only the filename stem;
    ///                       if false, retains the directory prefix and strips the extension.
    /// @param[in] flags Optional `PathFlags::AllowCompoundExtension` to strip compound extensions (e.g. `".tar.gz"`).
    /// @return Sub-view containing the base filename (with or without directory prefix).
    [[nodiscard]] static constexpr gp::StringView getBaseFilename(
        const gp::StringView path, const bool removePath = true, const PathFlags flags = PathFlags::None
    ) noexcept
    {
        const gp::StringView cleanName = getCleanFilename(path);
        const gp::StringView extWithDot = getExtension(cleanName, flags | PathFlags::IncludeDot);

        if (removePath)
        {
            return cleanName.substr(0u, cleanName.size() - extWithDot.size());
        }

        return path.substr(0u, path.size() - extWithDot.size());
    }

    /// @brief Extracts the full path and filename with the final file extension removed.
    /// @param[in] path Input path view.
    /// @return Sub-view of `path` without its final `.ext` suffix.
    [[nodiscard]] static constexpr gp::StringView getBaseFilenameWithPath(const gp::StringView path) noexcept
    {
        return getBaseFilename(path, false);
    }

    /// @brief Decomposes a path into its directory prefix, base filename stem, and extension views (without dot).
    /// @param[in] path Input path view to split.
    /// @param[out] outDirectory Receives the directory portion (`getPath(path)`).
    /// @param[out] outFilename Receives the base filename stem (`getBaseFilename(path, true)`).
    /// @param[out] outExtension Receives the extension without leading dot (`getExtension(path, PathFlags::None)`).
    static constexpr void split(
        const gp::StringView path,
        gp::StringView& outDirectory,
        gp::StringView& outFilename,
        gp::StringView& outExtension
    ) noexcept
    {
        const gp::StringView cleanName = getCleanFilename(path);
        if (cleanName.isEmpty())
        {
            // Path ends with a trailing separator (e.g., "Engine/Content/"): entire path (minus trailing slash) is dir
            gp::USize endPos = path.size();
            if (hasRedundantTerminatingSeparator(path))
            {
                while (endPos > 0u && isSeparator(path[endPos - 1u]))
                {
                    --endPos;
                }
            }
            outDirectory = path.substr(0u, endPos);
            outFilename = {};
            outExtension = {};
            return;
        }

        outDirectory = getPath(path);
        outFilename = getBaseFilename(cleanName, true);
        outExtension = getExtension(cleanName, PathFlags::None);
    }

    /// @brief Decomposes a path into a `PathSplitResult` containing `directory`, `filename`, and `extension` views.
    /// @param[in] path Input path view to split.
    /// @return Populated `PathSplitResult` struct.
    [[nodiscard]] static constexpr PathSplitResult split(const gp::StringView path) noexcept
    {
        PathSplitResult result{};
        split(path, result.directory, result.filename, result.extension);
        return result;
    }

    /// @brief Splits a path into its first non-empty component and the remaining sub-path.
    /// @param[in] path Input path view.
    /// @param[out] outFirstComponent Receives the first path component.
    /// @param[out] outRemainder Receives the remaining path after the separator following `outFirstComponent`.
    static constexpr void splitFirstComponent(
        const gp::StringView path, gp::StringView& outFirstComponent, gp::StringView& outRemainder
    ) noexcept
    {
        outFirstComponent = {};
        outRemainder = {};

        gp::USize start = 0u;
        while (start < path.size() && isSeparator(path[start]))
        {
            ++start;
        }

        if (start >= path.size())
        {
            return;
        }

        gp::USize end = start;
        while (end < path.size() && !isSeparator(path[end]))
        {
            ++end;
        }

        outFirstComponent = path.substr(start, end - start);

        gp::USize nextStart = end;
        while (nextStart < path.size() && isSeparator(path[nextStart]))
        {
            ++nextStart;
        }

        outRemainder = path.substr(nextStart);
    }

    /// @brief Returns the prefix of `path` spanning the first `inclusiveComponentCount` components.
    /// @param[in] path Input path view.
    /// @param[in] inclusiveComponentCount Number of path components to include (must be `>= 1`).
    /// @return Sub-view containing the requested prefix, or empty view if `inclusiveComponentCount < 1`.
    [[nodiscard]] static constexpr gp::StringView
        getPathPrefix(const gp::StringView path, const gp::Int32 inclusiveComponentCount) noexcept
    {
        if (path.isEmpty() || inclusiveComponentCount < 1)
        {
            return {};
        }

        gp::USize pos = 0u;
        while (pos < path.size() && isSeparator(path[pos]))
        {
            ++pos;
        }

        for (gp::Int32 componentIdx = 0; componentIdx < inclusiveComponentCount; ++componentIdx)
        {
            while (pos < path.size() && !isSeparator(path[pos]))
            {
                ++pos;
            }

            if (pos >= path.size())
            {
                return path;
            }

            if ((componentIdx + 1) < inclusiveComponentCount)
            {
                while (pos < path.size() && isSeparator(path[pos]))
                {
                    ++pos;
                }
            }
        }

        return path.substr(0u, pos);
    }

public:
    /// @section Path Classification & Lexical Comparison (`constexpr`)

    /// @brief Checks whether `path` refers to a bare drive or root specifier (`"/"`, `"\\"`, `"C:"`, `"C:/"`,
    ///        `"//server"`).
    /// @param[in] path Input path view.
    /// @return True if `path` represents a root/drive without subdirectories, false otherwise.
    [[nodiscard]] static constexpr bool isDrive(const gp::StringView path) noexcept
    {
        if (path.isEmpty())
        {
            return false;
        }

        if (path.size() == 1u && isSeparator(path[0]))
        {
            return true;
        }

        if (path.size() == 2u && isSeparator(path[0]) && isSeparator(path[1]))
        {
            return true;
        }

        if (path.size() >= 2u && isAsciiAlpha(path[0]) && path[1] == ':')
        {
            if (path.size() == 2u)
            {
                return true;
            }
            if (path.size() == 3u && isSeparator(path[2]))
            {
                return true;
            }
        }

        return false;
    }

    /// @brief Checks whether `path` starts with a Windows drive specifier lacking a root separator (e.g., `"C:foo"` or
    ///        `"C:"`).
    /// @param[in] path Input path view.
    /// @return True if `path` has `"X:"` not immediately followed by `'/'` or `'\\'`, false otherwise.
    [[nodiscard]] static constexpr bool isDriveSpecifierWithoutRoot(const gp::StringView path) noexcept
    {
        if (path.size() >= 2u && isAsciiAlpha(path[0]) && path[1] == ':')
        {
            return path.size() == 2u || !isSeparator(path[2]);
        }
        return false;
    }

    /// @brief Checks whether `path` is a relative path rather than a rooted/absolute path.
    /// @details A path is considered absolute (not relative) if it begins with a POSIX/UNC separator (`'/'` or `'\\'`),
    ///          a rooted Windows drive specifier (`"C:/"` or `"C:\\"`), or a VFS URI scheme (`"engine://"`).
    ///          Unrooted drive paths (`"C:foo"`) and empty paths are classified as relative.
    /// @param[in] path Input path view.
    /// @return True if `path` is relative, false if `path` is absolute/rooted.
    [[nodiscard]] static constexpr bool isRelative(const gp::StringView path) noexcept
    {
        if (path.isEmpty())
        {
            return true;
        }

        if (isSeparator(path[0]))
        {
            return false;
        }

        if (path.size() >= 3u && isAsciiAlpha(path[0]) && path[1] == ':' && isSeparator(path[2]))
        {
            return false;
        }

        gp::StringView volume{};
        gp::StringView remainder{};
        splitVolumeSpecifier(path, volume, remainder);
        if (volume.size() > 2u && volume.back() == ':' && remainder.size() >= 2u && isSeparator(remainder[0]) &&
            isSeparator(remainder[1]))
        {
            return false;
        }

        return true;
    }

    /// @brief Checks whether `path` is an absolute (rooted) path.
    /// @param[in] path Input path view.
    /// @return True if `path` is non-empty and rooted, false otherwise.
    [[nodiscard]] static constexpr bool isAbsolute(const gp::StringView path) noexcept
    {
        return !path.isEmpty() && !isRelative(path);
    }

    /// @brief Checks whether `path` contains only a single leaf component with no internal directory separators.
    /// @param[in] path Input path view.
    /// @return True if `path` has no directory separators before its leaf component.
    [[nodiscard]] static constexpr bool isPathLeaf(const gp::StringView path) noexcept
    {
        gp::StringView volume{};
        gp::StringView remainder{};
        splitVolumeSpecifier(path, volume, remainder);
        if (!volume.isEmpty())
        {
            return remainder.isEmpty();
        }

        gp::USize endPos = remainder.size();
        while (endPos > 0u && isSeparator(remainder[endPos - 1u]))
        {
            --endPos;
        }

        for (gp::USize i = 0u; i < endPos; ++i)
        {
            if (isSeparator(remainder[i]))
            {
                return false;
            }
        }
        return true;
    }

public:
    /// @section Stack-Allocated `PathBuffer` Normalization & Collapsing (`constexpr`)

    /// @brief Normalizes all directory separators in `inOutPath` to `'/'`, strips Win32 extended-length prefixes
    ///        (`"\\?\"` and `"\\?\UNC\"`), and removes redundant duplicate slashes in-place.
    /// @details Preserves a leading `"//"` for UNC network paths and `"://"` for VFS URI schemes.
    /// @param[in,out] inOutPath Path buffer to normalize in-place.
    static constexpr void normalizeFilename(PathBuffer& inOutPath) noexcept
    {
        if (inOutPath.isEmpty())
        {
            return;
        }

        bool isExtendedUnc = false;
        const gp::StringView stripped = stripExtendedLengthPrefix(inOutPath.view(), isExtendedUnc);
        if (isExtendedUnc)
        {
            PathBuffer uncBuffer("//");
            appendClamped(uncBuffer, stripped);
            inOutPath = uncBuffer;
        }
        else if (stripped.size() != inOutPath.size())
        {
            const PathBuffer directBuffer = clampToPathBuffer(stripped);
            inOutPath = directBuffer;
        }

        char* const data = inOutPath.data();
        const gp::USize len = inOutPath.size();
        for (gp::USize i = 0u; i < len; ++i)
        {
            if (data[i] == kAltSeparator)
            {
                data[i] = kPreferredSeparator;
            }
        }

        removeDuplicateSlashes(inOutPath);
    }

    /// @brief Returns a new `PathBuffer` with all separators converted to `'/'` and duplicate slashes removed.
    /// @param[in] path Input path view (clamped to `kMaxPathLength` if overlong).
    /// @return Normalized `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer createNormalizedFilename(const gp::StringView path) noexcept
    {
        PathBuffer result = clampToPathBuffer(path);
        normalizeFilename(result);
        return result;
    }

    /// @brief Normalizes `inOutPath` and strips any redundant trailing directory separator (preserving `"/"` and
    ///        `"C:/"`).
    /// @param[in,out] inOutPath Directory path buffer to normalize in-place.
    static constexpr void normalizeDirectoryName(PathBuffer& inOutPath) noexcept
    {
        normalizeFilename(inOutPath);
        while (hasRedundantTerminatingSeparator(inOutPath.view()))
        {
            inOutPath.popBack();
        }
    }

    /// @brief Returns a new `PathBuffer` normalized as a directory name (without redundant trailing slash).
    /// @param[in] path Input directory path view.
    /// @return Normalized `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer createNormalizedDirectoryName(const gp::StringView path) noexcept
    {
        PathBuffer result = clampToPathBuffer(path);
        normalizeDirectoryName(result);
        return result;
    }

    /// @brief Appends a trailing `'/'` separator to `inOutPath` if it is non-empty and does not already end with one.
    /// @param[in,out] inOutPath Directory path buffer to terminate with `'/'`.
    static constexpr void ensureTrailingSeparator(PathBuffer& inOutPath) noexcept
    {
        if (!inOutPath.isEmpty())
        {
            if (inOutPath.back() == kAltSeparator)
            {
                inOutPath[static_cast<PathBuffer::SizeType>(inOutPath.size() - 1u)] = kPreferredSeparator;
            }
            else if (inOutPath.back() != kPreferredSeparator && inOutPath.size() < inOutPath.capacity())
            {
                inOutPath.pushBack(kPreferredSeparator);
            }
        }
    }

    /// @brief Removes consecutive duplicate forward slashes (`"//" -> "/"`) in-place with zero temporary buffer copies.
    /// @details Preserves a leading `"//"` at index 0 (UNC network share) and `"://"` (VFS logical URI scheme).
    /// @param[in,out] inOutPath Path buffer to compact in-place.
    static constexpr void removeDuplicateSlashes(PathBuffer& inOutPath) noexcept
    {
        const gp::USize origLen = inOutPath.size();
        if (origLen < 2u)
        {
            return;
        }

        char* const data = inOutPath.data();
        gp::USize writeIdx = 0u;

        for (gp::USize readIdx = 0u; readIdx < origLen; ++readIdx)
        {
            const char ch = isSeparator(data[readIdx]) ? kPreferredSeparator : data[readIdx];
            if (ch == kPreferredSeparator && writeIdx > 0u && data[writeIdx - 1u] == kPreferredSeparator)
            {
                // Preserve leading "//" for UNC share paths (only when the original path is not "///" root)
                if (writeIdx == 1u && readIdx == 1u && (origLen == 2u || !isSeparator(data[2])))
                {
                    data[writeIdx++] = ch;
                    continue;
                }

                // Preserve "://" for VFS URI schemes
                if (writeIdx >= 3u && data[writeIdx - 2u] == ':' && data[writeIdx - 3u] != kPreferredSeparator)
                {
                    data[writeIdx++] = ch;
                    continue;
                }

                continue;
            }

            data[writeIdx++] = ch;
        }

        while (inOutPath.size() > writeIdx)
        {
            inOutPath.popBack();
        }
    }

    /// @brief Returns a copy of `path` with consecutive duplicate slashes collapsed.
    /// @param[in] path Input path view.
    /// @return `PathBuffer` without duplicate slashes.
    [[nodiscard]] static constexpr PathBuffer createWithoutDuplicateSlashes(const gp::StringView path) noexcept
    {
        PathBuffer result = clampToPathBuffer(path);
        removeDuplicateSlashes(result);
        return result;
    }

    /// @brief Lexically resolves and collapses `"."` and `".."` relative directory segments in-place.
    /// @details Operates purely lexically without touching the OS filesystem.
    ///          - Single-dot `"."` segments are removed (`"a/./b"` -> `"a/b"`).
    ///          - Double-dot `".."` segments pop the preceding non-`".."` component (`"a/b/../c"` -> `"a/c"`).
    ///          - On relative paths (`"../a/b"`), leading `".."` segments that have no parent component are preserved
    ///            when `collapseAllPossible` is true, or cause the function to return `false` when
    ///            `collapseAllPossible` is false.
    ///          - On rooted/absolute paths (`"C:/../a"` or `"/a/../../b"`), attempting to traverse above the root
    ///            clamps at the root and returns `false` to signal root-escape saturation.
    /// @param[in,out] inOutPath Path buffer to collapse in-place.
    /// @param[in] collapseAllPossible If true (default), collapses all resolvable segments even if leading `".."`
    ///                                segments remain on a relative path.
    /// @return True if all `".."` segments were validly resolved (or preserved as relative prefixes when
    ///         `collapseAllPossible` is true); false if a `".."` attempted to escape above a root or could not be
    ///         collapsed when `collapseAllPossible` is false.
    static constexpr bool
        collapseRelativeDirectories(PathBuffer& inOutPath, const bool collapseAllPossible = true) noexcept
    {
        if (inOutPath.isEmpty())
        {
            return true;
        }

        normalizeFilename(inOutPath);
        const PathBuffer snapshot = inOutPath;
        const gp::StringView fullView = snapshot.view();

        gp::StringView volume{};
        gp::StringView remainder{};
        splitVolumeSpecifier(fullView, volume, remainder);

        bool isRooted = false;
        gp::StringView rootSlashes{};

        if (volume.size() > 2u && volume.back() == ':' && remainder.size() >= 2u && remainder[0] == '/' &&
            remainder[1] == '/')
        {
            isRooted = true;
            rootSlashes = remainder.substr(0u, 2u);
            remainder = remainder.substr(2u);
        }
        else if (!remainder.isEmpty() && remainder[0] == '/')
        {
            isRooted = true;
            if (volume.isEmpty() && remainder.size() >= 2u && remainder[1] == '/')
            {
                rootSlashes = remainder.substr(0u, 2u);
                remainder = remainder.substr(2u);
            }
            else
            {
                rootSlashes = remainder.substr(0u, 1u);
                remainder = remainder.substr(1u);
            }
        }
        else if (volume.size() >= 2u && volume[0] == '/' && volume[1] == '/')
        {
            isRooted = true;
        }

        const bool hadTrailingSlash =
            !remainder.isEmpty() && remainder.back() == '/' && (fullView.size() > (volume.size() + rootSlashes.size()));

        struct ComponentSpan
        {
            gp::UInt16 offset{ 0u };
            gp::UInt16 length{ 0u };
            bool isParentDotDot{ false };
        };

        ComponentSpan stack[512]{};
        gp::UInt32 stackCount = 0u;
        bool allCollapsed = true;

        gp::USize pos = 0u;
        while (pos < remainder.size())
        {
            while (pos < remainder.size() && remainder[pos] == '/')
            {
                ++pos;
            }
            if (pos >= remainder.size())
            {
                break;
            }

            const gp::USize segStart = pos;
            while (pos < remainder.size() && remainder[pos] != '/')
            {
                ++pos;
            }

            const gp::StringView segment = remainder.substr(segStart, pos - segStart);
            if (segment == ".")
            {
                continue;
            }

            if (segment == "..")
            {
                if (stackCount > 0u && !stack[stackCount - 1u].isParentDotDot)
                {
                    --stackCount;
                }
                else if (isRooted)
                {
                    // Cannot climb above an absolute root
                    allCollapsed = false;
                    if (!collapseAllPossible)
                    {
                        return false;
                    }
                }
                else
                {
                    // Relative path with leading ".."
                    if (!collapseAllPossible)
                    {
                        return false;
                    }
                    if (stackCount < 512u)
                    {
                        const gp::USize absOffset = static_cast<gp::USize>(segment.data() - fullView.data());
                        stack[stackCount] = ComponentSpan{ static_cast<gp::UInt16>(absOffset),
                                                           static_cast<gp::UInt16>(segment.size()),
                                                           true };
                        ++stackCount;
                    }
                }
            }
            else
            {
                if (stackCount < 512u)
                {
                    const gp::USize absOffset = static_cast<gp::USize>(segment.data() - fullView.data());
                    stack[stackCount] = ComponentSpan{ static_cast<gp::UInt16>(absOffset),
                                                       static_cast<gp::UInt16>(segment.size()),
                                                       false };
                    ++stackCount;
                }
            }
        }

        inOutPath.clear();
        inOutPath.append(volume);
        inOutPath.append(rootSlashes);

        for (gp::UInt32 i = 0u; i < stackCount; ++i)
        {
            if (i > 0u)
            {
                inOutPath.pushBack(kPreferredSeparator);
            }
            const gp::StringView comp = fullView.substr(stack[i].offset, stack[i].length);
            inOutPath.append(comp);
        }

        if (hadTrailingSlash && stackCount > 0u && inOutPath.back() != kPreferredSeparator &&
            inOutPath.size() < inOutPath.capacity())
        {
            inOutPath.pushBack(kPreferredSeparator);
        }

        if (inOutPath.isEmpty() && !fullView.isEmpty() && !isRooted && volume.isEmpty())
        {
            inOutPath.assign(".");
        }

        return allCollapsed;
    }

    /// @brief Standardizes a path in-place by normalizing slashes, collapsing relative `"."` / `".."` segments, and
    ///        stripping redundant trailing separators in a single normalization pass.
    /// @param[in,out] inOutPath Path buffer to standardize in-place.
    static constexpr void makeStandardFilename(PathBuffer& inOutPath) noexcept
    {
        (void)collapseRelativeDirectories(inOutPath, true);
        while (hasRedundantTerminatingSeparator(inOutPath.view()))
        {
            inOutPath.popBack();
        }
    }

    /// @brief Returns a standardized copy of `path` with normalized slashes, collapsed `"."` / `".."` segments, and no
    ///        redundant trailing separator.
    /// @param[in] path Input path view.
    /// @return Standardized `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer createStandardFilename(const gp::StringView path) noexcept
    {
        PathBuffer result = clampToPathBuffer(path);
        makeStandardFilename(result);
        return result;
    }

    /// @brief Converts all directory separators in `inOutPath` to the host OS native separator (`'\\'` on Windows,
    ///        `'/'` on POSIX).
    /// @param[in,out] inOutPath Path buffer to convert in-place.
    static constexpr void makePlatformFilename(PathBuffer& inOutPath) noexcept
    {
        char* const data = inOutPath.data();
        const gp::USize len = inOutPath.size();
        for (gp::USize i = 0u; i < len; ++i)
        {
            if (isSeparator(data[i]))
            {
                data[i] = kNativeSeparator;
            }
        }
    }

    /// @brief Returns a copy of `path` with all separators converted to `kNativeSeparator`.
    /// @param[in] path Input path view.
    /// @return Platform-formatted `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer createPlatformFilename(const gp::StringView path) noexcept
    {
        PathBuffer result = clampToPathBuffer(path);
        makePlatformFilename(result);
        return result;
    }

public:
    /// @section Zero-Allocation Path Comparison & Hierarchy Queries (`constexpr`)

    /// @brief Compares two paths for lexical equivalence after normalizing slashes and collapsing relative segments.
    /// @details Uses a zero-allocation `StringView` fast path when neither path contains `"."` / `".."` segments or
    ///          duplicate slashes, and falls back to `createStandardFilename()` otherwise.
    /// @param[in] lhs First path view.
    /// @param[in] rhs Second path view.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive comparison; if true, exact match.
    /// @return True if both paths resolve to the same lexical location, false otherwise.
    [[nodiscard]] static constexpr bool
        equals(const gp::StringView lhs, const gp::StringView rhs, const bool caseSensitive = false) noexcept
    {
        if (canCompareDirectlyWithoutCollapsing(lhs) && canCompareDirectlyWithoutCollapsing(rhs))
        {
            const gp::StringView trimLhs = trimRedundantTerminatingSeparator(lhs);
            const gp::StringView trimRhs = trimRedundantTerminatingSeparator(rhs);
            if (trimLhs.size() != trimRhs.size())
            {
                return false;
            }
            return compareDirectNormalizedViews(trimLhs, trimRhs, caseSensitive) == 0;
        }

        const PathBuffer stdLhs = createStandardFilename(lhs);
        const PathBuffer stdRhs = createStandardFilename(rhs);

        if (caseSensitive)
        {
            return stdLhs.view() == stdRhs.view();
        }
        return compareCaseInsensitive(stdLhs.view(), stdRhs.view()) == 0;
    }

    /// @brief Alias for `equals()` matching Unreal Engine `FPaths::IsSamePath()` terminology.
    /// @param[in] lhs First path view.
    /// @param[in] rhs Second path view.
    /// @param[in] caseSensitive If false (default), performs ASCII case-insensitive comparison.
    /// @return True if both paths are lexically equivalent.
    [[nodiscard]] static constexpr bool
        isSamePath(const gp::StringView lhs, const gp::StringView rhs, const bool caseSensitive = false) noexcept
    {
        return equals(lhs, rhs, caseSensitive);
    }

    /// @brief Strict weak ordering comparison for standardized paths (`stdLhs < stdRhs`).
    /// @details Uses a zero-allocation `StringView` fast path when neither path contains `"."` / `".."` segments or
    ///          duplicate slashes.
    /// @param[in] lhs First path view.
    /// @param[in] rhs Second path view.
    /// @param[in] caseSensitive If false (default), orders case-insensitively.
    /// @return True if `lhs` precedes `rhs` in standardized lexical order.
    [[nodiscard]] static constexpr bool
        less(const gp::StringView lhs, const gp::StringView rhs, const bool caseSensitive = false) noexcept
    {
        if (canCompareDirectlyWithoutCollapsing(lhs) && canCompareDirectlyWithoutCollapsing(rhs))
        {
            const gp::StringView trimLhs = trimRedundantTerminatingSeparator(lhs);
            const gp::StringView trimRhs = trimRedundantTerminatingSeparator(rhs);
            return compareDirectNormalizedViews(trimLhs, trimRhs, caseSensitive) < 0;
        }

        const PathBuffer stdLhs = createStandardFilename(lhs);
        const PathBuffer stdRhs = createStandardFilename(rhs);

        if (caseSensitive)
        {
            return stdLhs.view().compare(stdRhs.view()) < 0;
        }
        return compareCaseInsensitive(stdLhs.view(), stdRhs.view()) < 0;
    }

    /// @brief Attempts to slice `child` as a relative sub-view of `parent` without allocating or mutating buffers.
    /// @details Matches Unreal Engine `FPathViews::TryMakeChildPathRelativeTo`. If `child` is identical to `parent`,
    ///          `outRelPath` is set to an empty `StringView` and `true` is returned. If `child` is inside `parent`,
    ///          `outRelPath` slices the portion of `child` after the `parent` prefix and separator.
    /// @param[in] child Candidate child path view.
    /// @param[in] parent Candidate parent directory view.
    /// @param[out] outRelPath Receives the sub-view of `child` relative to `parent` on success.
    /// @param[in] caseSensitive If false (default), matches path components case-insensitively.
    /// @return True if `child` is equal to or a descendant of `parent`, false otherwise.
    [[nodiscard]] static constexpr bool tryMakeChildPathRelativeTo(
        const gp::StringView child,
        const gp::StringView parent,
        gp::StringView& outRelPath,
        const bool caseSensitive = false
    ) noexcept
    {
        outRelPath = {};
        if (child.isEmpty() || parent.isEmpty())
        {
            return false;
        }

        gp::USize cIdx = 0u;
        gp::USize pIdx = 0u;

        // Trim redundant trailing slashes on parent
        gp::USize pEnd = parent.size();
        if (hasRedundantTerminatingSeparator(parent))
        {
            while (pEnd > 0u && isSeparator(parent[pEnd - 1u]))
            {
                --pEnd;
            }
        }

        while (pIdx < pEnd && cIdx < child.size())
        {
            const bool pSep = isSeparator(parent[pIdx]);
            const bool cSep = isSeparator(child[cIdx]);

            if (pSep && cSep)
            {
                while (pIdx < pEnd && isSeparator(parent[pIdx]))
                {
                    ++pIdx;
                }
                while (cIdx < child.size() && isSeparator(child[cIdx]))
                {
                    ++cIdx;
                }
                continue;
            }

            if (pSep != cSep)
            {
                return false;
            }

            const char pChar = caseSensitive ? parent[pIdx] : toLowerAscii(parent[pIdx]);
            const char cChar = caseSensitive ? child[cIdx] : toLowerAscii(child[cIdx]);
            if (pChar != cChar)
            {
                return false;
            }

            ++pIdx;
            ++cIdx;
        }

        if (pIdx < pEnd)
        {
            return false;
        }

        if (cIdx == child.size())
        {
            outRelPath = child.substr(child.size(), 0u);
            return true;
        }

        // If parent ended with a root slash (e.g. "/" or "C:/"), cIdx is already at the first child component;
        // otherwise child[cIdx] must be a separator boundary so "C:/Dir2" does not match "C:/Dir".
        const bool parentEndedWithSlash = isSeparator(parent[pEnd - 1u]);
        if (!parentEndedWithSlash && !isSeparator(child[cIdx]))
        {
            return false;
        }

        while (cIdx < child.size() && isSeparator(child[cIdx]))
        {
            ++cIdx;
        }

        outRelPath = child.substr(cIdx);
        return true;
    }

    /// @brief Checks whether `parent` is an ancestor/parent directory of `child` (or equal to `child`).
    /// @param[in] parent Candidate parent directory path.
    /// @param[in] child Candidate child path.
    /// @param[in] caseSensitive If false (default), compares case-insensitively.
    /// @return True if `child` is located inside or equal to `parent`.
    [[nodiscard]] static constexpr bool isParentPathOf(
        const gp::StringView parent, const gp::StringView child, const bool caseSensitive = false
    ) noexcept
    {
        const PathBuffer stdParent = createStandardFilename(parent);
        const PathBuffer stdChild = createStandardFilename(child);
        gp::StringView unusedRel{};
        return tryMakeChildPathRelativeTo(stdChild.view(), stdParent.view(), unusedRel, caseSensitive);
    }

    /// @brief Checks whether `path` resides under `directory` (after collapsing relative `".."` segments).
    /// @param[in] path Candidate file or subdirectory path.
    /// @param[in] directory Candidate parent directory path.
    /// @param[in] caseSensitive If false (default), compares case-insensitively.
    /// @return True if `path` is under or equal to `directory`.
    [[nodiscard]] static constexpr bool isUnderDirectory(
        const gp::StringView path, const gp::StringView directory, const bool caseSensitive = false
    ) noexcept
    {
        return isParentPathOf(directory, path, caseSensitive);
    }

public:
    /// @section Path Composition, Extension Mutation & Relative/Absolute Resolution (`constexpr`)

    /// @brief Appends a single relative path fragment onto `inOutPath` with a `'/'` separator and normalized slashes.
    /// @param[in,out] inOutPath Base path buffer to append into.
    /// @param[in] part Sub-path fragment to append.
    static constexpr void append(PathBuffer& inOutPath, const gp::StringView part) noexcept
    {
        if (part.isEmpty())
        {
            return;
        }

        gp::USize startIdx = 0u;
        if (!inOutPath.isEmpty())
        {
            if (isSeparator(inOutPath.back()))
            {
                inOutPath[static_cast<PathBuffer::SizeType>(inOutPath.size() - 1u)] = kPreferredSeparator;
            }
            else if (inOutPath.size() < inOutPath.capacity())
            {
                inOutPath.pushBack(kPreferredSeparator);
            }

            while (startIdx < part.size() && isSeparator(part[startIdx]))
            {
                ++startIdx;
            }
        }

        for (gp::USize i = startIdx; i < part.size(); ++i)
        {
            const char ch = isSeparator(part[i]) ? kPreferredSeparator : part[i];
            if (ch == kPreferredSeparator && !inOutPath.isEmpty() && inOutPath.back() == kPreferredSeparator)
            {
                // Preserve leading "//" for UNC or "://" for VFS schemes if at the very start of inOutPath
                if (inOutPath.size() == 1u && i == 1u && (part.size() == 2u || !isSeparator(part[2])))
                {
                    inOutPath.pushBack(ch);
                    continue;
                }
                if (inOutPath.size() >= 3u &&
                    inOutPath[static_cast<PathBuffer::SizeType>(inOutPath.size() - 2u)] == ':' &&
                    inOutPath[static_cast<PathBuffer::SizeType>(inOutPath.size() - 3u)] != kPreferredSeparator)
                {
                    inOutPath.pushBack(ch);
                    continue;
                }
                continue;
            }

            if (inOutPath.size() < inOutPath.capacity())
            {
                inOutPath.pushBack(ch);
            }
        }
    }

    /// @brief Joins an arbitrary number of path views/strings into a single normalized `PathBuffer`.
    /// @tparam InPathTypes Variadic sequence of types convertible to `gp::StringView` (`const char*`, `StringView`,
    ///                     `PathBuffer`, `FixedString<N>`).
    /// @param[in] parts Path segments to join in order.
    /// @return Combined and slash-normalized `PathBuffer`.
    template <typename... InPathTypes>
    [[nodiscard]] static constexpr PathBuffer combine(const InPathTypes&... parts) noexcept
    {
        PathBuffer result{};
        (append(result, gp::StringView(parts)), ...);
        return result;
    }

    /// @brief Replaces the extension of `path` only if `path` already has an extension.
    /// @details Matches Unreal Engine `FPaths::ChangeExtension`: if `path` has no existing extension, returns `path`
    ///          unchanged. If `newExtension` is empty, strips the existing extension.
    /// @param[in] path Input file path view.
    /// @param[in] newExtension Replacement extension (with or without a leading `'.'`), or empty view to remove.
    /// @return `PathBuffer` with the updated extension.
    [[nodiscard]] static constexpr PathBuffer
        changeExtension(const gp::StringView path, const gp::StringView newExtension) noexcept
    {
        const gp::StringView existingExtWithDot = getExtension(path, PathFlags::IncludeDot);
        if (existingExtWithDot.isEmpty())
        {
            return clampToPathBuffer(path);
        }

        PathBuffer result = clampToPathBuffer(path.substr(0u, path.size() - existingExtWithDot.size()));
        if (!newExtension.isEmpty())
        {
            if (newExtension.front() != '.' && result.size() < result.capacity())
            {
                result.pushBack('.');
            }
            appendClamped(result, newExtension);
        }
        return result;
    }

    /// @brief Sets the extension of `path`, replacing any existing extension or appending one if none exists.
    /// @details Matches Unreal Engine `FPaths::SetExtension`: if `newExtension` is empty, removes any existing
    ///          extension.
    /// @param[in] path Input file path view.
    /// @param[in] newExtension Target extension (with or without a leading `'.'`), or empty view to remove.
    /// @return `PathBuffer` with the target extension applied.
    [[nodiscard]] static constexpr PathBuffer
        setExtension(const gp::StringView path, const gp::StringView newExtension) noexcept
    {
        const gp::StringView existingExtWithDot = getExtension(path, PathFlags::IncludeDot);
        PathBuffer result = clampToPathBuffer(path.substr(0u, path.size() - existingExtWithDot.size()));

        if (!newExtension.isEmpty() && !result.isEmpty())
        {
            if (newExtension.front() != '.' && result.size() < result.capacity())
            {
                result.pushBack('.');
            }
            appendClamped(result, newExtension);
        }
        return result;
    }

    /// @brief Resolves `path` against `basePath` and collapses all relative `"."` and `".."` segments.
    /// @details If `path` is already absolute, `basePath` is ignored and `path` is standardized directly.
    ///          If `path` is empty, returns the standardized `basePath`.
    /// @param[in] path Target relative or absolute path view.
    /// @param[in] basePath Base directory path view used when `path` is relative.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer resolve(const gp::StringView path, const gp::StringView basePath) noexcept
    {
        if (path.isEmpty())
        {
            return createStandardFilename(basePath);
        }

        if (isAbsolute(path))
        {
            return createStandardFilename(path);
        }

        PathBuffer combined = combine(basePath, path);
        makeStandardFilename(combined);
        return combined;
    }

    /// @brief Resolves `path` against the engine's `baseDir()` (or `workingDir()` if `baseDir()` is empty).
    /// @param[in] path Target relative or absolute path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer resolve(const gp::StringView path) noexcept;

    /// @brief Unreal Engine `FPaths::ConvertRelativePathToFull(BasePath, InPath)` compatibility overload.
    /// @param[in] basePath Base directory path view.
    /// @param[in] inPath Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer
        convertRelativePathToFull(const gp::StringView basePath, const gp::StringView inPath) noexcept
    {
        return resolve(inPath, basePath);
    }

    /// @brief Unreal Engine `FPaths::ConvertRelativePathToFull(InPath)` compatibility overload using `baseDir()`.
    /// @param[in] inPath Relative or absolute target path view.
    /// @return Standardized full `PathBuffer`.
    [[nodiscard]] static PathBuffer convertRelativePathToFull(const gp::StringView inPath) noexcept;

    /// @brief Converts `inOutPath` into a relative path measured from the directory `relativeTo`.
    /// @details Standardizes both paths, verifies that their volume/root prefixes match, strips common ancestor
    ///          components, and prepends `"../"` for each remaining directory level in `relativeTo`.
    ///          If both paths resolve to the exact same location, `inOutPath` is set to `"."`.
    ///          If the paths reside on different drives/schemes/UNC shares or mix relative and absolute roots,
    ///          `inOutPath` is left unchanged and `false` is returned.
    /// @note Uses compact 4-byte `TokenSpan` descriptors rather than 16-byte `StringView` arrays to keep the stack
    ///       frame small on fiber-based job threads.
    /// @param[in,out] inOutPath Target path buffer to convert in-place into a relative path.
    /// @param[in] relativeTo Base directory view from which the relative path is computed.
    /// @param[in] caseSensitive If false (default), matches common path components case-insensitively.
    /// @return True if a valid relative path was computed, false if the paths do not share a common root/volume.
    static constexpr bool makePathRelativeTo(
        PathBuffer& inOutPath, const gp::StringView relativeTo, const bool caseSensitive = false
    ) noexcept
    {
        if (inOutPath.isEmpty() || relativeTo.isEmpty())
        {
            return false;
        }

        const PathBuffer targetStd = createStandardFilename(inOutPath.view());
        const PathBuffer baseStd = createStandardFilename(relativeTo);

        const gp::StringView targetView = targetStd.view();
        const gp::StringView baseView = baseStd.view();

        if (isAbsolute(targetView) != isAbsolute(baseView))
        {
            return false;
        }

        gp::StringView targetVol{};
        gp::StringView targetRem{};
        splitVolumeSpecifier(targetView, targetVol, targetRem);

        gp::StringView baseVol{};
        gp::StringView baseRem{};
        splitVolumeSpecifier(baseView, baseVol, baseRem);

        const bool volumesMatch =
            caseSensitive ? (targetVol == baseVol) : (compareCaseInsensitive(targetVol, baseVol) == 0);
        if (!volumesMatch)
        {
            return false;
        }

        struct TokenSpan
        {
            gp::UInt16 offset{ 0u };
            gp::UInt16 length{ 0u };
        };

        TokenSpan targetTokens[256]{};
        gp::UInt32 targetCount = 0u;
        {
            gp::USize pos = 0u;
            while (pos < targetRem.size() && targetCount < 256u)
            {
                while (pos < targetRem.size() && isSeparator(targetRem[pos]))
                {
                    ++pos;
                }
                if (pos >= targetRem.size())
                {
                    break;
                }
                const gp::USize start = pos;
                while (pos < targetRem.size() && !isSeparator(targetRem[pos]))
                {
                    ++pos;
                }
                const gp::StringView tok = targetRem.substr(start, pos - start);
                if (tok != ".")
                {
                    targetTokens[targetCount++] =
                        TokenSpan{ static_cast<gp::UInt16>(start), static_cast<gp::UInt16>(pos - start) };
                }
            }
        }

        TokenSpan baseTokens[256]{};
        gp::UInt32 baseCount = 0u;
        {
            gp::USize pos = 0u;
            while (pos < baseRem.size() && baseCount < 256u)
            {
                while (pos < baseRem.size() && isSeparator(baseRem[pos]))
                {
                    ++pos;
                }
                if (pos >= baseRem.size())
                {
                    break;
                }
                const gp::USize start = pos;
                while (pos < baseRem.size() && !isSeparator(baseRem[pos]))
                {
                    ++pos;
                }
                const gp::StringView tok = baseRem.substr(start, pos - start);
                if (tok != ".")
                {
                    baseTokens[baseCount++] =
                        TokenSpan{ static_cast<gp::UInt16>(start), static_cast<gp::UInt16>(pos - start) };
                }
            }
        }

        gp::UInt32 commonCount = 0u;
        while (commonCount < targetCount && commonCount < baseCount)
        {
            const gp::StringView tTok =
                targetRem.substr(targetTokens[commonCount].offset, targetTokens[commonCount].length);
            const gp::StringView bTok = baseRem.substr(baseTokens[commonCount].offset, baseTokens[commonCount].length);
            const bool same = caseSensitive ? (tTok == bTok) : (compareCaseInsensitive(tTok, bTok) == 0);
            if (!same)
            {
                break;
            }
            ++commonCount;
        }

        // Cannot climb above a leading ".." that differs in a relative base path
        if (commonCount < baseCount &&
            baseRem.substr(baseTokens[commonCount].offset, baseTokens[commonCount].length) == "..")
        {
            return false;
        }

        PathBuffer result{};
        const gp::UInt32 upCount = baseCount - commonCount;
        for (gp::UInt32 i = 0u; i < upCount; ++i)
        {
            if (!result.isEmpty() && result.size() < result.capacity())
            {
                result.pushBack(kPreferredSeparator);
            }
            appendClamped(result, "..");
        }

        for (gp::UInt32 i = commonCount; i < targetCount; ++i)
        {
            if (!result.isEmpty() && result.size() < result.capacity())
            {
                result.pushBack(kPreferredSeparator);
            }
            const gp::StringView tTok = targetRem.substr(targetTokens[i].offset, targetTokens[i].length);
            appendClamped(result, tTok);
        }

        if (result.isEmpty())
        {
            result.assign(".");
        }

        inOutPath = result;
        return true;
    }

    /// @brief Functional wrapper around `makePathRelativeTo()` returning `gp::Optional<PathBuffer>`.
    /// @param[in] targetPath Target path view to make relative.
    /// @param[in] basePath Base directory view.
    /// @param[in] caseSensitive If false (default), compares path components case-insensitively.
    /// @return `Optional<PathBuffer>` containing the relative path, or `gp::nullopt` if on incompatible roots/drives.
    [[nodiscard]] static constexpr gp::Optional<PathBuffer> computeRelativePath(
        const gp::StringView targetPath, const gp::StringView basePath, const bool caseSensitive = false
    ) noexcept
    {
        PathBuffer buffer = clampToPathBuffer(targetPath);
        if (!makePathRelativeTo(buffer, basePath, caseSensitive))
        {
            return gp::nullopt;
        }
        return gp::Optional<PathBuffer>(buffer);
    }

    /// @brief Resolves an XDG directory according to the freedesktop.org XDG Base Directory Specification (`constexpr`
    ///        pure lexical helper).
    /// @details Per the XDG specification, if `xdgEnvValue` is empty or relative, it must be ignored in favor of
    ///          `combine(homeDirectory, fallbackRelativeSubpath)` with a trailing `'/'`.
    /// @param[in] xdgEnvValue Value of the `$XDG_*` environment variable (may be empty or relative).
    /// @param[in] homeDirectory Normalized user `$HOME` directory path.
    /// @param[in] fallbackRelativeSubpath Relative fallback path under `homeDirectory` (e.g., `".local/share"`).
    /// @return Normalized directory `PathBuffer` guaranteed to end with `'/'`.
    [[nodiscard]] static constexpr PathBuffer resolveXdgDirectory(
        const gp::StringView xdgEnvValue,
        const gp::StringView homeDirectory,
        const gp::StringView fallbackRelativeSubpath
    ) noexcept
    {
        PathBuffer result{};
        if (!xdgEnvValue.isEmpty() && isAbsolute(xdgEnvValue))
        {
            result = clampToPathBuffer(xdgEnvValue);
        }
        else
        {
            result = combine(homeDirectory, fallbackRelativeSubpath);
        }

        makeStandardFilename(result);
        ensureTrailingSeparator(result);
        return result;
    }

    /// @brief Parses a freedesktop.org `user-dirs.dirs` configuration buffer for a specific key such as
    ///        `"XDG_DOCUMENTS_DIR"` (`constexpr` pure lexical helper).
    /// @details Ignores comment lines (`#`), strips surrounding double quotes, expands leading `"$HOME/"` or
    ///          `"${HOME}/"` tokens against `homeDirectory`, and rejects relative paths per XDG rules.
    /// @param[in] configContent Full text contents of `$XDG_CONFIG_HOME/user-dirs.dirs`.
    /// @param[in] keyName Target variable name (e.g., `"XDG_DOCUMENTS_DIR"`).
    /// @param[in] homeDirectory Normalized user `$HOME` directory path.
    /// @return Normalized directory `PathBuffer` with trailing `'/'` if found and valid, or empty `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer parseXdgUserDirsEntry(
        const gp::StringView configContent, const gp::StringView keyName, const gp::StringView homeDirectory
    ) noexcept
    {
        if (configContent.isEmpty() || keyName.isEmpty())
        {
            return {};
        }

        gp::USize pos = 0u;
        while (pos < configContent.size())
        {
            gp::USize lineEnd = pos;
            while (lineEnd < configContent.size() && configContent[lineEnd] != '\n' && configContent[lineEnd] != '\r')
            {
                ++lineEnd;
            }

            gp::StringView line = configContent.substr(pos, lineEnd - pos);
            pos = lineEnd;
            while (pos < configContent.size() && (configContent[pos] == '\n' || configContent[pos] == '\r'))
            {
                ++pos;
            }

            // Trim leading whitespace
            gp::USize start = 0u;
            while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
            {
                ++start;
            }
            line = line.substr(start);

            if (line.isEmpty() || line.front() == '#')
            {
                continue;
            }

            if (line.size() > keyName.size() && line.startsWith(keyName) && line[keyName.size()] == '=')
            {
                gp::StringView rawVal = line.substr(keyName.size() + 1u);

                // Trim trailing whitespace
                gp::USize valEnd = rawVal.size();
                while (valEnd > 0u && (rawVal[valEnd - 1u] == ' ' || rawVal[valEnd - 1u] == '\t'))
                {
                    --valEnd;
                }
                rawVal = rawVal.substr(0u, valEnd);

                // Strip surrounding double quotes
                if (rawVal.size() >= 2u && rawVal.front() == '"' && rawVal.back() == '"')
                {
                    rawVal = rawVal.substr(1u, rawVal.size() - 2u);
                }

                PathBuffer resolved{};
                if (rawVal == "$HOME" || rawVal == "${HOME}")
                {
                    resolved = clampToPathBuffer(homeDirectory);
                }
                else if (rawVal.startsWith("$HOME/"))
                {
                    resolved = combine(homeDirectory, rawVal.substr(6u));
                }
                else if (rawVal.startsWith("${HOME}/"))
                {
                    resolved = combine(homeDirectory, rawVal.substr(8u));
                }
                else if (isAbsolute(rawVal))
                {
                    resolved = clampToPathBuffer(rawVal);
                }

                if (!resolved.isEmpty() && isAbsolute(resolved.view()))
                {
                    makeStandardFilename(resolved);
                    ensureTrailingSeparator(resolved);
                    return resolved;
                }
            }
        }

        return {};
    }

    /// @brief Resolves the macOS `.app` bundle `Contents/Resources/` directory from an executable directory path
    ///        (`constexpr` pure lexical helper).
    /// @details If `executableDirectory` ends with `".app/Contents/MacOS"` (or `".app/Contents/MacOS/"`), maps it to
    ///          `".app/Contents/Resources/"`. Otherwise returns `executableDirectory` normalized with a trailing `'/'`.
    /// @param[in] executableDirectory Path to the directory containing the running executable.
    /// @return Normalized directory `PathBuffer` guaranteed to end with `'/'`.
    [[nodiscard]] static constexpr PathBuffer
        resolveMacOsBundleResourcesDir(const gp::StringView executableDirectory) noexcept
    {
        if (executableDirectory.isEmpty())
        {
            return {};
        }

        PathBuffer stdDir = createStandardFilename(executableDirectory);
        const gp::StringView view = stdDir.view();
        constexpr gp::StringView kMacOsSuffix = ".app/Contents/MacOS";

        if (view.size() > kMacOsSuffix.size())
        {
            const gp::StringView tail = view.substr(view.size() - kMacOsSuffix.size());
            if (compareCaseInsensitive(tail, kMacOsSuffix) == 0)
            {
                constexpr gp::USize kMacOsTokenLen = 5u;   // "MacOS"
                PathBuffer resourcesDir = clampToPathBuffer(view.substr(0u, view.size() - kMacOsTokenLen));
                appendClamped(resourcesDir, "Resources/");
                return resourcesDir;
            }
        }

        ensureTrailingSeparator(stdDir);
        return stdDir;
    }

public:
    /// @section Path Validation & Filename Sanitization (`constexpr`)

    /// @brief Returns the set of characters forbidden inside individual file or directory names across platforms.
    /// @return `StringView` containing `"\"*:<>?\\|/"`.
    [[nodiscard]] static constexpr gp::StringView getInvalidFileSystemChars() noexcept
    {
        return "\"*:<>?\\|/";
    }

    /// @brief Checks whether a filename stem (without extension) matches a reserved Windows/DOS device name.
    /// @details Matches `CON`, `PRN`, `AUX`, `NUL`, `CLOCK$`, `COM1`..`COM9`, and `LPT1`..`LPT9` case-insensitively.
    /// @param[in] stem Filename or component stem (excluding extension).
    /// @return True if `stem` is a reserved Windows device name, false otherwise.
    [[nodiscard]] static constexpr bool isReservedWindowsDeviceName(const gp::StringView stem) noexcept
    {
        if (stem.size() < 3u || stem.size() > 6u)
        {
            return false;
        }

        if (stem.size() == 3u)
        {
            if (compareCaseInsensitive(stem, "CON") == 0 || compareCaseInsensitive(stem, "PRN") == 0 ||
                compareCaseInsensitive(stem, "AUX") == 0 || compareCaseInsensitive(stem, "NUL") == 0)
            {
                return true;
            }
        }
        else if (stem.size() == 4u)
        {
            const gp::StringView prefix = stem.substr(0u, 3u);
            const char digit = stem[3];
            if (digit >= '1' && digit <= '9')
            {
                if (compareCaseInsensitive(prefix, "COM") == 0 || compareCaseInsensitive(prefix, "LPT") == 0)
                {
                    return true;
                }
            }
        }
        else if (stem.size() == 6u)
        {
            if (compareCaseInsensitive(stem, "CLOCK$") == 0)
            {
                return true;
            }
        }

        return false;
    }

    /// @brief Sanitizes a candidate filename by replacing or stripping illegal characters, trailing dots/spaces, and
    ///        reserved Windows device names (including compound extensions like `"NUL.tar.gz"`).
    /// @param[in] fileName Raw candidate filename view.
    /// @param[in] replacementChar Replacement character for invalid symbols (if `'\0'`, invalid chars are omitted; if
    ///                            itself illegal, falls back to `'_'`).
    /// @return Sanitized `PathBuffer` safe for file creation on Windows, Linux, macOS, and consoles.
    [[nodiscard]] static constexpr PathBuffer
        makeValidFileName(const gp::StringView fileName, const char replacementChar = '_') noexcept
    {
        if (fileName.isEmpty())
        {
            return {};
        }

        const gp::StringView invalidChars = getInvalidFileSystemChars();
        char safeReplacement = replacementChar;
        if (safeReplacement != '\0')
        {
            const gp::UInt8 repByte = static_cast<gp::UInt8>(safeReplacement);
            if (repByte < 32u || safeReplacement == ' ' || safeReplacement == '.' ||
                invalidChars.contains(safeReplacement))
            {
                safeReplacement = '_';
            }
        }

        PathBuffer output{};
        for (gp::USize i = 0u; i < fileName.size(); ++i)
        {
            const char ch = fileName[i];
            const gp::UInt8 byteVal = static_cast<gp::UInt8>(ch);
            const bool isIllegal = (byteVal < 32u) || invalidChars.contains(ch);

            if (isIllegal)
            {
                if (safeReplacement != '\0' && output.size() < output.capacity())
                {
                    output.pushBack(safeReplacement);
                }
            }
            else if (output.size() < output.capacity())
            {
                output.pushBack(ch);
            }
        }

        // Strip trailing spaces and trailing periods (forbidden on Win32/NTFS)
        while (!output.isEmpty() && (output.back() == ' ' || output.back() == '.'))
        {
            output.popBack();
        }

        if (output.isEmpty())
        {
            output.pushBack((safeReplacement != '\0') ? safeReplacement : '_');
            return output;
        }

        // Check both the primary stem (before first dot, e.g. "NUL.tar.gz") and single-extension stem
        const gp::StringView compoundStem = getBaseFilename(output.view(), true, PathFlags::AllowCompoundExtension);
        const gp::StringView singleStem = getBaseFilename(output.view(), true, PathFlags::None);
        if (isReservedWindowsDeviceName(compoundStem) || isReservedWindowsDeviceName(singleStem))
        {
            PathBuffer prefixed("_");
            appendClamped(prefixed, output.view());
            output = prefixed;
        }

        return output;
    }

    /// @brief Validates whether `path` is lexically legal across all supported desktop and console filesystems.
    /// @param[in] path Input path view to validate.
    /// @param[out] outError Optional pointer receiving the specific `PathValidationError` reason code.
    /// @return True if `path` is valid (`PathValidationError::None`), false otherwise.
    [[nodiscard]] static constexpr bool
        validatePath(const gp::StringView path, PathValidationError* const outError = nullptr) noexcept
    {
        const auto setError = [outError](const PathValidationError err) constexpr noexcept -> bool
        {
            if (outError != nullptr)
            {
                *outError = err;
            }
            return err == PathValidationError::None;
        };

        if (path.isEmpty())
        {
            return setError(PathValidationError::EmptyPath);
        }

        if (path.size() > kMaxPathLength)
        {
            return setError(PathValidationError::ExceedsMaxLength);
        }

        bool isExtendedUnc = false;
        const gp::StringView effectivePath = stripExtendedLengthPrefix(path, isExtendedUnc);
        if (effectivePath.isEmpty())
        {
            return setError(PathValidationError::EmptyPath);
        }

        if (!isExtendedUnc && isDriveSpecifierWithoutRoot(effectivePath))
        {
            return setError(PathValidationError::UnrootedDriveSpecifier);
        }

        gp::StringView volume{};
        gp::StringView remainder{};
        if (isExtendedUnc)
        {
            remainder = effectivePath;
        }
        else
        {
            splitVolumeSpecifier(effectivePath, volume, remainder);
        }

        // Validate characters inside volume specifier (if any)
        for (gp::USize i = 0u; i < volume.size(); ++i)
        {
            const gp::UInt8 byteVal = static_cast<gp::UInt8>(volume[i]);
            if (byteVal < 32u || volume[i] == '"' || volume[i] == '*' || volume[i] == '<' || volume[i] == '>' ||
                volume[i] == '?' || volume[i] == '|')
            {
                return setError(PathValidationError::InvalidCharacter);
            }
        }

        // Walk each component in remainder
        gp::USize pos = 0u;
        while (pos < remainder.size())
        {
            while (pos < remainder.size() && isSeparator(remainder[pos]))
            {
                ++pos;
            }
            if (pos >= remainder.size())
            {
                break;
            }

            const gp::USize compStart = pos;
            while (pos < remainder.size() && !isSeparator(remainder[pos]))
            {
                ++pos;
            }

            const gp::StringView comp = remainder.substr(compStart, pos - compStart);
            for (gp::USize i = 0u; i < comp.size(); ++i)
            {
                const char ch = comp[i];
                const gp::UInt8 byteVal = static_cast<gp::UInt8>(ch);
                if (byteVal < 32u || ch == '"' || ch == '*' || ch == ':' || ch == '<' || ch == '>' || ch == '?' ||
                    ch == '|')
                {
                    return setError(PathValidationError::InvalidCharacter);
                }
            }

            if (comp != "." && comp != "..")
            {
                if (comp.back() == ' ' || comp.back() == '.')
                {
                    return setError(PathValidationError::TrailingDotOrSpace);
                }

                const gp::StringView compoundStem = getBaseFilename(comp, true, PathFlags::AllowCompoundExtension);
                const gp::StringView singleStem = getBaseFilename(comp, true, PathFlags::None);
                if (isReservedWindowsDeviceName(compoundStem) || isReservedWindowsDeviceName(singleStem))
                {
                    return setError(PathValidationError::ReservedDeviceName);
                }
            }
        }

        return setError(PathValidationError::None);
    }

public:
    /// @section Zero-Allocation Component & Ancestor Iteration (`constexpr`)

    /// @brief Iterates forward over every non-empty path component in `path`, invoking `visitor(componentView)`.
    /// @tparam InVisitor Callable accepting `gp::StringView` (returning `void` or `bool`).
    /// @param[in] path Input path view to traverse.
    /// @param[in] visitor Callback invoked for each component in forward order.
    template <typename InVisitor>
    static constexpr void iterateComponents(const gp::StringView path, InVisitor&& visitor) noexcept
    {
        gp::USize pos = 0u;
        while (pos < path.size())
        {
            while (pos < path.size() && isSeparator(path[pos]))
            {
                ++pos;
            }
            if (pos >= path.size())
            {
                break;
            }

            const gp::USize start = pos;
            while (pos < path.size() && !isSeparator(path[pos]))
            {
                ++pos;
            }

            const gp::StringView component = path.substr(start, pos - start);
            if constexpr (std::is_same_v<decltype(visitor(component)), bool>)
            {
                if (!visitor(component))
                {
                    return;
                }
            }
            else
            {
                visitor(component);
            }
        }
    }

    /// @brief Iterates upward over `path` and each of its parent directory views from leaf to root.
    /// @details Invokes `visitor(ancestorView)` starting with `path` (trimmed of redundant trailing slashes) and
    ///          stepping upward via `getPath()` until the root is reached or `visitor` returns `false`.
    /// @tparam InVisitor Callable accepting `gp::StringView` and returning `bool` (or `void`).
    /// @param[in] path Starting path view.
    /// @param[in] visitor Callback invoked for each ancestor path view.
    /// @return True if iteration completed to the root, false if terminated early by `visitor` returning `false`.
    template <typename InVisitor>
    static constexpr bool iterateAncestors(const gp::StringView path, InVisitor&& visitor) noexcept
    {
        if (path.isEmpty())
        {
            return true;
        }

        gp::StringView current = trimRedundantTerminatingSeparator(path);

        while (!current.isEmpty())
        {
            if constexpr (std::is_same_v<decltype(visitor(current)), bool>)
            {
                if (!visitor(current))
                {
                    return false;
                }
            }
            else
            {
                visitor(current);
            }

            const gp::StringView parent = getPath(current);
            if (parent.size() == current.size())
            {
                break;
            }
            current = parent;
        }

        return true;
    }

protected:
    /// @section Internal Override & Cache Generation Helpers for Derived Platform Classes

    /// @brief Returns the active testing override for `executablePath()`, or an empty view if none is set.
    [[nodiscard]] static gp::StringView tryGetExecutablePathOverride() noexcept;

    /// @brief Returns the active testing or `-basedir=` CLI override for `baseDir()`, or an empty view if none is set.
    [[nodiscard]] static gp::StringView tryGetBaseDirOverride() noexcept;

    /// @brief Returns the active testing or `-appdata=` CLI override for `appDataDir()`, or an empty view if none is
    ///        set.
    [[nodiscard]] static gp::StringView tryGetAppDataDirOverride() noexcept;

    /// @brief Returns the active testing or `-userdir=` CLI override for `documentsDir()`, or an empty view if none is
    ///        set.
    [[nodiscard]] static gp::StringView tryGetDocumentsDirOverride() noexcept;

    /// @brief Returns the monotonic cache generation counter incremented by `resetCachesForTesting()`.
    [[nodiscard]] static gp::UInt32 getCacheGeneration() noexcept;

    /// @brief Acquires the global `.bss` path cache spinlock for thread-safe lazy initialization and override updates.
    static void acquireCacheLock() noexcept;

    /// @brief Releases the global `.bss` path cache spinlock.
    static void releaseCacheLock() noexcept;

    /// @brief Scoped RAII guard for the `.bss` path cache spinlock.
    class ScopedCacheLock final
    {
    public:
        /// @brief Acquires the cache spinlock upon construction.
        ScopedCacheLock() noexcept
        {
            Paths::acquireCacheLock();
        }

        /// @brief Releases the cache spinlock upon destruction.
        ~ScopedCacheLock() noexcept
        {
            Paths::releaseCacheLock();
        }

        ScopedCacheLock(const ScopedCacheLock&) = delete;
        ScopedCacheLock& operator=(const ScopedCacheLock&) = delete;
        ScopedCacheLock(ScopedCacheLock&&) = delete;
        ScopedCacheLock& operator=(ScopedCacheLock&&) = delete;
    };

private:
    /// @section Internal Bounds-Safety & Zero-Allocation Fast-Path Helpers (`constexpr`)

    /// @brief Constructs a `PathBuffer` from `view`, clamping its length to `kMaxPathLength` (`1024` bytes).
    /// @param[in] view Input string view.
    /// @return Capacity-safe `PathBuffer`.
    [[nodiscard]] static constexpr PathBuffer clampToPathBuffer(const gp::StringView view) noexcept
    {
        const gp::USize clampedLen = (view.size() <= kMaxPathLength) ? view.size() : kMaxPathLength;
        return PathBuffer(view.substr(0u, clampedLen));
    }

    /// @brief Appends `view` to `inOutBuffer`, clamping to the remaining capacity of `inOutBuffer`.
    /// @param[in,out] inOutBuffer Target path buffer.
    /// @param[in] view String view to append.
    static constexpr void appendClamped(PathBuffer& inOutBuffer, const gp::StringView view) noexcept
    {
        if (view.isEmpty() || inOutBuffer.size() >= inOutBuffer.capacity())
        {
            return;
        }

        const gp::USize avail = inOutBuffer.capacity() - inOutBuffer.size();
        const gp::USize copyLen = (view.size() <= avail) ? view.size() : avail;
        inOutBuffer.append(view.substr(0u, copyLen));
    }

    /// @brief Strips a leading Windows extended-length path prefix (`"\\?\"` or `"\\?\UNC\"`) if present.
    /// @param[in] path Input path view.
    /// @param[out] outIsExtendedUnc Set to true if `path` started with `"\\?\UNC\"`.
    /// @return Sub-view following the extended-length prefix, or `path` unchanged.
    [[nodiscard]] static constexpr gp::StringView
        stripExtendedLengthPrefix(const gp::StringView path, bool& outIsExtendedUnc) noexcept
    {
        outIsExtendedUnc = false;
        if (path.size() >= 4u && isSeparator(path[0]) && isSeparator(path[1]) && path[2] == '?' && isSeparator(path[3]))
        {
            if (path.size() >= 8u && toUpperAscii(path[4]) == 'U' && toUpperAscii(path[5]) == 'N' &&
                toUpperAscii(path[6]) == 'C' && isSeparator(path[7]))
            {
                outIsExtendedUnc = true;
                return path.substr(8u);
            }
            if (path.size() >= 6u && isAsciiAlpha(path[4]) && path[5] == ':')
            {
                return path.substr(4u);
            }
        }
        return path;
    }

    /// @brief Returns a sub-view of `path` with any redundant trailing separators removed.
    /// @param[in] path Input path view.
    /// @return Trimmed `StringView`.
    [[nodiscard]] static constexpr gp::StringView trimRedundantTerminatingSeparator(const gp::StringView path) noexcept
    {
        gp::StringView trimmed = path;
        if (hasRedundantTerminatingSeparator(trimmed))
        {
            gp::USize endPos = trimmed.size();
            while (endPos > 0u && isSeparator(trimmed[endPos - 1u]))
            {
                --endPos;
            }
            trimmed = trimmed.substr(0u, endPos);
        }
        return trimmed;
    }

    /// @brief Checks if `path` can be compared directly via `StringView` without allocating a `PathBuffer` to collapse
    ///        relative `"."` / `".."` segments or duplicate slashes.
    /// @param[in] path Input path view.
    /// @return True if `path` has no `"."` / `".."` segments, no duplicate slashes, and no Win32 extended prefix.
    [[nodiscard]] static constexpr bool canCompareDirectlyWithoutCollapsing(const gp::StringView path) noexcept
    {
        if (path.size() > kMaxPathLength)
        {
            return false;
        }

        bool isExtUnc = false;
        if (stripExtendedLengthPrefix(path, isExtUnc).size() != path.size())
        {
            return false;
        }

        for (gp::USize i = 0u; i < path.size(); ++i)
        {
            if (isSeparator(path[i]) && i > 0u && isSeparator(path[i - 1u]))
            {
                const bool isUncPrefix = (i == 1u) && (path.size() == 2u || !isSeparator(path[2]));
                const bool isUriScheme = (i >= 2u) && (path[i - 2u] == ':') && (i == 2u || !isSeparator(path[i - 3u]));
                if (!isUncPrefix && !isUriScheme)
                {
                    return false;
                }
            }

            if (path[i] == '.' && (i == 0u || isSeparator(path[i - 1u])))
            {
                if ((i + 1u) == path.size() || isSeparator(path[i + 1u]))
                {
                    return false;
                }
                if (path[i + 1u] == '.' && ((i + 2u) == path.size() || isSeparator(path[i + 2u])))
                {
                    return false;
                }
            }
        }

        return true;
    }

    /// @brief Compares two pre-vetted path views character-by-character, treating `'/'` and `'\\'` as identical.
    /// @param[in] lhs First trimmed path view.
    /// @param[in] rhs Second trimmed path view.
    /// @param[in] caseSensitive If false, compares ASCII characters case-insensitively.
    /// @return Negative if `lhs < rhs`, zero if equal, positive if `lhs > rhs`.
    [[nodiscard]] static constexpr gp::Int32 compareDirectNormalizedViews(
        const gp::StringView lhs, const gp::StringView rhs, const bool caseSensitive
    ) noexcept
    {
        const gp::USize minLen = (lhs.size() < rhs.size()) ? lhs.size() : rhs.size();
        for (gp::USize i = 0u; i < minLen; ++i)
        {
            const char rawL = isSeparator(lhs[i]) ? kPreferredSeparator : lhs[i];
            const char rawR = isSeparator(rhs[i]) ? kPreferredSeparator : rhs[i];
            const gp::UInt8 a = static_cast<gp::UInt8>(caseSensitive ? rawL : toLowerAscii(rawL));
            const gp::UInt8 b = static_cast<gp::UInt8>(caseSensitive ? rawR : toLowerAscii(rawR));
            if (a != b)
            {
                return (a < b) ? -1 : 1;
            }
        }

        if (lhs.size() < rhs.size())
        {
            return -1;
        }
        if (lhs.size() > rhs.size())
        {
            return 1;
        }
        return 0;
    }
};

}   // namespace gp::platform::generic
