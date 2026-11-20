// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/generic/system/GenericFileSystem.hpp"

namespace gp::platform::windows
{

/// @brief Win32 implementation of Layer 0 OS file and directory utilities (ADR-0060).
/// @details Uses wide UTF-16 stack buffers (`wchar_t[kMaxPathLength + 1]`) with `CreateFileW`, `OVERLAPPED` positional
///          `ReadFile`/`WriteFile`, `GetFileAttributesExW`, `SetFileAttributesW`, `CreateDirectoryW`, `DeleteFileW`,
///          `RemoveDirectoryW`, `MoveFileExW` (`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`), `CopyFileExW`,
///          and zero-syscall-overhead `FindFirstFileExW` (`FindExInfoBasic`, `FIND_FIRST_EX_LARGE_FETCH`).
class GP_CORE_API FileSystem : public gp::platform::generic::FileSystem
{
public:
    FileSystem() = delete;

public:
    /// @copydoc gp::platform::generic::FileSystem::open()
    /// @note Invokes Win32 `CreateFileW` after converting the UTF-8 path into a stack-allocated UTF-16 buffer.
    [[nodiscard]] static gp::Expected<FileHandle, FileError> open(
        const gp::StringView path,
        const FileAccessMode access,
        const FileOpenMode openMode,
        const FileShareMode shareMode = FileShareMode::Read
    ) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::openRead()
    [[nodiscard]] static gp::Expected<FileHandle, FileError>
        openRead(const gp::StringView path, const FileShareMode shareMode = FileShareMode::ReadWrite) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::openWrite()
    [[nodiscard]] static gp::Expected<FileHandle, FileError> openWrite(
        const gp::StringView path,
        const bool append = false,
        const bool allowRead = false,
        const FileShareMode shareMode = FileShareMode::Read
    ) noexcept;

public:
    /// @copydoc gp::platform::generic::FileSystem::getStats()
    /// @note Queries `GetFileAttributesExW(GetFileExInfoStandard)` and normalizes `FILETIME` ticks to Unix Epoch ns.
    [[nodiscard]] static gp::Expected<FileStats, FileError> getStats(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::exists()
    [[nodiscard]] static bool exists(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::isFile()
    [[nodiscard]] static bool isFile(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::isDirectory()
    [[nodiscard]] static bool isDirectory(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::isReadOnly()
    [[nodiscard]] static bool isReadOnly(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::setReadOnly()
    static gp::Expected<bool, FileError> setReadOnly(const gp::StringView path, const bool readOnly) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::fileSize()
    [[nodiscard]] static gp::Expected<gp::UInt64, FileError> fileSize(const gp::StringView path) noexcept;

public:
    /// @copydoc gp::platform::generic::FileSystem::createDirectory()
    static gp::Expected<bool, FileError> createDirectory(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::createDirectoryTree()
    static gp::Expected<bool, FileError> createDirectoryTree(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::removeFile()
    static gp::Expected<bool, FileError> removeFile(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::removeDirectory()
    static gp::Expected<bool, FileError> removeDirectory(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::removeDirectoryTree()
    static gp::Expected<bool, FileError> removeDirectoryTree(const gp::StringView path) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::move()
    static gp::Expected<bool, FileError> move(
        const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite = true
    ) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::copyFile()
    static gp::Expected<bool, FileError> copyFile(
        const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite = true
    ) noexcept;

public:
    /// @copydoc gp::platform::generic::FileSystem::iterateDirectory()
    /// @note Populates `DirectoryEntry::stats` directly from `WIN32_FIND_DATAW` with zero extra syscalls per entry.
    static gp::Expected<bool, FileError> iterateDirectory(
        const gp::StringView directoryPath, const DirectoryVisitorFn visitor, void* const userData = nullptr
    ) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::iterateDirectory(gp::StringView, InVisitor&&)
    template <typename InVisitor>
    requires(
        !std::is_convertible_v<InVisitor, DirectoryVisitorFn> &&
        requires(InVisitor& fn, const DirectoryEntry& entry) {
            {
                fn(entry)
            } -> std::same_as<DirectoryVisitControl>;
        }
    ) static gp::Expected<bool, FileError>
        iterateDirectory(const gp::StringView directoryPath, InVisitor&& visitor) noexcept
    {
        return iterateDirectoryCallableImpl<FileSystem>(directoryPath, visitor);
    }

    /// @copydoc gp::platform::generic::FileSystem::iterateDirectoryRecursive()
    static gp::Expected<bool, FileError> iterateDirectoryRecursive(
        const gp::StringView directoryPath, const DirectoryVisitorFn visitor, void* const userData = nullptr
    ) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::iterateDirectoryRecursive(gp::StringView, InVisitor&&)
    template <typename InVisitor>
    requires(
        !std::is_convertible_v<InVisitor, DirectoryVisitorFn> &&
        requires(InVisitor& fn, const DirectoryEntry& entry) {
            {
                fn(entry)
            } -> std::same_as<DirectoryVisitControl>;
        }
    ) static gp::Expected<bool, FileError>
        iterateDirectoryRecursive(const gp::StringView directoryPath, InVisitor&& visitor) noexcept
    {
        return iterateDirectoryRecursiveCallableImpl<FileSystem>(directoryPath, visitor);
    }

public:
    /// @copydoc gp::platform::generic::FileSystem::readBytes()
    [[nodiscard]] static gp::Expected<gp::USize, FileError>
        readBytes(const gp::StringView path, void* const outBuffer, const gp::USize bufferCapacity) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::readAllBytes()
    template <typename InByteContainer>
    requires(sizeof(typename InByteContainer::ValueType) == 1u) static gp::Expected<gp::USize, FileError>
        readAllBytes(const gp::StringView path, InByteContainer& outBuffer) noexcept
    {
        return readAllBytesImpl<FileSystem, InByteContainer>(path, outBuffer);
    }

    /// @copydoc gp::platform::generic::FileSystem::readAllText()
    template <gp::concepts::IsAllocator InAllocator>
    static gp::Expected<gp::USize, FileError>
        readAllText(const gp::StringView path, gp::container::BasicString<char, InAllocator>& outString) noexcept
    {
        return readAllTextImpl<FileSystem, InAllocator>(path, outString);
    }

    /// @copydoc gp::platform::generic::FileSystem::writeAllBytes()
    static gp::Expected<bool, FileError>
        writeAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::writeAllText()
    static gp::Expected<bool, FileError> writeAllText(const gp::StringView path, const gp::StringView text) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::appendAllBytes()
    static gp::Expected<bool, FileError>
        appendAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::appendAllText()
    static gp::Expected<bool, FileError> appendAllText(const gp::StringView path, const gp::StringView text) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::writeAtomic()
    static gp::Expected<bool, FileError>
        writeAtomic(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @copydoc gp::platform::generic::FileSystem::writeAtomicText()
    static gp::Expected<bool, FileError> writeAtomicText(const gp::StringView path, const gp::StringView text) noexcept;
};

}   // namespace gp::platform::windows
