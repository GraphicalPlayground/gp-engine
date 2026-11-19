// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileHandle.hpp"
#include "platforms/base/system/FileTypes.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "templates/Expected.hpp"
#include "vfs/VfsEnums.hpp"
#include "vfs/VfsTypes.hpp"   // IWYU pragma: keep

namespace gp::vfs
{

/// @brief Abstract storage medium contract for a mounted VFS layer (`PhysicalMountBackend`, `MemoryMountBackend`,
///        or future `PakMountBackend`) (ADR-0007, ADR-0044, ADR-0060).
/// @details All `relativePath` parameters passed into `IMountBackend` are guaranteed by `VirtualFileSystem` to be
///          pre-normalized (`'/'` separators, no leading slash) and sandbox-validated (`collapseRelativeDirectories`
///          already verified zero leading `".."` escape segments).
class GP_CORE_API IMountBackend
{
public:
    /// @brief Default constructor.
    constexpr IMountBackend() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IMountBackend() noexcept = default;

    IMountBackend(const IMountBackend&) = delete;
    IMountBackend& operator=(const IMountBackend&) = delete;
    IMountBackend(IMountBackend&&) = delete;
    IMountBackend& operator=(IMountBackend&&) = delete;

public:
    /// @brief Returns the physical OS root directory or diagnostic identifier of this backend.
    /// @return Non-owning `gp::StringView` describing the mount root.
    [[nodiscard]] virtual gp::StringView rootPath() const noexcept = 0;

    /// @brief Checks whether this backend is backed by a physical OS directory capable of resolving native OS paths.
    /// @return True if `resolvePhysicalPath()` is supported, false for in-memory or compressed archive backends.
    [[nodiscard]] virtual bool isPhysical() const noexcept = 0;

    /// @brief Resolves a sandbox-validated `relativePath` to a physical OS `PathBuffer` if supported by this backend.
    /// @param[in] relativePath Sandbox-validated relative path within the mount.
    /// @return `Expected<gp::PathBuffer, VfsError>` containing the physical OS path, or `VfsError::NotSupported`.
    [[nodiscard]] virtual gp::Expected<gp::PathBuffer, VfsError>
        resolvePhysicalPath(const gp::StringView relativePath) const noexcept = 0;

    /// @brief Checks whether a file or directory exists at `relativePath` inside this backend.
    /// @param[in] relativePath Sandbox-validated relative path within the mount (empty view denotes the mount root).
    /// @return True if the entry exists in this backend, false otherwise.
    [[nodiscard]] virtual bool exists(const gp::StringView relativePath) const noexcept = 0;

    /// @brief Queries metadata (`size`, timestamps, `FileAttributeFlags`) for `relativePath` inside this backend.
    /// @param[in] relativePath Sandbox-validated relative path within the mount.
    /// @return `Expected<FileStats, VfsError>` containing the entry's metadata, or `VfsError::NotFound`.
    [[nodiscard]] virtual gp::Expected<gp::platform::FileStats, VfsError>
        getStats(const gp::StringView relativePath) const noexcept = 0;

    /// @brief Opens a native OS `FileHandle` for `relativePath` when supported by the backend.
    /// @param[in] relativePath Sandbox-validated relative path within the mount.
    /// @param[in] access Read/Write access flags.
    /// @param[in] openMode Creation/truncation disposition.
    /// @param[in] shareMode Concurrent sharing permissions.
    /// @return `Expected<FileHandle, VfsError>` containing the open OS handle, or `VfsError::NotSupported`.
    [[nodiscard]] virtual gp::Expected<gp::platform::FileHandle, VfsError> openFileHandle(
        const gp::StringView relativePath,
        const gp::platform::FileAccessMode access,
        const gp::platform::FileOpenMode openMode,
        const gp::platform::FileShareMode shareMode
    ) noexcept = 0;

