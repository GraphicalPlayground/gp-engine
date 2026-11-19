// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixFileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace gp::platform::unix::detail
{

/// @brief Maps a POSIX `errno` value to the canonical engine `gp::platform::FileError` enumeration.
/// @param[in] posixErrno Raw `errno` integer value.
/// @return Mapped `FileError` enum value.
[[nodiscard]] static FileError mapPosixErrno(const int posixErrno) noexcept
{
    switch (posixErrno)
    {
    case 0:
        return FileError::None;
    case ENOENT:
        return FileError::NotFound;
    case EEXIST:
        return FileError::AlreadyExists;
    case EACCES:
    case EPERM:
        return FileError::AccessDenied;
    case EROFS:
        return FileError::ReadOnlyFileSystem;
    case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
    case EWOULDBLOCK:
#endif
    case ETXTBSY:
        return FileError::SharingViolation;
    case ENAMETOOLONG:
    case ELOOP:
        return FileError::InvalidPath;
    case EBADF:
        return FileError::InvalidHandle;
    case EINVAL:
    case ESPIPE:
        return FileError::InvalidArgument;
    case EISDIR:
        return FileError::IsDirectory;
    case ENOTDIR:
        return FileError::NotDirectory;
    case ENOTEMPTY:
        return FileError::DirectoryNotEmpty;
    case ENOSPC:
    case EDQUOT:
        return FileError::DiskFull;
    case EMFILE:
    case ENFILE:
        return FileError::TooManyOpenFiles;
    case EXDEV:
        return FileError::CrossDeviceLink;
    default:
        return FileError::IoFailure;
    }
}

/// @brief Normalizes a UTF-8 `gp::StringView` into a null-terminated `PathBuffer` for POSIX syscalls.
/// @param[in] utf8Path Input UTF-8 path view.
/// @param[out] outPath Destination `PathBuffer`.
/// @param[in] trimTrailingSlash If true, removes redundant trailing slashes.
/// @return True if `outPath` is non-empty and fits within `kMaxPathLength`, false otherwise.
[[nodiscard]] static bool
    toPosixPath(const gp::StringView utf8Path, PathBuffer& outPath, const bool trimTrailingSlash = false) noexcept
{
    outPath.clear();
    if (utf8Path.isEmpty() || utf8Path.size() > kMaxPathLength)
    {
        return false;
    }

    outPath.assign(utf8Path);
    if (trimTrailingSlash)
    {
        Paths::normalizeDirectoryName(outPath);
    }
    else
    {
        Paths::normalizeFilename(outPath);
    }

    return !outPath.isEmpty();
}

/// @brief Converts POSIX `seconds` and `nanoseconds` into a `gp::time::Timestamp` (nanoseconds since Unix Epoch).
/// @param[in] sec Signed seconds since 1970-01-01T00:00:00Z.
/// @param[in] nsec Nanosecond sub-second offset.
/// @return Normalized `gp::time::Timestamp`.
[[nodiscard]] static gp::time::Timestamp timespecToTimestamp(const gp::Int64 sec, const gp::Int64 nsec) noexcept
{
    if (sec <= 0)
    {
        return gp::time::Timestamp(0ULL);
    }
    const gp::UInt64 totalNs =
        static_cast<gp::UInt64>(sec) * 1'000'000'000ULL + (nsec > 0 ? static_cast<gp::UInt64>(nsec) : 0ULL);
    return gp::time::Timestamp(totalNs);
}

/// @brief Populates a `FileStats` structure from a POSIX `struct stat` and leaf filename.
/// @param[in] st Populated `struct stat` from `::lstat` or `::stat`.
/// @param[in] isSymlink True if `lstat` detected `S_ISLNK(lstatBuf.st_mode)`.
/// @param[in] leafName Clean leaf filename (used to detect POSIX dot-prefixed hidden files).
/// @return Populated `FileStats` snapshot.
[[nodiscard]] static FileStats
    buildStatsFromPosixStat(const struct stat& st, const bool isSymlink, const gp::StringView leafName) noexcept
{
    FileStats stats{};
    const bool isDir = S_ISDIR(st.st_mode);
    stats.size = isDir ? 0ULL : static_cast<gp::UInt64>(st.st_size);

#if GP_PLATFORM_APPLE || GP_PLATFORM_MACOS
    stats.lastWriteTime = timespecToTimestamp(
        static_cast<gp::Int64>(st.st_mtimespec.tv_sec), static_cast<gp::Int64>(st.st_mtimespec.tv_nsec)
    );
    stats.lastAccessTime = timespecToTimestamp(
        static_cast<gp::Int64>(st.st_atimespec.tv_sec), static_cast<gp::Int64>(st.st_atimespec.tv_nsec)
    );
    stats.creationTime = timespecToTimestamp(
        static_cast<gp::Int64>(st.st_birthtimespec.tv_sec), static_cast<gp::Int64>(st.st_birthtimespec.tv_nsec)
    );
#else
    stats.lastWriteTime =
        timespecToTimestamp(static_cast<gp::Int64>(st.st_mtim.tv_sec), static_cast<gp::Int64>(st.st_mtim.tv_nsec));
    stats.lastAccessTime =
        timespecToTimestamp(static_cast<gp::Int64>(st.st_atim.tv_sec), static_cast<gp::Int64>(st.st_atim.tv_nsec));
    stats.creationTime = stats.lastWriteTime;
#endif

    if (stats.creationTime.nanoseconds == 0ULL)
    {
        stats.creationTime = stats.lastWriteTime;
    }

    FileAttributeFlags flags = isDir ? FileAttributeFlags::Directory : FileAttributeFlags::RegularFile;
    if ((st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0)
    {
        flags = flags | FileAttributeFlags::ReadOnly;
    }
    if (!leafName.isEmpty() && leafName[0] == '.' && leafName != "." && leafName != "..")
    {
        flags = flags | FileAttributeFlags::Hidden;
    }
    if (isSymlink)
    {
        flags = flags | FileAttributeFlags::SymbolicLink;
    }

    stats.attributes = flags;
    return stats;
}

}   // namespace gp::platform::unix::detail

namespace gp::platform
{

gp::Expected<bool, FileError> FileHandle::close() noexcept
{
    if (!isValid())
    {
        return true;
    }

    const int fd = m_descriptor;
    m_descriptor = kInvalidNativeFileDescriptor;

    if (::close(fd) != 0 && errno != EINTR)
    {
        return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
    }
    return true;
}

gp::Expected<gp::USize, FileError> FileHandle::read(void* const outBuffer, const gp::USize bytesToRead) noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }
    if (bytesToRead == 0u)
    {
        return 0u;
    }
    if (outBuffer == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    gp::Byte* dst = static_cast<gp::Byte*>(outBuffer);
    gp::USize totalRead = 0u;

    while (totalRead < bytesToRead)
    {
        const gp::USize remaining = bytesToRead - totalRead;
        const ssize_t n = ::read(m_descriptor, dst + totalRead, remaining);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
        }
        if (n == 0)
        {
            break;
        }
        totalRead += static_cast<gp::USize>(n);
    }

    return totalRead;
}

