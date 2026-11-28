// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"

namespace gp::platform
{

/// @brief Stack buffer capacity used to null-terminate exported symbol names before OS lookup calls.
inline constexpr gp::USize kSharedLibraryProcNameStackCapacity = 512u;

/// @brief Sentinel directory index returned when a search directory is not registered.
inline constexpr gp::UInt32 kInvalidSearchDirectoryIndex = static_cast<gp::UInt32>(-1);

/// @brief Function pointer predicate that reports whether a filename is a dynamic library for the active platform.
using SharedLibraryFileFilterFn = bool (*)(gp::StringView fileName) noexcept;

/// @brief Configuration policy controlling search-path matching and directory indexing in a search registry.
struct SharedLibrarySearchPolicy
{
    /// @brief Default shared library extension including the leading dot (for example `".so"` or `".dylib"`).
    gp::StringView defaultExtension{};

    /// @brief Predicate used by `addDirectory()` to decide which files to index into the cache.
    /// @note If null, `addDirectory()` matches files whose extension equals `defaultExtension`.
    SharedLibraryFileFilterFn fileFilter{ nullptr };

    /// @brief True for case-sensitive path and filename comparisons (Linux/Unix); false for case-insensitive
    /// (macOS/Windows).
    bool caseSensitive{ true };
};

/// @brief Snapshot of a registered search directory and its stable index for lock-free disk probing.
struct SharedLibraryFallbackDir
{
    PathBuffer directory{};
    gp::UInt32 directoryIndex{ 0u };
};

/// @brief Lock-free snapshot of candidate full paths and fallback directories produced for one `getHandle()` lookup.
/// @details Built under the registry mutex and consumed after releasing the lock so OS loader calls and disk I/O
///          never run while holding an internal engine lock.
struct SharedLibraryCandidateBatch
{
    gp::InlineVector<PathBuffer, 16u> prioritizedCandidates{};
    gp::InlineVector<SharedLibraryFallbackDir, 8u> fallbackDirs{};
    bool hasActivePushedDir{ false };
};

}   // namespace gp::platform
