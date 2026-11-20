// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsFileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep

namespace gp::platform::windows::detail
{

/// @brief Maximum UTF-16 `wchar_t` stack buffer capacity for Win32 filesystem calls.
constexpr gp::USize kWin32WidePathCapacity = kMaxPathLength + 16u;

/// @brief Difference between the Windows `FILETIME` Epoch (`1601-01-01T00:00:00Z`) and Unix Epoch
/// (`1970-01-01T00:00:00Z`)
///        measured in 100-nanosecond intervals.
constexpr gp::UInt64 kWin32FileTimeToUnixEpochTicks = 116444736000000000ULL;

/// @brief Maps a Win32 `GetLastError()` code to the canonical engine `gp::platform::FileError` enumeration.
/// @param[in] win32Error Raw `DWORD` error code returned by `::GetLastError()`.
/// @return Mapped `FileError` enum value.
[[nodiscard]] static FileError mapWin32Error(const DWORD win32Error) noexcept
{
    switch (win32Error)
    {
    case ERROR_SUCCESS:
        return FileError::None;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
        return FileError::NotFound;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return FileError::AlreadyExists;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
    case ERROR_CURRENT_DIRECTORY:
        return FileError::AccessDenied;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return FileError::SharingViolation;
    case ERROR_WRITE_PROTECT:
        return FileError::ReadOnlyFileSystem;
    case ERROR_INVALID_NAME:
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BAD_PATHNAME:
    case ERROR_DIRECTORY:
        return FileError::InvalidPath;
    case ERROR_INVALID_HANDLE:
        return FileError::InvalidHandle;
    case ERROR_INVALID_PARAMETER:
    case ERROR_NEGATIVE_SEEK:
        return FileError::InvalidArgument;
    case ERROR_DIR_NOT_EMPTY:
        return FileError::DirectoryNotEmpty;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return FileError::DiskFull;
    case ERROR_TOO_MANY_OPEN_FILES:
        return FileError::TooManyOpenFiles;
    case ERROR_NOT_SAME_DEVICE:
        return FileError::CrossDeviceLink;
    default:
        return FileError::IoFailure;
    }
}

/// @brief Converts a UTF-8 `gp::StringView` path into a null-terminated Win32 UTF-16 `wchar_t` buffer on the stack.
/// @param[in] utf8Path Input UTF-8 path view.
/// @param[out] outWideBuf Destination stack buffer of capacity `kWin32WidePathCapacity`.
/// @param[in] trimTrailingSlash If true, removes redundant trailing slashes prior to conversion.
/// @return True if conversion succeeded and produced a valid non-empty UTF-16 path, false otherwise.
[[nodiscard]] static bool toWin32WidePath(
    const gp::StringView utf8Path, wchar_t (&outWideBuf)[kWin32WidePathCapacity], const bool trimTrailingSlash = false
) noexcept
{
    outWideBuf[0] = L'\0';
    if (utf8Path.isEmpty() || utf8Path.size() > kMaxPathLength)
    {
        return false;
    }

    PathBuffer platformPath(utf8Path);
    if (trimTrailingSlash)
    {
        Paths::normalizeDirectoryName(platformPath);
    }
    Paths::makePlatformFilename(platformPath);
    if (platformPath.isEmpty())
    {
        return false;
    }

    const int wideLen = ::MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        platformPath.cString(),
        static_cast<int>(platformPath.size()),
        outWideBuf,
        static_cast<int>(kWin32WidePathCapacity - 1u)
    );

    if (wideLen <= 0 || static_cast<gp::USize>(wideLen) >= kWin32WidePathCapacity)
    {
        outWideBuf[0] = L'\0';
        return false;
    }

    outWideBuf[wideLen] = L'\0';
    return true;
}

/// @brief Converts a Win32 `FILETIME` (100-ns intervals since 1601-01-01) into a `gp::time::Timestamp` (Unix Epoch ns).
/// @param[in] ft Win32 `FILETIME` structure.
/// @return Normalized `gp::time::Timestamp` in nanoseconds since 1970-01-01 UTC.
[[nodiscard]] static gp::time::Timestamp fileTimeToTimestamp(const FILETIME& ft) noexcept
{
    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    if (uli.QuadPart <= kWin32FileTimeToUnixEpochTicks)
    {
        return gp::time::Timestamp(0ULL);
    }

    const gp::UInt64 unixTicks100ns = uli.QuadPart - kWin32FileTimeToUnixEpochTicks;
    return gp::time::Timestamp(unixTicks100ns * 100ULL);
}