gp::Expected<gp::USize, FileError> FileHandle::write(const void* const buffer, const gp::USize bytesToWrite) noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }
    if (bytesToWrite == 0u)
    {
        return 0u;
    }
    if (buffer == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    const gp::Byte* src = static_cast<const gp::Byte*>(buffer);
    gp::USize totalWritten = 0u;

    while (totalWritten < bytesToWrite)
    {
        const gp::USize remaining = bytesToWrite - totalWritten;
        const ssize_t n = ::write(m_descriptor, src + totalWritten, remaining);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
        }
        if (n == 0)
        {
            break;
        }
        totalWritten += static_cast<gp::USize>(n);
    }

    return totalWritten;
}

gp::Expected<gp::USize, FileError>
    FileHandle::readAt(const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead) const noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }
    if (bytesToRead == 0u)
    {
        return 0u;
    }
    if (outBuffer == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    gp::Byte* dst = static_cast<gp::Byte*>(outBuffer);
    gp::USize totalRead = 0u;

    while (totalRead < bytesToRead)
    {
        const gp::USize remaining = bytesToRead - totalRead;
        const off_t currentOffset = static_cast<off_t>(offset + static_cast<gp::UInt64>(totalRead));
        const ssize_t n = ::pread(m_descriptor, dst + totalRead, remaining, currentOffset);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
        }
        if (n == 0)
        {
            break;
        }
        totalRead += static_cast<gp::USize>(n);
    }

    return totalRead;
}

