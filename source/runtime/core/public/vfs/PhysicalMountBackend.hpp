// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "vfs/IMountBackend.hpp"

namespace gp::vfs
{

/// @brief Physical operating system directory mount backend for `gp::vfs::VirtualFileSystem` (ADR-0044, ADR-0060).
/// @details Routes VFS operations to Layer 0 `gp::platform::FileSystem` rooted at a normalized OS directory
///          (`m_rootPath`, guaranteed trailing `'/'`), mechanically enforcing a second sandbox-escape check via
///          `gp::platform::Paths::collapseRelativeDirectories()` and `Paths::isUnderDirectory()`.
class GP_CORE_API PhysicalMountBackend final : public IMountBackend
{
public:
    /// @brief Default constructor (uninitialized empty root).
    [[nodiscard]] constexpr PhysicalMountBackend() noexcept = default;

    /// @brief Constructs a `PhysicalMountBackend` rooted at `physicalRootDir`.
    /// @param[in] physicalRootDir Physical OS directory path (normalized automatically with a trailing `'/'`).
    [[nodiscard]] explicit PhysicalMountBackend(const gp::StringView physicalRootDir) noexcept;

    /// @brief Virtual destructor.
    virtual ~PhysicalMountBackend() noexcept override = default;

    /// @brief Initializes or re-binds the physical OS directory root of this backend.
    /// @param[in] physicalRootDir Physical OS directory path.
    /// @return True if `physicalRootDir` is non-empty and valid, false otherwise.
    bool setRootPath(const gp::StringView physicalRootDir) noexcept;

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
    gp::PathBuffer m_rootPath{};
};

}   // namespace gp::vfs
