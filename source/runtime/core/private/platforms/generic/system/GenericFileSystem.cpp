// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/GenericFileSystem.hpp"

namespace gp::platform::generic
{

gp::Expected<FileHandle, FileError> FileSystem::open(
    const gp::StringView path, const FileAccessMode access, const FileOpenMode openMode, const FileShareMode shareMode
) noexcept
{
    (void)path;
    (void)access;
    (void)openMode;
    (void)shareMode;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<FileHandle, FileError>
    FileSystem::openRead(const gp::StringView path, const FileShareMode shareMode) noexcept
{
    return open(path, FileAccessMode::Read, FileOpenMode::OpenExisting, shareMode);
}

gp::Expected<FileHandle, FileError> FileSystem::openWrite(
    const gp::StringView path, const bool append, const bool allowRead, const FileShareMode shareMode
) noexcept
{
    const FileAccessMode access = allowRead ? FileAccessMode::ReadWrite : FileAccessMode::Write;
    const FileOpenMode mode = append ? FileOpenMode::Append : FileOpenMode::CreateAlways;
    return open(path, access, mode, shareMode);
}

gp::Expected<FileStats, FileError> FileSystem::getStats(const gp::StringView path) noexcept
{
    (void)path;
    return gp::makeUnexpected(FileError::NotSupported);
}

bool FileSystem::exists(const gp::StringView path) noexcept
{
    return getStats(path).hasValue();
}

bool FileSystem::isFile(const gp::StringView path) noexcept
{
    const gp::Expected<FileStats, FileError> stats = getStats(path);
    return stats.hasValue() && stats.value().isFile();
}

bool FileSystem::isDirectory(const gp::StringView path) noexcept
{
    const gp::Expected<FileStats, FileError> stats = getStats(path);
    return stats.hasValue() && stats.value().isDirectory();
}

bool FileSystem::isReadOnly(const gp::StringView path) noexcept
{
    const gp::Expected<FileStats, FileError> stats = getStats(path);
    return stats.hasValue() && stats.value().isReadOnly();
}

gp::Expected<bool, FileError> FileSystem::setReadOnly(const gp::StringView path, const bool readOnly) noexcept
{
    (void)path;
    (void)readOnly;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<gp::UInt64, FileError> FileSystem::fileSize(const gp::StringView path) noexcept
{
    const gp::Expected<FileStats, FileError> stats = getStats(path);
    if (!stats.hasValue())
    {
        return gp::makeUnexpected(stats.error());
    }
    if (stats.value().isDirectory())
    {
        return gp::makeUnexpected(FileError::IsDirectory);
    }
    return stats.value().size;
}

gp::Expected<bool, FileError> FileSystem::createDirectory(const gp::StringView path) noexcept
{
    (void)path;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::createDirectoryTree(const gp::StringView path) noexcept
{
    return createDirectoryTreeImpl<FileSystem>(path);
}

gp::Expected<bool, FileError> FileSystem::removeFile(const gp::StringView path) noexcept
{
    (void)path;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::removeDirectory(const gp::StringView path) noexcept
{
    (void)path;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::removeDirectoryTree(const gp::StringView path) noexcept
{
    return removeDirectoryTreeImpl<FileSystem>(path);
}

gp::Expected<bool, FileError> FileSystem::move(
    const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite
) noexcept
{
    (void)sourcePath;
    (void)destinationPath;
    (void)overwrite;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::copyFile(
    const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite
) noexcept
{
    (void)sourcePath;
    (void)destinationPath;
    (void)overwrite;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::iterateDirectory(
    const gp::StringView directoryPath, const DirectoryVisitorFn visitor, void* const userData
) noexcept
{
    (void)directoryPath;
    (void)visitor;
    (void)userData;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::iterateDirectoryRecursive(
    const gp::StringView directoryPath, const DirectoryVisitorFn visitor, void* const userData
) noexcept
{
    bool stopped = false;
    return iterateDirectoryRecursiveImpl<FileSystem>(directoryPath, visitor, userData, 0u, stopped);
}

gp::Expected<gp::USize, FileError>
    FileSystem::readBytes(const gp::StringView path, void* const outBuffer, const gp::USize bufferCapacity) noexcept
{
    (void)path;
    (void)outBuffer;
    (void)bufferCapacity;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError>
    FileSystem::writeAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept
{
    (void)path;
    (void)data;
    (void)sizeInBytes;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::writeAllText(const gp::StringView path, const gp::StringView text) noexcept
{
    return writeAllBytes(path, text.data(), text.size());
}

gp::Expected<bool, FileError>
    FileSystem::appendAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept
{
    (void)path;
    (void)data;
    (void)sizeInBytes;
    return gp::makeUnexpected(FileError::NotSupported);
}

gp::Expected<bool, FileError> FileSystem::appendAllText(const gp::StringView path, const gp::StringView text) noexcept
{
    return appendAllBytes(path, text.data(), text.size());
}

gp::Expected<bool, FileError>
    FileSystem::writeAtomic(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept
{
    return writeAtomicImpl<FileSystem>(path, data, sizeInBytes);
}

gp::Expected<bool, FileError> FileSystem::writeAtomicText(const gp::StringView path, const gp::StringView text) noexcept
{
    return writeAtomic(path, text.data(), text.size());
}

}   // namespace gp::platform::generic