gp::Expected<gp::USize, FileError>
    FileHandle::writeAt(const gp::UInt64 offset, const void* const buffer, const gp::USize bytesToWrite) noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }
    if (bytesToWrite == 0u)
    {
        return 0u;
    }
    if (buffer == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    const gp::Byte* src = static_cast<const gp::Byte*>(buffer);
    gp::USize totalWritten = 0u;

    while (totalWritten < bytesToWrite)
    {
        const gp::USize remaining = bytesToWrite - totalWritten;
        const off_t currentOffset = static_cast<off_t>(offset + static_cast<gp::UInt64>(totalWritten));
        const ssize_t n = ::pwrite(m_descriptor, src + totalWritten, remaining, currentOffset);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
        }
        if (n == 0)
        {
            break;
        }
        totalWritten += static_cast<gp::USize>(n);
    }

    return totalWritten;
}

gp::Expected<gp::UInt64, FileError> FileHandle::seek(const gp::Int64 offset, const FileSeekOrigin origin) noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    int whence = SEEK_SET;
    switch (origin)
    {
    case FileSeekOrigin::Begin:
        whence = SEEK_SET;
        break;
    case FileSeekOrigin::Current:
        whence = SEEK_CUR;
        break;
    case FileSeekOrigin::End:
        whence = SEEK_END;
        break;
    default:
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    const off_t newPos = ::lseek(m_descriptor, static_cast<off_t>(offset), whence);
    if (newPos < 0)
    {
        return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
    }

    return static_cast<gp::UInt64>(newPos);
}

gp::Expected<gp::UInt64, FileError> FileHandle::tell() const noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    const off_t currentPos = ::lseek(m_descriptor, 0, SEEK_CUR);
    if (currentPos < 0)
    {
        return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
    }

    return static_cast<gp::UInt64>(currentPos);
}

gp::Expected<gp::UInt64, FileError> FileHandle::size() const noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    struct stat st{};
    if (::fstat(m_descriptor, &st) != 0)
    {
        return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
    }

    return static_cast<gp::UInt64>(st.st_size);
}

gp::Expected<bool, FileError> FileHandle::flush() noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    while (::fsync(m_descriptor) != 0)
    {
        if (errno == EINTR)
        {
            continue;
        }
        return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
    }

    return true;
}

gp::Expected<bool, FileError> FileHandle::truncate(const gp::UInt64 newSize) noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    while (::ftruncate(m_descriptor, static_cast<off_t>(newSize)) != 0)
    {
        if (errno == EINTR)
        {
            continue;
        }
        return gp::makeUnexpected(unix::detail::mapPosixErrno(errno));
    }

    return true;
}

}   // namespace gp::platform

namespace gp::platform::unix
{

gp::Expected<FileHandle, FileError> FileSystem::open(
    const gp::StringView path,
    const FileAccessMode access,
    const FileOpenMode openMode,
    [[maybe_unused]] const FileShareMode shareMode
) noexcept
{
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, false))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    int flags = 0;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    const bool canRead = gp::enums::hasAllFlags(access, FileAccessMode::Read);
    const bool canWrite = gp::enums::hasAllFlags(access, FileAccessMode::Write);
    if (canRead && canWrite)
    {
        flags |= O_RDWR;
    }
    else if (canRead)
    {
        flags |= O_RDONLY;
    }
    else if (canWrite)
    {
        flags |= O_WRONLY;
    }
    else
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    switch (openMode)
    {
    case FileOpenMode::OpenExisting:
        break;
    case FileOpenMode::CreateAlways:
        flags |= (O_CREAT | O_TRUNC);
        break;
    case FileOpenMode::CreateNew:
        flags |= (O_CREAT | O_EXCL);
        break;
    case FileOpenMode::OpenAlways:
        flags |= O_CREAT;
        break;
    case FileOpenMode::Append:
        flags |= (O_CREAT | O_APPEND);
        break;
    default:
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    int fd = -1;
    do
    {
        fd = ::open(posixPath.cString(), flags, 0644);
    }
    while (fd < 0 && errno == EINTR);

    if (fd < 0)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }

    struct stat st{};
    if (::fstat(fd, &st) == 0 && S_ISDIR(st.st_mode))
    {
        ::close(fd);
        return gp::makeUnexpected(FileError::IsDirectory);
    }

    return FileHandle(fd);
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
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    struct stat lstatBuf{};
    if (::lstat(posixPath.cString(), &lstatBuf) != 0)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }

    const bool isSymlink = S_ISLNK(lstatBuf.st_mode);
    struct stat targetStat = lstatBuf;
    if (isSymlink)
    {
        if (::stat(posixPath.cString(), &targetStat) != 0)
        {
            targetStat = lstatBuf;
        }
    }

    return detail::buildStatsFromPosixStat(targetStat, isSymlink, Paths::getPathLeaf(posixPath.view()));
}

