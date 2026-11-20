// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::platform
{

/// @brief Explicit diagnostic error codes returned by Layer 0 `gp::platform::FileSystem` and `FileHandle` (ADR-0060).
enum class FileError : gp::UInt8
{
    /// @brief Operation completed without error.
    None = 0,

    /// @brief The requested file or directory does not exist on the filesystem.
    NotFound = 1,

    /// @brief The target file or directory already exists when an exclusive creation mode was requested.
    AlreadyExists = 2,

    /// @brief The operating system denied access due to read-only flags, file locks, or insufficient privileges.
    AccessDenied = 3,

    /// @brief The supplied path is empty, exceeds `kMaxPathLength`, or fails lexical validation.
    InvalidPath = 4,

    /// @brief An invalid parameter (e.g., null buffer with non-zero size or unaligned/negative offset) was provided.
    InvalidArgument = 5,

    /// @brief The `FileHandle` is closed or holds an invalid native OS descriptor.
    InvalidHandle = 6,

    /// @brief A directory was encountered where a regular file was expected (e.g., `openRead` on a folder).
    IsDirectory = 7,

    /// @brief A regular file was encountered where a directory was expected (e.g., `iterateDirectory` on a file).
    NotDirectory = 8,

    /// @brief Attempted to remove a directory that still contains child files or subdirectories.
    DirectoryNotEmpty = 9,

    /// @brief The destination storage volume has insufficient free space to complete the write.
    DiskFull = 10,

    /// @brief The file is currently locked or opened with incompatible sharing flags by another process or handle.
    SharingViolation = 11,

    /// @brief The process or system has reached its maximum open file descriptor limit (`EMFILE` / `ENFILE`).
    TooManyOpenFiles = 12,

    /// @brief Attempted to perform an atomic rename across distinct physical volumes/mount points (`EXDEV`).
    CrossDeviceLink = 13,

    /// @brief Directory recursion depth exceeded `kMaxDirectoryRecursionDepth` (`32`).
    MaxRecursionDepthExceeded = 14,

    /// @brief The target filesystem or mount point is read-only (`EROFS`).
    ReadOnlyFileSystem = 15,

    /// @brief A hardware, driver, or kernel I/O transfer failure occurred (`EIO`).
    IoFailure = 16,

    /// @brief The operation is not supported on the current platform or fallback backend.
    NotSupported = 17
};

/// @brief Read/write permission modes requested when opening an OS `FileHandle` (ADR-0060).
enum class FileAccessMode : gp::UInt8
{
    /// @brief Open the file strictly for reading (`GENERIC_READ` / `O_RDONLY`).
    Read = 0x01,

    /// @brief Open the file strictly for writing (`GENERIC_WRITE` / `O_WRONLY`).
    Write = 0x02,

    /// @brief Open the file for both reading and writing (`GENERIC_READ | GENERIC_WRITE` / `O_RDWR`).
    ReadWrite = Read | Write
};

/// @brief Creation and truncation disposition when opening an OS `FileHandle` (ADR-0060).
enum class FileOpenMode : gp::UInt8
{
    /// @brief Opens an existing file; fails with `FileError::NotFound` if the file does not exist (`OPEN_EXISTING`).
    OpenExisting = 0,

    /// @brief Creates a new file or truncates an existing file to zero bytes (`CREATE_ALWAYS` / `O_CREAT | O_TRUNC`).
    CreateAlways = 1,

    /// @brief Creates a new file; fails with `FileError::AlreadyExists` if it already exists (`CREATE_NEW` / `O_EXCL`).
    CreateNew = 2,

    /// @brief Opens the file if it exists or creates a new file if it does not (`OPEN_ALWAYS` / `O_CREAT`).
    OpenAlways = 3,

    /// @brief Opens or creates a file and positions all sequential writes at the end (`O_CREAT | O_APPEND`).
    Append = 4
};

/// @brief Concurrent access sharing flags granted to other handles while a `FileHandle` is open (ADR-0060).
enum class FileShareMode : gp::UInt8
{
    /// @brief Exclusive access: prevents other openers from reading, writing, or deleting the file.
    None = 0x00,

    /// @brief Allows other handles to read the file concurrently (`FILE_SHARE_READ`).
    Read = 0x01,

    /// @brief Allows other handles to write to the file concurrently (`FILE_SHARE_WRITE`).
    Write = 0x02,

    /// @brief Allows other handles to delete or rename the file while open (`FILE_SHARE_DELETE`).
    Delete = 0x04,

    /// @brief Default read-write sharing mask (`Read | Write`).
    ReadWrite = Read | Write,

    /// @brief Full POSIX-like sharing semantics (`Read | Write | Delete`).
    All = Read | Write | Delete
};

/// @brief Reference origin for `FileHandle::seek()` cursor repositioning (ADR-0060).
enum class FileSeekOrigin : gp::UInt8
{
    /// @brief Offset is measured relative to the beginning of the file (`FILE_BEGIN` / `SEEK_SET`).
    Begin = 0,

    /// @brief Offset is measured relative to the current cursor position (`FILE_CURRENT` / `SEEK_CUR`).
    Current = 1,

    /// @brief Offset is measured relative to the end of the file (`FILE_END` / `SEEK_END`).
    End = 2
};

/// @brief Bitmask attributes describing a filesystem entry in `FileStats` (ADR-0060).
enum class FileAttributeFlags : gp::UInt8
{
    /// @brief No valid attributes (or entry does not exist).
    None = 0x00,

    /// @brief Entry is a standard data file.
    RegularFile = 0x01,

    /// @brief Entry is a directory/folder.
    Directory = 0x02,

    /// @brief Entry is marked read-only (writes and deletions without attribute clearing will fail).
    ReadOnly = 0x04,

    /// @brief Entry is marked hidden (Win32 `FILE_ATTRIBUTE_HIDDEN` or dot-prefixed `".name"` on POSIX).
    Hidden = 0x08,

    /// @brief Entry is a symbolic link or NTFS reparse point.
    SymbolicLink = 0x10
};

/// @brief Flow control directive returned by directory iteration callbacks (ADR-0060).
enum class DirectoryVisitControl : gp::UInt8
{
    /// @brief Continue visiting remaining entries (and descend into subdirectories during recursive traversal).
    Continue = 0,

    /// @brief During `iterateDirectoryRecursive`, do not descend into the current directory entry, but continue
    /// siblings.
    SkipSubdirectory = 1,

    /// @brief Immediately terminate directory traversal and return success to the caller.
    Stop = 2
};

/// @brief Returns a human-readable ASCII name for a `FileError` enum value (for diagnostics and logging).
/// @param[in] error The `FileError` value to stringify.
/// @return Null-terminated static string view naming the error code.
[[nodiscard]] constexpr gp::StringView toString(const FileError error) noexcept
{
    switch (error)
    {
    case FileError::None:
        return "None";
    case FileError::NotFound:
        return "NotFound";
    case FileError::AlreadyExists:
        return "AlreadyExists";
    case FileError::AccessDenied:
        return "AccessDenied";
    case FileError::InvalidPath:
        return "InvalidPath";
    case FileError::InvalidArgument:
        return "InvalidArgument";
    case FileError::InvalidHandle:
        return "InvalidHandle";
    case FileError::IsDirectory:
        return "IsDirectory";
    case FileError::NotDirectory:
        return "NotDirectory";
    case FileError::DirectoryNotEmpty:
        return "DirectoryNotEmpty";
    case FileError::DiskFull:
        return "DiskFull";
    case FileError::SharingViolation:
        return "SharingViolation";
    case FileError::TooManyOpenFiles:
        return "TooManyOpenFiles";
    case FileError::CrossDeviceLink:
        return "CrossDeviceLink";
    case FileError::MaxRecursionDepthExceeded:
        return "MaxRecursionDepthExceeded";
    case FileError::ReadOnlyFileSystem:
        return "ReadOnlyFileSystem";
    case FileError::IoFailure:
        return "IoFailure";
    case FileError::NotSupported:
        return "NotSupported";
    default:
        return "Unknown";
    }
}

}   // namespace gp::platform

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::platform::FileAccessMode);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::platform::FileShareMode);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::platform::FileAttributeFlags);