/// @brief Populates a `FileStats` structure from Win32 file attributes, sizes, and `FILETIME` timestamps.
/// @param[in] dwFileAttributes Win32 `FILE_ATTRIBUTE_*` bitmask.
/// @param[in] nFileSizeHigh High 32 bits of file size.
/// @param[in] nFileSizeLow Low 32 bits of file size.
/// @param[in] ftCreationTime Win32 creation timestamp.
/// @param[in] ftLastWriteTime Win32 modification timestamp.
/// @param[in] ftLastAccessTime Win32 last-access timestamp.
/// @return Populated `FileStats` snapshot.
[[nodiscard]] static FileStats buildStatsFromWin32Data(
    const DWORD dwFileAttributes,
    const DWORD nFileSizeHigh,
    const DWORD nFileSizeLow,
    const FILETIME& ftCreationTime,
    const FILETIME& ftLastWriteTime,
    const FILETIME& ftLastAccessTime
) noexcept
{
    FileStats stats{};
    const bool isDir = (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u;
    stats.size =
        isDir ? 0ULL : ((static_cast<gp::UInt64>(nFileSizeHigh) << 32u) | static_cast<gp::UInt64>(nFileSizeLow));
    stats.creationTime = fileTimeToTimestamp(ftCreationTime);
    stats.lastWriteTime = fileTimeToTimestamp(ftLastWriteTime);
    stats.lastAccessTime = fileTimeToTimestamp(ftLastAccessTime);

    FileAttributeFlags flags = isDir ? FileAttributeFlags::Directory : FileAttributeFlags::RegularFile;
    if ((dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0u)
    {
        flags = flags | FileAttributeFlags::ReadOnly;
    }
    if ((dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0u)
    {
        flags = flags | FileAttributeFlags::Hidden;
    }
    if ((dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
    {
        flags = flags | FileAttributeFlags::SymbolicLink;
    }

    stats.attributes = flags;
    return stats;
}

}   // namespace gp::platform::windows::detail

namespace gp::platform
{

gp::Expected<bool, FileError> FileHandle::close() noexcept
{
    if (!isValid())
    {
        return true;
    }

    const HANDLE rawHandle = static_cast<HANDLE>(m_descriptor);
    m_descriptor = kInvalidNativeFileDescriptor;

    if (::CloseHandle(rawHandle) == 0)
    {
        return gp::makeUnexpected(windows::detail::mapWin32Error(::GetLastError()));
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

    const HANDLE rawHandle = static_cast<HANDLE>(m_descriptor);
    gp::Byte* dst = static_cast<gp::Byte*>(outBuffer);
    gp::USize totalRead = 0u;

    while (totalRead < bytesToRead)
    {
        const gp::USize remaining = bytesToRead - totalRead;
        const DWORD chunkToRead =
            (remaining > static_cast<gp::USize>(0x7FFFFFFFu)) ? 0x7FFFFFFFu : static_cast<DWORD>(remaining);

        DWORD chunkRead = 0u;
        if (::ReadFile(rawHandle, dst + totalRead, chunkToRead, &chunkRead, nullptr) == 0)
        {
            const DWORD err = ::GetLastError();
            if (err == ERROR_HANDLE_EOF)
            {
                break;
            }
            return gp::makeUnexpected(windows::detail::mapWin32Error(err));
        }

        if (chunkRead == 0u)
        {
            break;
        }
        totalRead += static_cast<gp::USize>(chunkRead);
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

    const HANDLE rawHandle = static_cast<HANDLE>(m_descriptor);
    const gp::Byte* src = static_cast<const gp::Byte*>(buffer);
    gp::USize totalWritten = 0u;

    while (totalWritten < bytesToWrite)
    {
        const gp::USize remaining = bytesToWrite - totalWritten;
        const DWORD chunkToWrite =
            (remaining > static_cast<gp::USize>(0x7FFFFFFFu)) ? 0x7FFFFFFFu : static_cast<DWORD>(remaining);

        DWORD chunkWritten = 0u;
        if (::WriteFile(rawHandle, src + totalWritten, chunkToWrite, &chunkWritten, nullptr) == 0)
        {
            return gp::makeUnexpected(windows::detail::mapWin32Error(::GetLastError()));
        }

        if (chunkWritten == 0u)
        {
            break;
        }
        totalWritten += static_cast<gp::USize>(chunkWritten);
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

    const HANDLE rawHandle = static_cast<HANDLE>(m_descriptor);
    gp::Byte* dst = static_cast<gp::Byte*>(outBuffer);
    gp::USize totalRead = 0u;

    // Pedagogical note: Passing a stack-allocated OVERLAPPED struct with an explicit 64-bit Offset/OffsetHigh to
    // synchronous ReadFile tells the NT I/O Manager to read directly at the requested byte offset without consulting
    // the shared FILE_OBJECT::CurrentByteOffset, allowing lock-free concurrent reads across JobSystem fibers.
    while (totalRead < bytesToRead)
    {
        const gp::UInt64 currentOffset = offset + static_cast<gp::UInt64>(totalRead);
        const gp::USize remaining = bytesToRead - totalRead;
        const DWORD chunkToRead =
            (remaining > static_cast<gp::USize>(0x7FFFFFFFu)) ? 0x7FFFFFFFu : static_cast<DWORD>(remaining);

        OVERLAPPED overlapped{};
        overlapped.Offset = static_cast<DWORD>(currentOffset & 0xFFFFFFFFULL);
        overlapped.OffsetHigh = static_cast<DWORD>(currentOffset >> 32u);

        DWORD chunkRead = 0u;
        if (::ReadFile(rawHandle, dst + totalRead, chunkToRead, &chunkRead, &overlapped) == 0)
        {
            const DWORD err = ::GetLastError();
            if (err == ERROR_HANDLE_EOF)
            {
                break;
            }
            return gp::makeUnexpected(windows::detail::mapWin32Error(err));
        }

        if (chunkRead == 0u)
        {
            break;
        }
        totalRead += static_cast<gp::USize>(chunkRead);
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

    const HANDLE rawHandle = static_cast<HANDLE>(m_descriptor);
    const gp::Byte* src = static_cast<const gp::Byte*>(buffer);
    gp::USize totalWritten = 0u;

    while (totalWritten < bytesToWrite)
    {
        const gp::UInt64 currentOffset = offset + static_cast<gp::UInt64>(totalWritten);
        const gp::USize remaining = bytesToWrite - totalWritten;
        const DWORD chunkToWrite =
            (remaining > static_cast<gp::USize>(0x7FFFFFFFu)) ? 0x7FFFFFFFu : static_cast<DWORD>(remaining);

        OVERLAPPED overlapped{};
        overlapped.Offset = static_cast<DWORD>(currentOffset & 0xFFFFFFFFULL);
        overlapped.OffsetHigh = static_cast<DWORD>(currentOffset >> 32u);

        DWORD chunkWritten = 0u;
        if (::WriteFile(rawHandle, src + totalWritten, chunkToWrite, &chunkWritten, &overlapped) == 0)
        {
            return gp::makeUnexpected(windows::detail::mapWin32Error(::GetLastError()));
        }

        if (chunkWritten == 0u)
        {
            break;
        }
        totalWritten += static_cast<gp::USize>(chunkWritten);
    }

    return totalWritten;
}

gp::Expected<gp::UInt64, FileError> FileHandle::seek(const gp::Int64 offset, const FileSeekOrigin origin) noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    DWORD moveMethod = FILE_BEGIN;
    switch (origin)
    {
    case FileSeekOrigin::Begin:
        moveMethod = FILE_BEGIN;
        break;
    case FileSeekOrigin::Current:
        moveMethod = FILE_CURRENT;
        break;
    case FileSeekOrigin::End:
        moveMethod = FILE_END;
        break;
    default:
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    LARGE_INTEGER distance{};
    distance.QuadPart = offset;
    LARGE_INTEGER newPos{};

    if (::SetFilePointerEx(static_cast<HANDLE>(m_descriptor), distance, &newPos, moveMethod) == 0)
    {
        return gp::makeUnexpected(windows::detail::mapWin32Error(::GetLastError()));
    }

    return static_cast<gp::UInt64>(newPos.QuadPart);
}

gp::Expected<gp::UInt64, FileError> FileHandle::tell() const noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    const LARGE_INTEGER zero{};
    LARGE_INTEGER currentPos{};
    if (::SetFilePointerEx(static_cast<HANDLE>(m_descriptor), zero, &currentPos, FILE_CURRENT) == 0)
    {
        return gp::makeUnexpected(windows::detail::mapWin32Error(::GetLastError()));
    }

    return static_cast<gp::UInt64>(currentPos.QuadPart);
}

gp::Expected<gp::UInt64, FileError> FileHandle::size() const noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    LARGE_INTEGER fileSize{};
    if (::GetFileSizeEx(static_cast<HANDLE>(m_descriptor), &fileSize) == 0)
    {
        return gp::makeUnexpected(windows::detail::mapWin32Error(::GetLastError()));
    }

    return static_cast<gp::UInt64>(fileSize.QuadPart);
}

gp::Expected<bool, FileError> FileHandle::flush() noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    if (::FlushFileBuffers(static_cast<HANDLE>(m_descriptor)) == 0)
    {
        return gp::makeUnexpected(windows::detail::mapWin32Error(::GetLastError()));
    }

    return true;
}

gp::Expected<bool, FileError> FileHandle::truncate(const gp::UInt64 newSize) noexcept
{
    if (!isValid())
    {
        return gp::makeUnexpected(FileError::InvalidHandle);
    }

    const gp::Expected<gp::UInt64, FileError> savedPos = tell();
    if (!savedPos.hasValue())
    {
        return gp::makeUnexpected(savedPos.error());
    }

    const gp::Expected<gp::UInt64, FileError> seekRes = seek(static_cast<gp::Int64>(newSize), FileSeekOrigin::Begin);
    if (!seekRes.hasValue())
    {
        return gp::makeUnexpected(seekRes.error());
    }

    if (::SetEndOfFile(static_cast<HANDLE>(m_descriptor)) == 0)
    {
        const DWORD err = ::GetLastError();
        (void)seek(static_cast<gp::Int64>(savedPos.value()), FileSeekOrigin::Begin);
        return gp::makeUnexpected(windows::detail::mapWin32Error(err));
    }

    const gp::UInt64 restoredPos = (savedPos.value() <= newSize) ? savedPos.value() : newSize;
    (void)seek(static_cast<gp::Int64>(restoredPos), FileSeekOrigin::Begin);
    return true;
}

}   // namespace gp::platform

namespace gp::platform::windows
{

gp::Expected<FileHandle, FileError> FileSystem::open(
    const gp::StringView path, const FileAccessMode access, const FileOpenMode openMode, const FileShareMode shareMode
) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, false))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    DWORD desiredAccess = 0u;
    if (gp::enums::hasAllFlags(access, FileAccessMode::Read))
    {
        desiredAccess |= GENERIC_READ;
    }
    if (gp::enums::hasAllFlags(access, FileAccessMode::Write))
    {
        // For Append mode without read access, Win32 FILE_APPEND_DATA guarantees kernel-atomic end-of-file writes
        // matching POSIX O_APPEND even when multiple handles append concurrently.
        if (openMode == FileOpenMode::Append && !gp::enums::hasAllFlags(access, FileAccessMode::Read))
        {
            desiredAccess |= FILE_APPEND_DATA;
        }
        else
        {
            desiredAccess |= GENERIC_WRITE;
        }
    }
    if (desiredAccess == 0u)
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    DWORD winShare = 0u;
    if (gp::enums::hasAllFlags(shareMode, FileShareMode::Read))
    {
        winShare |= FILE_SHARE_READ;
    }
    if (gp::enums::hasAllFlags(shareMode, FileShareMode::Write))
    {
        winShare |= FILE_SHARE_WRITE;
    }
    if (gp::enums::hasAllFlags(shareMode, FileShareMode::Delete))
    {
        winShare |= FILE_SHARE_DELETE;
    }

    DWORD creationDisposition = OPEN_EXISTING;
    switch (openMode)
    {
    case FileOpenMode::OpenExisting:
        creationDisposition = OPEN_EXISTING;
        break;
    case FileOpenMode::CreateAlways:
        creationDisposition = CREATE_ALWAYS;
        break;
    case FileOpenMode::CreateNew:
        creationDisposition = CREATE_NEW;
        break;
    case FileOpenMode::OpenAlways:
    case FileOpenMode::Append:
        creationDisposition = OPEN_ALWAYS;
        break;
    default:
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    const HANDLE rawHandle =
        ::CreateFileW(widePath, desiredAccess, winShare, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (rawHandle == INVALID_HANDLE_VALUE)
    {
        const DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        {
            const DWORD attrs = ::GetFileAttributesW(widePath);
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0u)
            {
                return gp::makeUnexpected(FileError::IsDirectory);
            }
        }
        return gp::makeUnexpected(detail::mapWin32Error(err));
    }

    FileHandle handle(static_cast<NativeFileDescriptor>(rawHandle));
    if (openMode == FileOpenMode::Append && gp::enums::hasAllFlags(access, FileAccessMode::Read))
    {
        const gp::Expected<gp::UInt64, FileError> seekEnd = handle.seek(0, FileSeekOrigin::End);
        if (!seekEnd.hasValue())
        {
            return gp::makeUnexpected(seekEnd.error());
        }
    }

    return handle;
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
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrData{};
    if (::GetFileAttributesExW(widePath, GetFileExInfoStandard, &attrData) == 0)
    {
        return gp::makeUnexpected(detail::mapWin32Error(::GetLastError()));
    }

    return detail::buildStatsFromWin32Data(
        attrData.dwFileAttributes,
        attrData.nFileSizeHigh,
        attrData.nFileSizeLow,
        attrData.ftCreationTime,
        attrData.ftLastWriteTime,
        attrData.ftLastAccessTime
    );
}

bool FileSystem::exists(const gp::StringView path) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return false;
    }

    return ::GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES;
}

bool FileSystem::isFile(const gp::StringView path) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return false;
    }

    const DWORD attrs = ::GetFileAttributesW(widePath);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0u;
}