bool FileSystem::exists(const gp::StringView path) noexcept
{
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, true))
    {
        return false;
    }

    struct stat st{};
    return ::stat(posixPath.cString(), &st) == 0 || ::lstat(posixPath.cString(), &st) == 0;
}

bool FileSystem::isFile(const gp::StringView path) noexcept
{
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, true))
    {
        return false;
    }

    struct stat st{};
    return ::stat(posixPath.cString(), &st) == 0 && S_ISREG(st.st_mode);
}

bool FileSystem::isDirectory(const gp::StringView path) noexcept
{
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, true))
    {
        return false;
    }

    struct stat st{};
    return ::stat(posixPath.cString(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool FileSystem::isReadOnly(const gp::StringView path) noexcept
{
    const gp::Expected<FileStats, FileError> stats = getStats(path);
    return stats.hasValue() && stats.value().isReadOnly();
}

gp::Expected<bool, FileError> FileSystem::setReadOnly(const gp::StringView path, const bool readOnly) noexcept
{
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    struct stat st{};
    if (::stat(posixPath.cString(), &st) != 0)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }

    mode_t newMode = st.st_mode;
    if (readOnly)
    {
        newMode &= ~static_cast<mode_t>(S_IWUSR | S_IWGRP | S_IWOTH);
    }
    else
    {
        newMode |= static_cast<mode_t>(S_IWUSR);
    }

    if (::chmod(posixPath.cString(), newMode) != 0)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }

    return true;
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
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    if (::mkdir(posixPath.cString(), 0755) != 0)
    {
        const int err = errno;
        if (err == EEXIST)
        {
            struct stat st{};
            if (::stat(posixPath.cString(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                return true;
            }
            return gp::makeUnexpected(FileError::AlreadyExists);
        }
        return gp::makeUnexpected(detail::mapPosixErrno(err));
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::createDirectoryTree(const gp::StringView path) noexcept
{
    return createDirectoryTreeImpl<FileSystem>(path);
}

gp::Expected<bool, FileError> FileSystem::removeFile(const gp::StringView path) noexcept
{
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, false))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    if (::unlink(posixPath.cString()) != 0)
    {
        const int err = errno;
        if (err == EPERM || err == EISDIR)
        {
            struct stat st{};
            if (::lstat(posixPath.cString(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                return gp::makeUnexpected(FileError::IsDirectory);
            }
        }
        return gp::makeUnexpected(detail::mapPosixErrno(err));
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::removeDirectory(const gp::StringView path) noexcept
{
    PathBuffer posixPath{};
    if (!detail::toPosixPath(path, posixPath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    struct stat st{};
    if (::lstat(posixPath.cString(), &st) != 0)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }
    if (!S_ISDIR(st.st_mode))
    {
        return gp::makeUnexpected(FileError::NotDirectory);
    }

    if (::rmdir(posixPath.cString()) != 0)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::removeDirectoryTree(const gp::StringView path) noexcept
{
    return removeDirectoryTreeImpl<FileSystem>(path);
}

gp::Expected<bool, FileError> FileSystem::move(
    const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite
) noexcept
{
    PathBuffer posixSrc{};
    PathBuffer posixDst{};
    if (!detail::toPosixPath(sourcePath, posixSrc, true) || !detail::toPosixPath(destinationPath, posixDst, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    if (!overwrite && exists(posixDst.view()))
    {
        return gp::makeUnexpected(FileError::AlreadyExists);
    }

    if (::rename(posixSrc.cString(), posixDst.cString()) != 0)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::copyFile(
    const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite
) noexcept
{
    if (sourcePath.isEmpty() || destinationPath.isEmpty())
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    if (Paths::equals(sourcePath, destinationPath))
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    gp::Expected<FileHandle, FileError> srcOpen = openRead(sourcePath);
    if (!srcOpen.hasValue())
    {
        return gp::makeUnexpected(srcOpen.error());
    }

    const FileOpenMode dstMode = overwrite ? FileOpenMode::CreateAlways : FileOpenMode::CreateNew;
    gp::Expected<FileHandle, FileError> dstOpen =
        open(destinationPath, FileAccessMode::Write, dstMode, FileShareMode::None);
    if (!dstOpen.hasValue())
    {
        return gp::makeUnexpected(dstOpen.error());
    }

    FileHandle& srcHandle = srcOpen.value();
    FileHandle& dstHandle = dstOpen.value();

    gp::Byte chunkBuf[kFileCopyChunkSize]{};
    while (true)
    {
        const gp::Expected<gp::USize, FileError> readRes = srcHandle.read(chunkBuf, kFileCopyChunkSize);
        if (!readRes.hasValue())
        {
            return gp::makeUnexpected(readRes.error());
        }
        const gp::USize bytesRead = readRes.value();
        if (bytesRead == 0u)
        {
            break;
        }

        const gp::Expected<gp::USize, FileError> writeRes = dstHandle.write(chunkBuf, bytesRead);
        if (!writeRes.hasValue())
        {
            return gp::makeUnexpected(writeRes.error());
        }
        if (writeRes.value() != bytesRead)
        {
            return gp::makeUnexpected(FileError::DiskFull);
        }
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::iterateDirectory(
    const gp::StringView directoryPath, const DirectoryVisitorFn visitor, void* const userData
) noexcept
{
    if (visitor == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    PathBuffer baseDir{};
    if (!detail::toPosixPath(directoryPath, baseDir, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    DIR* const dirStream = ::opendir(baseDir.cString());
    if (dirStream == nullptr)
    {
        return gp::makeUnexpected(detail::mapPosixErrno(errno));
    }

    while (true)
    {
        errno = 0;
        const struct dirent* const ent = ::readdir(dirStream);
        if (ent == nullptr)
        {
            if (errno != 0)
            {
                const int err = errno;
                ::closedir(dirStream);
                return gp::makeUnexpected(detail::mapPosixErrno(err));
            }
            break;
        }

        const char* const rawName = ent->d_name;
        if (rawName[0] == '.' && (rawName[1] == '\0' || (rawName[1] == '.' && rawName[2] == '\0')))
        {
            continue;
        }

        const gp::StringView leafName(rawName);
        const PathBuffer fullEntryPath = Paths::combine(baseDir.view(), leafName);
        const gp::Expected<FileStats, FileError> entryStats = getStats(fullEntryPath.view());
        if (!entryStats.hasValue())
        {
            continue;
        }

        DirectoryEntry entry{};
        entry.path = fullEntryPath.view();
        entry.name = Paths::getCleanFilename(entry.path);
        entry.stats = entryStats.value();
        entry.depth = 0u;

        const DirectoryVisitControl ctrl = visitor(entry, userData);
        if (ctrl == DirectoryVisitControl::Stop)
        {
            break;
        }
    }

    ::closedir(dirStream);
    return true;
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
    if (bufferCapacity > 0u && outBuffer == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    gp::Expected<FileHandle, FileError> openRes = openRead(path);
    if (!openRes.hasValue())
    {
        return gp::makeUnexpected(openRes.error());
    }

    return openRes.value().read(outBuffer, bufferCapacity);
}

gp::Expected<bool, FileError>
    FileSystem::writeAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept
{
    if (sizeInBytes > 0u && data == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    gp::Expected<FileHandle, FileError> openRes = openWrite(path, false, false);
    if (!openRes.hasValue())
    {
        return gp::makeUnexpected(openRes.error());
    }

    if (sizeInBytes == 0u)
    {
        return true;
    }

    const gp::Expected<gp::USize, FileError> writeRes = openRes.value().write(data, sizeInBytes);
    if (!writeRes.hasValue())
    {
        return gp::makeUnexpected(writeRes.error());
    }
    if (writeRes.value() != sizeInBytes)
    {
        return gp::makeUnexpected(FileError::DiskFull);
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::writeAllText(const gp::StringView path, const gp::StringView text) noexcept
{
    return writeAllBytes(path, text.data(), text.size());
}

gp::Expected<bool, FileError>
    FileSystem::appendAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept
{
    if (sizeInBytes > 0u && data == nullptr)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    gp::Expected<FileHandle, FileError> openRes = openWrite(path, true, false);
    if (!openRes.hasValue())
    {
        return gp::makeUnexpected(openRes.error());
    }

    if (sizeInBytes == 0u)
    {
        return true;
    }

    const gp::Expected<gp::USize, FileError> writeRes = openRes.value().write(data, sizeInBytes);
    if (!writeRes.hasValue())
    {
        return gp::makeUnexpected(writeRes.error());
    }
    if (writeRes.value() != sizeInBytes)
    {
        return gp::makeUnexpected(FileError::DiskFull);
    }

    return true;
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

}   // namespace gp::platform::unix
