// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/Platform.hpp"       // IWYU pragma: keep
#include "platforms/base/system/FileEnums.hpp"
#include "templates/Enums.hpp"
#include "time/TimeTypes.hpp"

namespace gp::platform
{

/// @brief Maximum nested subdirectory depth permitted during stack-buffered recursive directory traversal (ADR-0060).
/// @details Bounded to `32` levels (~35 KiB maximum stack footprint across recursive `PathBuffer` frames) so recursive
///          directory queries remain safe inside `64 KiB` standard job-system fibers (ADR-0049).
inline constexpr gp::UInt32 kMaxDirectoryRecursionDepth = 32u;

/// @brief Default stack chunk buffer size (`16 KiB`) used by `FileSystem::copyFile` fallback streaming.
inline constexpr gp::USize kFileCopyChunkSize = 16384u;

#if GP_PLATFORM_WINDOWS
/// @brief Native operating system file handle representation (`void*` Win32 `HANDLE`).
using NativeFileDescriptor = void*;

/// @brief Sentinel value representing an unopened or closed Win32 `HANDLE` (`INVALID_HANDLE_VALUE`).
inline const NativeFileDescriptor kInvalidNativeFileDescriptor = reinterpret_cast<void*>(static_cast<gp::ISize>(-1));
#else
/// @brief Native operating system file handle representation (`gp::Int32` POSIX file descriptor).
using NativeFileDescriptor = gp::Int32;

/// @brief Sentinel value representing an unopened or closed POSIX file descriptor (`-1`).
inline constexpr NativeFileDescriptor kInvalidNativeFileDescriptor = -1;
#endif

/// @brief Plain-Old-Data (POD) metadata snapshot for a file or directory on disk (ADR-0060).
struct FileStats
{
public:
    /// @brief Logical size of the file in bytes (`0` for directories).
    gp::UInt64 size{ 0ULL };

    /// @brief Creation (birth) time in UTC nanoseconds since the Unix Epoch (`1970-01-01T00:00:00Z`).
    /// @note On POSIX filesystems where birth time is unavailable, falls back to `lastWriteTime`.
    gp::time::Timestamp creationTime{};

    /// @brief Last content modification time in UTC nanoseconds since the Unix Epoch (`1970-01-01T00:00:00Z`).
    gp::time::Timestamp lastWriteTime{};

    /// @brief Last access time in UTC nanoseconds since the Unix Epoch (`1970-01-01T00:00:00Z`).
    gp::time::Timestamp lastAccessTime{};

    /// @brief Bitmask of `FileAttributeFlags` (`RegularFile`, `Directory`, `ReadOnly`, `Hidden`, `SymbolicLink`).
    FileAttributeFlags attributes{ FileAttributeFlags::None };

public:
    /// @brief Checks whether this metadata snapshot represents a regular data file.
    /// @return True if `FileAttributeFlags::RegularFile` is set, false otherwise.
    [[nodiscard]] constexpr bool isFile() const noexcept
    {
        return gp::enums::hasAllFlags(attributes, FileAttributeFlags::RegularFile);
    }

    /// @brief Checks whether this metadata snapshot represents a directory.
    /// @return True if `FileAttributeFlags::Directory` is set, false otherwise.
    [[nodiscard]] constexpr bool isDirectory() const noexcept
    {
        return gp::enums::hasAllFlags(attributes, FileAttributeFlags::Directory);
    }

    /// @brief Checks whether this entry is marked read-only.
    /// @return True if `FileAttributeFlags::ReadOnly` is set, false otherwise.
    [[nodiscard]] constexpr bool isReadOnly() const noexcept
    {
        return gp::enums::hasAllFlags(attributes, FileAttributeFlags::ReadOnly);
    }

    /// @brief Checks whether this entry is marked hidden.
    /// @return True if `FileAttributeFlags::Hidden` is set, false otherwise.
    [[nodiscard]] constexpr bool isHidden() const noexcept
    {
        return gp::enums::hasAllFlags(attributes, FileAttributeFlags::Hidden);
    }

    /// @brief Checks whether this entry is a symbolic link or reparse point.
    /// @return True if `FileAttributeFlags::SymbolicLink` is set, false otherwise.
    [[nodiscard]] constexpr bool isSymbolicLink() const noexcept
    {
        return gp::enums::hasAllFlags(attributes, FileAttributeFlags::SymbolicLink);
    }

    /// @brief Compares two `FileStats` instances for exact equality.
    /// @param[in] other The stats struct to compare against.
    /// @return True if all fields match, false otherwise.
    [[nodiscard]] constexpr bool operator==(const FileStats& other) const noexcept = default;
};

/// @brief Non-owning stack view passed to directory iteration visitors during `FileSystem::iterateDirectory`
/// (ADR-0060).
/// @note Both `path` and `name` point directly into the active stack `PathBuffer` of the iteration frame. Callers that
///       wish to retain paths after the visitor callback returns must copy them into a `PathBuffer` or `gp::String`.
struct DirectoryEntry
{
public:
    /// @brief Normalized full or relative path to the entry (e.g., `"C:/Dir/Sub/File.txt"`, no trailing slash).
    gp::StringView path{};

    /// @brief Clean leaf filename or directory name without parent path components (e.g., `"File.txt"` or `"Sub"`).
    gp::StringView name{};

    /// @brief Pre-populated metadata snapshot for the entry (size, timestamps, and `FileAttributeFlags`).
    FileStats stats{};

    /// @brief Zero-based recursion depth relative to the root directory passed to `iterateDirectoryRecursive`.
    gp::UInt32 depth{ 0u };

public:
    /// @brief Convenience check returning true if this directory entry is a regular file.
    /// @return True if `stats.isFile()`, false otherwise.
    [[nodiscard]] constexpr bool isFile() const noexcept
    {
        return stats.isFile();
    }

    /// @brief Convenience check returning true if this directory entry is a subdirectory.
    /// @return True if `stats.isDirectory()`, false otherwise.
    [[nodiscard]] constexpr bool isDirectory() const noexcept
    {
        return stats.isDirectory();
    }
};

/// @brief Stateless C-style callback signature for zero-allocation directory iteration (ADR-0060).
/// @param[in] entry Stack-resident directory entry descriptor for the current item.
/// @param[in] userData Opaque caller-supplied context pointer.
/// @return `DirectoryVisitControl` instructing the iterator to `Continue`, `SkipSubdirectory`, or `Stop`.
using DirectoryVisitorFn = DirectoryVisitControl (*)(const DirectoryEntry& entry, void* userData);

}   // namespace gp::platform