bool FileSystem::isDirectory(const gp::StringView path) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return false;
    }

    const DWORD attrs = ::GetFileAttributesW(widePath);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0u;
}

bool FileSystem::isReadOnly(const gp::StringView path) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return false;
    }

    const DWORD attrs = ::GetFileAttributesW(widePath);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY) != 0u;
}

gp::Expected<bool, FileError> FileSystem::setReadOnly(const gp::StringView path, const bool readOnly) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    DWORD attrs = ::GetFileAttributesW(widePath);
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return gp::makeUnexpected(detail::mapWin32Error(::GetLastError()));
    }

    if (readOnly)
    {
        attrs |= FILE_ATTRIBUTE_READONLY;
    }
    else
    {
        attrs &= ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);
        if (attrs == 0u)
        {
            attrs = FILE_ATTRIBUTE_NORMAL;
        }
    }

    if (::SetFileAttributesW(widePath, attrs) == 0)
    {
        return gp::makeUnexpected(detail::mapWin32Error(::GetLastError()));
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
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    if (::CreateDirectoryW(widePath, nullptr) == 0)
    {
        const DWORD err = ::GetLastError();
        if (err == ERROR_ALREADY_EXISTS)
        {
            const DWORD attrs = ::GetFileAttributesW(widePath);
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0u)
            {
                return true;
            }
            return gp::makeUnexpected(FileError::AlreadyExists);
        }
        return gp::makeUnexpected(detail::mapWin32Error(err));
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::createDirectoryTree(const gp::StringView path) noexcept
{
    return createDirectoryTreeImpl<FileSystem>(path);
}

gp::Expected<bool, FileError> FileSystem::removeFile(const gp::StringView path) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, false))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    if (::DeleteFileW(widePath) == 0)
    {
        const DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        {
            const DWORD attrs = ::GetFileAttributesW(widePath);
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0u)
            {
                return gp::makeUnexpected(FileError::IsDirectory);
            }
        }
        return gp::makeUnexpected(detail::mapWin32Error(err));
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::removeDirectory(const gp::StringView path) noexcept
{
    wchar_t widePath[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(path, widePath, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    const DWORD attrs = ::GetFileAttributesW(widePath);
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return gp::makeUnexpected(detail::mapWin32Error(::GetLastError()));
    }
    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0u)
    {
        return gp::makeUnexpected(FileError::NotDirectory);
    }

    if (::RemoveDirectoryW(widePath) == 0)
    {
        return gp::makeUnexpected(detail::mapWin32Error(::GetLastError()));
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
    wchar_t wideSrc[detail::kWin32WidePathCapacity]{};
    wchar_t wideDst[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(sourcePath, wideSrc, true) || !detail::toWin32WidePath(destinationPath, wideDst, true))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    const DWORD flags = overwrite ? (MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) : 0u;
    if (::MoveFileExW(wideSrc, wideDst, flags) == 0)
    {
        const DWORD err = ::GetLastError();
        if (!overwrite && err == ERROR_ALREADY_EXISTS)
        {
            return gp::makeUnexpected(FileError::AlreadyExists);
        }
        return gp::makeUnexpected(detail::mapWin32Error(err));
    }

    return true;
}

gp::Expected<bool, FileError> FileSystem::copyFile(
    const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite
) noexcept
{
    wchar_t wideSrc[detail::kWin32WidePathCapacity]{};
    wchar_t wideDst[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(sourcePath, wideSrc, false) ||
        !detail::toWin32WidePath(destinationPath, wideDst, false))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    if (Paths::equals(sourcePath, destinationPath))
    {
        return gp::makeUnexpected(FileError::InvalidArgument);
    }

    if (::CopyFileW(wideSrc, wideDst, overwrite ? FALSE : TRUE) == 0)
    {
        const DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        {
            const DWORD srcAttrs = ::GetFileAttributesW(wideSrc);
            if (srcAttrs != INVALID_FILE_ATTRIBUTES && (srcAttrs & FILE_ATTRIBUTE_DIRECTORY) != 0u)
            {
                return gp::makeUnexpected(FileError::IsDirectory);
            }
        }
        return gp::makeUnexpected(detail::mapWin32Error(err));
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
    if (directoryPath.isEmpty())
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    const gp::Expected<FileStats, FileError> dirStats = getStats(directoryPath);
    if (!dirStats.hasValue())
    {
        return gp::makeUnexpected(dirStats.error());
    }
    if (!dirStats.value().isDirectory())
    {
        return gp::makeUnexpected(FileError::NotDirectory);
    }

    PathBuffer baseDir(directoryPath);
    Paths::normalizeDirectoryName(baseDir);

    PathBuffer searchPattern = Paths::combine(baseDir.view(), "*");
    wchar_t widePattern[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(searchPattern.view(), widePattern, false))
    {
        return gp::makeUnexpected(FileError::InvalidPath);
    }

    WIN32_FIND_DATAW findData{};
    const HANDLE findHandle = ::FindFirstFileExW(
        widePattern, FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH
    );

    if (findHandle == INVALID_HANDLE_VALUE)
    {
        const DWORD err = ::GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
        {
            return true;
        }
        return gp::makeUnexpected(detail::mapWin32Error(err));
    }

    do
    {
        const wchar_t* const wName = findData.cFileName;
        if (wName[0] == L'.' && (wName[1] == L'\0' || (wName[1] == L'.' && wName[2] == L'\0')))
        {
            continue;
        }

        char utf8Name[kMaxPathLength + 1u]{};
        const int utf8Len = ::WideCharToMultiByte(
            CP_UTF8, 0u, wName, -1, utf8Name, static_cast<int>(kMaxPathLength + 1u), nullptr, nullptr
        );
        if (utf8Len <= 1)
        {
            continue;
        }

        const gp::StringView leafName(utf8Name, static_cast<gp::USize>(utf8Len - 1));
        const PathBuffer fullEntryPath = Paths::combine(baseDir.view(), leafName);

        DirectoryEntry entry{};
        entry.path = fullEntryPath.view();
        entry.name = Paths::getCleanFilename(entry.path);
        entry.stats = detail::buildStatsFromWin32Data(
            findData.dwFileAttributes,
            findData.nFileSizeHigh,
            findData.nFileSizeLow,
            findData.ftCreationTime,
            findData.ftLastWriteTime,
            findData.ftLastAccessTime
        );
        entry.depth = 0u;

        const DirectoryVisitControl ctrl = visitor(entry, userData);
        if (ctrl == DirectoryVisitControl::Stop)
        {
            break;
        }
    }
    while (::FindNextFileW(findHandle, &findData) != 0);

    ::FindClose(findHandle);
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

}   // namespace gp::platform::windows
