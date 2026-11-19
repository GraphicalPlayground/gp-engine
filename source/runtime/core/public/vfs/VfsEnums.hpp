// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/FileEnums.hpp"

namespace gp::vfs
{

/// @brief Explicit diagnostic error codes returned by Layer 1 `gp::vfs::VirtualFileSystem` and `IMountBackend`
///        (ADR-0044, ADR-0060).
enum class VfsError : gp::UInt8
{
    /// @brief Operation completed without error.
    None = 0,

    /// @brief The requested virtual file or directory does not exist in any mounted backend for the URI scheme.
    NotFound = 1,

    /// @brief The target virtual file or directory already exists when exclusive creation was requested.
    AlreadyExists = 2,

    /// @brief The operating system or VFS backend denied access (e.g., read-only physical file).
    AccessDenied = 3,

    /// @brief The URI lacks a valid `"scheme://"` prefix or has an empty/malformed relative sub-path.
    InvalidUri = 4,

    /// @brief No mount point is currently registered for the requested URI scheme (e.g., `"unknown://"`).
    SchemeNotMounted = 5,

    /// @brief Relative `".."` segments in the URI attempted to climb above the mount root (ADR-0044 Invariant 1).
    SandboxEscapeViolation = 6,

    /// @brief Attempted a write, directory creation, or deletion on a `MountAccess::ReadOnly` scheme (`engine://`,
    ///        `game://`).
    ReadOnlyMount = 7,

    /// @brief A directory was encountered where a regular file was expected.
    IsDirectory = 8,

    /// @brief A regular file was encountered where a directory was expected.
    NotDirectory = 9,

    /// @brief Attempted to remove a non-empty directory without recursive deletion.
    DirectoryNotEmpty = 10,

    /// @brief The mount table or in-memory backend capacity limit was reached.
    MountTableFull = 11,

    /// @brief An invalid argument (null pointer or invalid flags) was provided.
    InvalidArgument = 12,

    /// @brief The underlying storage medium is full.
    DiskFull = 13,

    /// @brief Directory recursion depth exceeded `kMaxDirectoryRecursionDepth` (`32`).
    MaxRecursionDepthExceeded = 14,

    /// @brief Attempted to move/rename a file across two different mount backends or schemes.
    CrossMountOperation = 15,

    /// @brief The operation is not supported by the target `IMountBackend` (e.g., raw OS `FileHandle` on a memory/pak
    ///        backend).
    NotSupported = 16,

    /// @brief Underlying Layer 0 OS I/O failure.
    IoFailure = 17
};

/// @brief Priority tier governing read overlay order when multiple mounts share one logical URI scheme (ADR-0044).
/// @details Higher numerical priority mounts are queried first during read and existence resolution
/// (`DeveloperOverride`
///          > `ModOverride` > `Patch` > `DLC` > `Base`).
enum class MountPriority : gp::UInt16
{
    /// @brief Shipped base engine or game archive/folder (`100`).
    Base = 100u,

    /// @brief Expansion pack or downloadable content mount (`200`).
    DLC = 200u,

    /// @brief Official title update or hotfix patch archive (`300`).
    Patch = 300u,

    /// @brief Community or user modification overlay (`400`).
    ModOverride = 400u,

    /// @brief Local developer loose-file override or live hot-reload folder (`500`).
    DeveloperOverride = 500u
};

/// @brief Access permission contract enforced on a VFS mount point (ADR-0044).
enum class MountAccess : gp::UInt8
{
    /// @brief Immutable mount point (`engine://`, `game://`, `.pak` archives): all mutation calls return
    ///        `VfsError::ReadOnlyMount`.
    ReadOnly = 0,

