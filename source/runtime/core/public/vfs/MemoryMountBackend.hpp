// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "threading/sync/SharedMutex.hpp"
#include "vfs/IMountBackend.hpp"

namespace gp::vfs
{

/// @brief Maximum number of virtual files and directories stored inside a `MemoryMountBackend` instance.
inline constexpr gp::USize kMaxMemoryMountNodes = 128u;

/// @brief In-memory virtual filesystem backend for unit testing, embedded engine shaders/fonts, and scratch mounts
///        (ADR-0044, ADR-0060).
/// @details Stores virtual directory and file nodes inside an internal table guarded by a `gp::thread::SharedMutex`,
///          allowing full VFS read/write/atomic-write/mkdir/remove/move/copy/iterate workflows without touching the
///          physical OS disk.
class GP_CORE_API MemoryMountBackend final : public IMountBackend
{
public:
    /// @brief Default-constructs an empty `MemoryMountBackend` with an active root directory node.
    [[nodiscard]] MemoryMountBackend() noexcept;

    /// @brief Constructs an empty `MemoryMountBackend` labeled with `identifier`.
    /// @param[in] identifier Diagnostic identifier returned by `rootPath()` (e.g., `"[memory:test]"`).
    [[nodiscard]] explicit MemoryMountBackend(const gp::StringView identifier) noexcept;

    /// @brief Virtual destructor.
    virtual ~MemoryMountBackend() noexcept override = default;

    /// @brief Removes all files and directories from this in-memory backend, resetting it to an empty root.
    void clear() noexcept;

public:
    /// @copydoc IMountBackend::rootPath()
    [[nodiscard]] virtual gp::StringView rootPath() const noexcept override;

    /// @copydoc IMountBackend::isPhysical()
    [[nodiscard]] virtual bool isPhysical() const noexcept override;

    /// @copydoc IMountBackend::resolvePhysicalPath()
    [[nodiscard]] virtual gp::Expected<gp::PathBuffer, VfsError>
        resolvePhysicalPath(const gp::StringView relativePath) const noexcept override;

    /// @copydoc IMountBackend::exists()
    [[nodiscard]] virtual bool exists(const gp::StringView relativePath) const noexcept override;

    /// @copydoc IMountBackend::getStats()
    [[nodiscard]] virtual gp::Expected<gp::platform::FileStats, VfsError>
        getStats(const gp::StringView relativePath) const noexcept override;

    /// @copydoc IMountBackend::openFileHandle()
    [[nodiscard]] virtual gp::Expected<gp::platform::FileHandle, VfsError> openFileHandle(
        const gp::StringView relativePath,
        const gp::platform::FileAccessMode access,
        const gp::platform::FileOpenMode openMode,
        const gp::platform::FileShareMode shareMode
    ) noexcept override;

    /// @copydoc IMountBackend::readBytesAt()
    [[nodiscard]] virtual gp::Expected<gp::USize, VfsError> readBytesAt(
        const gp::StringView relativePath, const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead
    ) const noexcept override;

    /// @copydoc IMountBackend::writeBytes()
    virtual gp::Expected<bool, VfsError> writeBytes(
        const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes, const bool append
    ) noexcept override;

    /// @copydoc IMountBackend::writeAtomic()
    virtual gp::Expected<bool, VfsError> writeAtomic(
        const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes
    ) noexcept override;

    /// @copydoc IMountBackend::createDirectory()
    virtual gp::Expected<bool, VfsError> createDirectory(const gp::StringView relativePath) noexcept override;

    /// @copydoc IMountBackend::createDirectoryTree()
    virtual gp::Expected<bool, VfsError> createDirectoryTree(const gp::StringView relativePath) noexcept override;

    /// @copydoc IMountBackend::removeFile()
    virtual gp::Expected<bool, VfsError> removeFile(const gp::StringView relativePath) noexcept override;

    /// @copydoc IMountBackend::removeDirectory()
    virtual gp::Expected<bool, VfsError> removeDirectory(const gp::StringView relativePath) noexcept override;

    /// @copydoc IMountBackend::removeDirectoryTree()
    virtual gp::Expected<bool, VfsError> removeDirectoryTree(const gp::StringView relativePath) noexcept override;

    /// @copydoc IMountBackend::move()
    virtual gp::Expected<bool, VfsError> move(
        const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
    ) noexcept override;

    /// @copydoc IMountBackend::copyFile()
    virtual gp::Expected<bool, VfsError> copyFile(
        const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
    ) noexcept override;

    /// @copydoc IMountBackend::iterateDirectory()
    virtual gp::Expected<bool, VfsError> iterateDirectory(
        const gp::StringView relativeDirectoryPath, const gp::platform::DirectoryVisitorFn visitor, void* const userData
    ) const noexcept override;

private:
    struct MemoryNode
    {
        gp::PathBuffer relativePath{};
        gp::String content{};
        gp::platform::FileStats stats{};
        bool active{ false };
    };

private:
    /// @brief Normalizes and validates a relative path inside the in-memory tree.
    /// @param[in] rawRelativePath Input relative path view.
    /// @param[out] outNormalized Receives the normalized relative path without leading/trailing slashes.
    /// @return `VfsError::None` on success, or `VfsError::SandboxEscapeViolation`.
    [[nodiscard]] static VfsError
        normalizeRelativeKey(const gp::StringView rawRelativePath, gp::PathBuffer& outNormalized) noexcept;

    /// @brief Locates an active node matching `normalizedKey` (caller must hold `m_mutex`).
    /// @param[in] normalizedKey Normalized relative path key.
    /// @return Index in `m_nodes`, or `kMaxMemoryMountNodes` if not found.
    [[nodiscard]] gp::USize findNodeIndexUnlocked(const gp::StringView normalizedKey) const noexcept;

    /// @brief Allocates an unused slot in `m_nodes` (caller must hold exclusive `m_mutex`).
    /// @return Index in `m_nodes`, or `kMaxMemoryMountNodes` if full.
    [[nodiscard]] gp::USize allocateNodeSlotUnlocked() noexcept;

    /// @brief Creates a single directory node (caller must hold exclusive `m_mutex`).
    /// @param[in] normalizedKey Normalized relative directory path key.
    /// @return `Expected<bool, VfsError>`.
    gp::Expected<bool, VfsError> createDirectoryUnlocked(const gp::StringView normalizedKey) noexcept;

private:
    mutable gp::thread::SharedMutex m_mutex{};
    gp::PathBuffer m_identifier{ "[memory]" };
    MemoryNode m_nodes[kMaxMemoryMountNodes]{};
};

}   // namespace gp::vfs
