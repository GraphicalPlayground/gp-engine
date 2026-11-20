// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "vfs/PhysicalMountBackend.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"

namespace gp::vfs
{

PhysicalMountBackend::PhysicalMountBackend(const gp::StringView physicalRootDir) noexcept
{
    setRootPath(physicalRootDir);
}

bool PhysicalMountBackend::setRootPath(const gp::StringView physicalRootDir) noexcept
{
    m_rootPath.clear();
    if (physicalRootDir.isEmpty() || physicalRootDir.size() > gp::platform::kMaxPathLength)
    {
        return false;
    }

    m_rootPath.assign(physicalRootDir);
    gp::platform::Paths::makeStandardFilename(m_rootPath);
    gp::platform::Paths::ensureTrailingSeparator(m_rootPath);
    return !m_rootPath.isEmpty();
}

gp::StringView PhysicalMountBackend::rootPath() const noexcept
{
    return m_rootPath.view();
}

bool PhysicalMountBackend::isPhysical() const noexcept
{
    return true;
}

gp::Expected<gp::PathBuffer, VfsError>
    PhysicalMountBackend::resolvePhysicalPath(const gp::StringView relativePath) const noexcept
{
    if (m_rootPath.isEmpty())
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }

    if (relativePath.isEmpty())
    {
        gp::PathBuffer rootCopy(m_rootPath);
        gp::platform::Paths::normalizeDirectoryName(rootCopy);
        return rootCopy;
    }

    gp::PathBuffer sanitizedRel(relativePath);
    gp::platform::Paths::normalizeFilename(sanitizedRel);
    if (!gp::platform::Paths::collapseRelativeDirectories(sanitizedRel))
    {
        return gp::makeUnexpected(VfsError::SandboxEscapeViolation);
    }

    const gp::StringView relView = sanitizedRel.view();
    if (relView == ".." || relView.startsWith("../") || !gp::platform::Paths::isRelative(relView))
    {
        return gp::makeUnexpected(VfsError::SandboxEscapeViolation);
    }

    if (relView.isEmpty() || relView == ".")
    {
        gp::PathBuffer rootCopy(m_rootPath);
        gp::platform::Paths::normalizeDirectoryName(rootCopy);
        return rootCopy;
    }

    gp::PathBuffer combined = gp::platform::Paths::combine(m_rootPath.view(), relView);
    gp::platform::Paths::normalizeDirectoryName(combined);
    return combined;
}

bool PhysicalMountBackend::exists(const gp::StringView relativePath) const noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return false;
    }
    return gp::platform::FileSystem::exists(phys.value().view());
}

gp::Expected<gp::platform::FileStats, VfsError>
    PhysicalMountBackend::getStats(const gp::StringView relativePath) const noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<gp::platform::FileStats, gp::platform::FileError> stats =
        gp::platform::FileSystem::getStats(phys.value().view());
    if (!stats.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(stats.error()));
    }
    return stats.value();
}

gp::Expected<gp::platform::FileHandle, VfsError> PhysicalMountBackend::openFileHandle(
    const gp::StringView relativePath,
    const gp::platform::FileAccessMode access,
    const gp::platform::FileOpenMode openMode,
    const gp::platform::FileShareMode shareMode
) noexcept
{
    if (relativePath.isEmpty())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    gp::Expected<gp::platform::FileHandle, gp::platform::FileError> handleRes =
        gp::platform::FileSystem::open(phys.value().view(), access, openMode, shareMode);
    if (!handleRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(handleRes.error()));
    }
    return std::move(handleRes.value());
}

gp::Expected<gp::USize, VfsError> PhysicalMountBackend::readBytesAt(
    const gp::StringView relativePath, const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead
) const noexcept
{
    if (relativePath.isEmpty())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    gp::Expected<gp::platform::FileHandle, gp::platform::FileError> openRes =
        gp::platform::FileSystem::openRead(phys.value().view());
    if (!openRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(openRes.error()));
    }

    const gp::Expected<gp::USize, gp::platform::FileError> readRes =
        openRes.value().readAt(offset, outBuffer, bytesToRead);
    if (!readRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(readRes.error()));
    }
    return readRes.value();
}

