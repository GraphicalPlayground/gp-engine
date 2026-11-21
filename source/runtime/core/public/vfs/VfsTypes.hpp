// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/FixedString.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileTypes.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "vfs/VfsEnums.hpp"

namespace gp::vfs
{

/// @brief Maximum UTF-8 byte length for a normalized VFS scheme prefix including `"://"` (e.g., `"appdata://"`).
inline constexpr gp::USize kMaxSchemeLength = 32u;

/// @brief Maximum number of concurrent mount points registered in `VirtualFileSystem` (zero-allocation `.bss` table).
inline constexpr gp::USize kMaxMountPoints = 32u;

/// @brief Number of open-addressed hash slots (`512` slots = `4 KiB` on stack) used for O(1) shadowed-entry
///        deduplication during multi-mount VFS directory iteration (ADR-0044, ADR-0060).
inline constexpr gp::USize kVfsMergedDirectoryHashSlots = 512u;

/// @brief Canonical logical URI schemes defined by ADR-0044.
inline constexpr gp::StringView kEngineScheme = "engine://";
inline constexpr gp::StringView kGameScheme = "game://";
inline constexpr gp::StringView kUserScheme = "user://";
inline constexpr gp::StringView kAppDataScheme = "appdata://";
inline constexpr gp::StringView kCacheScheme = "cache://";
inline constexpr gp::StringView kTempScheme = "temp://";

/// @brief Stack-allocated buffer holding a normalized lowercase URI scheme prefix (e.g., `"game://"`).
using SchemeBuffer = gp::FixedString<kMaxSchemeLength>;

/// @brief Result of decomposing and sandbox-validating a logical VFS URI (ADR-0044).
struct ParsedUri
{
public:
    /// @brief Normalized lowercase scheme prefix including `"://"` (e.g., `"game://"`).
    SchemeBuffer scheme{};

    /// @brief Sandbox-validated relative path inside the scheme with all `"."` and `".."` segments collapsed
    ///        (e.g., `"textures/hero.ktx2"`, or empty for the root of the scheme `"game://"`).
    gp::PathBuffer relativePath{};

public:
    /// @brief Checks whether this parsed URI refers to the bare root of the scheme (e.g., `"game://"`).
    /// @return True if `relativePath` is empty, false otherwise.
    [[nodiscard]] constexpr bool isSchemeRoot() const noexcept
    {
        return relativePath.isEmpty();
    }
};

/// @brief Descriptor snapshot describing a registered mount point inside `VirtualFileSystem` (ADR-0044).
struct MountPointDesc
{
public:
    /// @brief Normalized lowercase URI scheme bound to this mount (e.g., `"game://"`).
    SchemeBuffer scheme{};

    /// @brief Physical OS root directory or logical backend identifier (e.g., `"C:/Game/Content/"` or `"[memory]"`).
    gp::PathBuffer rootPath{};

    /// @brief Overlay priority tier (`DeveloperOverride` > `ModOverride` > `Patch` > `DLC` > `Base`).
    MountPriority priority{ MountPriority::Base };

    /// @brief Access permission contract (`ReadOnly` vs. `ReadWrite`).
    MountAccess access{ MountAccess::ReadOnly };
};

/// @brief Non-owning stack view passed to VFS directory iteration visitors (ADR-0044, ADR-0060).
struct VfsDirectoryEntry
{
public:
    /// @brief Full logical VFS URI of the entry (e.g., `"game://textures/hero.ktx2"`).
    gp::StringView uri{};

    /// @brief Clean leaf filename or directory name without parent path components (e.g., `"hero.ktx2"`).
    gp::StringView name{};

    /// @brief Metadata snapshot (`size`, timestamps, `FileAttributeFlags`).
    gp::platform::FileStats stats{};

    /// @brief Priority of the winning mount backend that provided this entry.
    MountPriority sourcePriority{ MountPriority::Base };

    /// @brief Zero-based recursion depth relative to the queried VFS directory URI.
    gp::UInt32 depth{ 0u };

public:
    /// @brief Checks whether this VFS directory entry is a regular file.
    /// @return True if `stats.isFile()`, false otherwise.
    [[nodiscard]] constexpr bool isFile() const noexcept
    {
        return stats.isFile();
    }

    /// @brief Checks whether this VFS directory entry is a directory.
    /// @return True if `stats.isDirectory()`, false otherwise.
    [[nodiscard]] constexpr bool isDirectory() const noexcept
    {
        return stats.isDirectory();
    }
};

/// @brief Stateless C-style callback signature for zero-allocation VFS mount-table inspection.
/// @param[in] desc Stack-resident snapshot descriptor of an active mount point.
/// @param[in] userData Opaque caller-supplied context pointer.
/// @return True to continue visiting remaining mounts, false to stop early.
using MountPointVisitorFn = bool (*)(const MountPointDesc& desc, void* userData);

/// @brief Stateless C-style callback signature for zero-allocation VFS directory enumeration.
/// @param[in] entry Stack-resident VFS directory entry descriptor.
/// @param[in] userData Opaque caller-supplied context pointer.
/// @return `gp::platform::DirectoryVisitControl` (`Continue`, `SkipSubdirectory`, or `Stop`).
using VfsDirectoryVisitorFn = gp::platform::DirectoryVisitControl (*)(const VfsDirectoryEntry& entry, void* userData);

}   // namespace gp::vfs