    /// @brief Reads up to `bytesToRead` bytes starting at `offset` from `relativePath` into `outBuffer`.
    /// @note Works uniformly across both physical OS mounts and non-physical (`MemoryMountBackend` / `.pak`) mounts.
    /// @param[in] relativePath Sandbox-validated relative file path.
    /// @param[in] offset Byte offset from the start of the file.
    /// @param[out] outBuffer Destination memory buffer.
    /// @param[in] bytesToRead Maximum number of bytes to read.
    /// @return `Expected<gp::USize, VfsError>` containing the number of bytes read, or a `VfsError`.
    [[nodiscard]] virtual gp::Expected<gp::USize, VfsError> readBytesAt(
        const gp::StringView relativePath, const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead
    ) const noexcept = 0;

    /// @brief Creates or overwrites `relativePath` with `sizeInBytes` from `data`.
    /// @param[in] relativePath Sandbox-validated relative file path.
    /// @param[in] data Source memory buffer.
    /// @param[in] sizeInBytes Number of bytes to write.
    /// @param[in] append If true, appends to the end of the file; if false, truncates and overwrites.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> writeBytes(
        const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes, const bool append
    ) noexcept = 0;

    /// @brief Atomically writes `sizeInBytes` from `data` to `relativePath` via temporary staging and rename.
    /// @param[in] relativePath Sandbox-validated relative file path.
    /// @param[in] data Source memory buffer.
    /// @param[in] sizeInBytes Number of bytes to write.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> writeAtomic(
        const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes
    ) noexcept = 0;

    /// @brief Creates a single directory at `relativePath` inside this backend.
    /// @param[in] relativePath Sandbox-validated relative directory path.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> createDirectory(const gp::StringView relativePath) noexcept = 0;

    /// @brief Recursively creates `relativePath` and all missing parent directories inside this backend.
    /// @param[in] relativePath Sandbox-validated relative directory tree path.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> createDirectoryTree(const gp::StringView relativePath) noexcept = 0;

    /// @brief Removes a regular file at `relativePath` inside this backend.
    /// @param[in] relativePath Sandbox-validated relative file path.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> removeFile(const gp::StringView relativePath) noexcept = 0;

    /// @brief Removes an empty directory at `relativePath` inside this backend.
    /// @param[in] relativePath Sandbox-validated relative directory path.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> removeDirectory(const gp::StringView relativePath) noexcept = 0;

    /// @brief Recursively removes a directory and all its contents at `relativePath` inside this backend.
    /// @param[in] relativePath Sandbox-validated relative directory path.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> removeDirectoryTree(const gp::StringView relativePath) noexcept = 0;

    /// @brief Renames or moves an entry from `sourceRelativePath` to `destinationRelativePath` within this backend.
    /// @param[in] sourceRelativePath Existing sandbox-validated relative path.
    /// @param[in] destinationRelativePath Target sandbox-validated relative path.
    /// @param[in] overwrite If true, replaces any existing target entry.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> move(
        const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
    ) noexcept = 0;

    /// @brief Copies a regular file from `sourceRelativePath` to `destinationRelativePath` within this backend.
    /// @param[in] sourceRelativePath Existing sandbox-validated relative file path.
    /// @param[in] destinationRelativePath Target sandbox-validated relative file path.
    /// @param[in] overwrite If true, replaces any existing target file.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> copyFile(
        const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
    ) noexcept = 0;

    /// @brief Iterates over the immediate children of `relativeDirectoryPath` inside this backend.
    /// @param[in] relativeDirectoryPath Sandbox-validated relative directory path (empty view for mount root).
    /// @param[in] visitor Stateless Layer 0 `DirectoryVisitorFn` receiving each child's leaf name and `FileStats`.
    /// @param[in] userData Opaque context pointer forwarded to `visitor`.
    /// @return `Expected<bool, VfsError>` containing `true` on completion, or a `VfsError`.
    virtual gp::Expected<bool, VfsError> iterateDirectory(
        const gp::StringView relativeDirectoryPath, const gp::platform::DirectoryVisitorFn visitor, void* const userData
    ) const noexcept = 0;
};

}   // namespace gp::vfs