gp::Expected<bool, VfsError> PhysicalMountBackend::writeBytes(
    const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes, const bool append
) noexcept
{
    if (relativePath.isEmpty())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<bool, gp::platform::FileError> writeRes =
        append ? gp::platform::FileSystem::appendAllBytes(phys.value().view(), data, sizeInBytes)
               : gp::platform::FileSystem::writeAllBytes(phys.value().view(), data, sizeInBytes);
    if (!writeRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(writeRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::writeAtomic(
    const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes
) noexcept
{
    if (relativePath.isEmpty())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<bool, gp::platform::FileError> writeRes =
        gp::platform::FileSystem::writeAtomic(phys.value().view(), data, sizeInBytes);
    if (!writeRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(writeRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::createDirectory(const gp::StringView relativePath) noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<bool, gp::platform::FileError> mkdirRes =
        gp::platform::FileSystem::createDirectory(phys.value().view());
    if (!mkdirRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(mkdirRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::createDirectoryTree(const gp::StringView relativePath) noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<bool, gp::platform::FileError> mkdirRes =
        gp::platform::FileSystem::createDirectoryTree(phys.value().view());
    if (!mkdirRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(mkdirRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::removeFile(const gp::StringView relativePath) noexcept
{
    if (relativePath.isEmpty())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<bool, gp::platform::FileError> rmRes = gp::platform::FileSystem::removeFile(phys.value().view());
    if (!rmRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(rmRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::removeDirectory(const gp::StringView relativePath) noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<bool, gp::platform::FileError> rmRes =
        gp::platform::FileSystem::removeDirectory(phys.value().view());
    if (!rmRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(rmRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::removeDirectoryTree(const gp::StringView relativePath) noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> phys = resolvePhysicalPath(relativePath);
    if (!phys.hasValue())
    {
        return gp::makeUnexpected(phys.error());
    }

    const gp::Expected<bool, gp::platform::FileError> rmRes =
        gp::platform::FileSystem::removeDirectoryTree(phys.value().view());
    if (!rmRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(rmRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::move(
    const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
) noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> physSrc = resolvePhysicalPath(sourceRelativePath);
    if (!physSrc.hasValue())
    {
        return gp::makeUnexpected(physSrc.error());
    }
    const gp::Expected<gp::PathBuffer, VfsError> physDst = resolvePhysicalPath(destinationRelativePath);
    if (!physDst.hasValue())
    {
        return gp::makeUnexpected(physDst.error());
    }

    const gp::Expected<bool, gp::platform::FileError> moveRes =
        gp::platform::FileSystem::move(physSrc.value().view(), physDst.value().view(), overwrite);
    if (!moveRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(moveRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::copyFile(
    const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
) noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> physSrc = resolvePhysicalPath(sourceRelativePath);
    if (!physSrc.hasValue())
    {
        return gp::makeUnexpected(physSrc.error());
    }
    const gp::Expected<gp::PathBuffer, VfsError> physDst = resolvePhysicalPath(destinationRelativePath);
    if (!physDst.hasValue())
    {
        return gp::makeUnexpected(physDst.error());
    }

    const gp::Expected<bool, gp::platform::FileError> copyRes =
        gp::platform::FileSystem::copyFile(physSrc.value().view(), physDst.value().view(), overwrite);
    if (!copyRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(copyRes.error()));
    }
    return true;
}

gp::Expected<bool, VfsError> PhysicalMountBackend::iterateDirectory(
    const gp::StringView relativeDirectoryPath, const gp::platform::DirectoryVisitorFn visitor, void* const userData
) const noexcept
{
    const gp::Expected<gp::PathBuffer, VfsError> physDir = resolvePhysicalPath(relativeDirectoryPath);
    if (!physDir.hasValue())
    {
        return gp::makeUnexpected(physDir.error());
    }

    const gp::Expected<bool, gp::platform::FileError> iterRes =
        gp::platform::FileSystem::iterateDirectory(physDir.value().view(), visitor, userData);
    if (!iterRes.hasValue())
    {
        return gp::makeUnexpected(mapFileErrorToVfsError(iterRes.error()));
    }
    return true;
}

}   // namespace gp::vfs