    /// @brief Writable mount point (`user://`, `appdata://`, `cache://`, `temp://`).
    ReadWrite = 1
};

/// @brief Translates a Layer 0 `gp::platform::FileError` into the corresponding Layer 1 `gp::vfs::VfsError`.
/// @param[in] fileError Layer 0 OS filesystem error code.
/// @return Equivalent Layer 1 `VfsError`.
[[nodiscard]] constexpr VfsError mapFileErrorToVfsError(const gp::platform::FileError fileError) noexcept
{
    switch (fileError)
    {
    case gp::platform::FileError::None:
        return VfsError::None;
    case gp::platform::FileError::NotFound:
        return VfsError::NotFound;
    case gp::platform::FileError::AlreadyExists:
        return VfsError::AlreadyExists;
    case gp::platform::FileError::AccessDenied:
    case gp::platform::FileError::SharingViolation:
        return VfsError::AccessDenied;
    case gp::platform::FileError::ReadOnlyFileSystem:
        return VfsError::ReadOnlyMount;
    case gp::platform::FileError::InvalidPath:
        return VfsError::InvalidUri;
    case gp::platform::FileError::InvalidArgument:
    case gp::platform::FileError::InvalidHandle:
        return VfsError::InvalidArgument;
    case gp::platform::FileError::IsDirectory:
        return VfsError::IsDirectory;
    case gp::platform::FileError::NotDirectory:
        return VfsError::NotDirectory;
    case gp::platform::FileError::DirectoryNotEmpty:
        return VfsError::DirectoryNotEmpty;
    case gp::platform::FileError::DiskFull:
        return VfsError::DiskFull;
    case gp::platform::FileError::MaxRecursionDepthExceeded:
        return VfsError::MaxRecursionDepthExceeded;
    case gp::platform::FileError::CrossDeviceLink:
        return VfsError::CrossMountOperation;
    case gp::platform::FileError::NotSupported:
        return VfsError::NotSupported;
    case gp::platform::FileError::TooManyOpenFiles:
    case gp::platform::FileError::IoFailure:
    default:
        return VfsError::IoFailure;
    }
}

/// @brief Returns a human-readable ASCII name for a `VfsError` enum value (for diagnostics and logging).
/// @param[in] error The `VfsError` value to stringify.
/// @return Null-terminated static string view naming the VFS error code.
[[nodiscard]] constexpr gp::StringView toString(const VfsError error) noexcept
{
    switch (error)
    {
    case VfsError::None:
        return "None";
    case VfsError::NotFound:
        return "NotFound";
    case VfsError::AlreadyExists:
        return "AlreadyExists";
    case VfsError::AccessDenied:
        return "AccessDenied";
    case VfsError::InvalidUri:
        return "InvalidUri";
    case VfsError::SchemeNotMounted:
        return "SchemeNotMounted";
    case VfsError::SandboxEscapeViolation:
        return "SandboxEscapeViolation";
    case VfsError::ReadOnlyMount:
        return "ReadOnlyMount";
    case VfsError::IsDirectory:
        return "IsDirectory";
    case VfsError::NotDirectory:
        return "NotDirectory";
    case VfsError::DirectoryNotEmpty:
        return "DirectoryNotEmpty";
    case VfsError::MountTableFull:
        return "MountTableFull";
    case VfsError::InvalidArgument:
        return "InvalidArgument";
    case VfsError::DiskFull:
        return "DiskFull";
    case VfsError::MaxRecursionDepthExceeded:
        return "MaxRecursionDepthExceeded";
    case VfsError::CrossMountOperation:
        return "CrossMountOperation";
    case VfsError::NotSupported:
        return "NotSupported";
    case VfsError::IoFailure:
        return "IoFailure";
    default:
        return "Unknown";
    }
}

/// @brief Returns a human-readable ASCII name for a `MountPriority` tier (for diagnostics and console output).
/// @param[in] priority The `MountPriority` tier to stringify.
/// @return Null-terminated static string view naming the priority tier.
[[nodiscard]] constexpr gp::StringView toString(const MountPriority priority) noexcept
{
    switch (priority)
    {
    case MountPriority::Base:
        return "Base";
    case MountPriority::DLC:
        return "DLC";
    case MountPriority::Patch:
        return "Patch";
    case MountPriority::ModOverride:
        return "ModOverride";
    case MountPriority::DeveloperOverride:
        return "DeveloperOverride";
    default:
        return "Custom";
    }
}

/// @brief Returns a human-readable ASCII name for a `MountAccess` policy (for diagnostics and console output).
/// @param[in] access The `MountAccess` policy to stringify.
/// @return Null-terminated static string view (`"ReadOnly"` or `"ReadWrite"`).
[[nodiscard]] constexpr gp::StringView toString(const MountAccess access) noexcept
{
    switch (access)
    {
    case MountAccess::ReadOnly:
        return "ReadOnly";
    case MountAccess::ReadWrite:
        return "ReadWrite";
    default:
        return "Unknown";
    }
}

}   // namespace gp::vfs
