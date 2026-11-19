// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileHandle.hpp"
#include "platforms/base/system/FileTypes.hpp"
#include "platforms/base/system/Paths.hpp"
#include "templates/Expected.hpp"
#include "threading/atomics/Atomic.hpp"
#include <concepts>
#include <type_traits>

namespace gp::platform::generic
{

/// @brief Base contract and portable composite algorithms for Layer 0 OS file and directory utilities (ADR-0060).
/// @details Derived platform classes (`windows::FileSystem`, `unix::FileSystem`, `linux::FileSystem`,
///          `apple::FileSystem`, `macos::FileSystem`) implement the low-level OS syscalls (`open`, `getStats`,
///          `createDirectory`, `removeFile`, `removeDirectory`, `move`, `setReadOnly`, `iterateDirectory`) and inherit
///          portable recursive directory creation/deletion, chunked file copying, bulk file reading/writing, and
///          crash-safe atomic write transactions (`writeAtomic`).
class GP_CORE_API FileSystem
{
public:
    FileSystem() = delete;
    ~FileSystem() = delete;
    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;
    FileSystem(FileSystem&&) = delete;
    FileSystem& operator=(FileSystem&&) = delete;

public:
    /// @section File Handle Creation (Base Fallback Contract)

    /// @brief Opens a native OS `FileHandle` with explicit access, creation disposition, and sharing modes.
    /// @param[in] path Physical UTF-8 filesystem path to open.
    /// @param[in] access Read/Write access flags (`FileAccessMode::Read`, `Write`, or `ReadWrite`).
    /// @param[in] openMode Creation/truncation disposition (`OpenExisting`, `CreateAlways`, `CreateNew`, etc.).
    /// @param[in] shareMode Concurrent handle sharing flags (`FileShareMode::Read`, `ReadWrite`, `All`, `None`).
    /// @return `Expected<FileHandle, FileError>` containing the open RAII handle on success, or a `FileError`.
    [[nodiscard]] static gp::Expected<FileHandle, FileError> open(
        const gp::StringView path,
        const FileAccessMode access,
        const FileOpenMode openMode,
        const FileShareMode shareMode = FileShareMode::Read
    ) noexcept;

    /// @brief Convenience helper opening an existing file strictly for reading (`OpenExisting`, `Share::ReadWrite`).
    /// @param[in] path Physical UTF-8 filesystem path to open.
    /// @param[in] shareMode Concurrent sharing permissions (defaults to `FileShareMode::ReadWrite`).
    /// @return `Expected<FileHandle, FileError>` containing the open read-only `FileHandle`.
    [[nodiscard]] static gp::Expected<FileHandle, FileError>
        openRead(const gp::StringView path, const FileShareMode shareMode = FileShareMode::ReadWrite) noexcept;

    /// @brief Convenience helper opening or creating a file for writing.
    /// @param[in] path Physical UTF-8 filesystem path to open.
    /// @param[in] append If true, opens in `FileOpenMode::Append`; if false (default), truncates via `CreateAlways`.
    /// @param[in] allowRead If true, grants `FileAccessMode::ReadWrite` on the handle; otherwise `Write`.
    /// @param[in] shareMode Concurrent sharing permissions (defaults to `FileShareMode::Read`).
    /// @return `Expected<FileHandle, FileError>` containing the open writable `FileHandle`.
    [[nodiscard]] static gp::Expected<FileHandle, FileError> openWrite(
        const gp::StringView path,
        const bool append = false,
        const bool allowRead = false,
        const FileShareMode shareMode = FileShareMode::Read
    ) noexcept;

public:
    /// @section Metadata & Existence Queries

    /// @brief Queries metadata (`size`, `creationTime`, `lastWriteTime`, `lastAccessTime`, `attributes`) for `path`.
    /// @param[in] path Physical UTF-8 filesystem path to inspect.
    /// @return `Expected<FileStats, FileError>` containing the entry's `FileStats`, or `FileError::NotFound`.
    [[nodiscard]] static gp::Expected<FileStats, FileError> getStats(const gp::StringView path) noexcept;

    /// @brief Checks whether a file or directory exists at `path`.
    /// @param[in] path Physical UTF-8 filesystem path to check.
    /// @return True if any filesystem entry exists at `path`, false otherwise.
    [[nodiscard]] static bool exists(const gp::StringView path) noexcept;

    /// @brief Checks whether `path` exists and refers to a regular data file (not a directory).
    /// @param[in] path Physical UTF-8 filesystem path to check.
    /// @return True if `path` exists and is a regular file, false otherwise.
    [[nodiscard]] static bool isFile(const gp::StringView path) noexcept;

    /// @brief Checks whether `path` exists and refers to a directory.
    /// @param[in] path Physical UTF-8 filesystem path to check.
    /// @return True if `path` exists and is a directory, false otherwise.
    [[nodiscard]] static bool isDirectory(const gp::StringView path) noexcept;

    /// @brief Checks whether `path` exists and is marked read-only.
    /// @param[in] path Physical UTF-8 filesystem path to check.
    /// @return True if `path` exists and has `FileAttributeFlags::ReadOnly`, false otherwise.
    [[nodiscard]] static bool isReadOnly(const gp::StringView path) noexcept;

    /// @brief Sets or clears the read-only attribute on `path`.
    /// @param[in] path Physical UTF-8 filesystem path to modify.
    /// @param[in] readOnly True to mark the entry read-only; false to make it writable.
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    static gp::Expected<bool, FileError> setReadOnly(const gp::StringView path, const bool readOnly) noexcept;

    /// @brief Queries the logical byte size of a regular file at `path`.
    /// @param[in] path Physical UTF-8 file path to inspect.
    /// @return `Expected<gp::UInt64, FileError>` containing the file size in bytes, or `FileError::IsDirectory` /
    ///         `FileError::NotFound`.
    [[nodiscard]] static gp::Expected<gp::UInt64, FileError> fileSize(const gp::StringView path) noexcept;

public:
    /// @section Directory & File Mutations

    /// @brief Creates a single directory at `path` (succeeds idempotently if the directory already exists).
    /// @param[in] path Physical UTF-8 directory path to create (parent directory must already exist).
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    static gp::Expected<bool, FileError> createDirectory(const gp::StringView path) noexcept;

    /// @brief Recursively creates `path` and all missing parent directories (`mkdir -p` semantics) on the stack.
    /// @param[in] path Physical UTF-8 directory tree path to create.
    /// @return `Expected<bool, FileError>` containing `true` once the full directory chain exists, or a `FileError`.
    static gp::Expected<bool, FileError> createDirectoryTree(const gp::StringView path) noexcept;

    /// @brief Deletes a regular file at `path`.
    /// @param[in] path Physical UTF-8 file path to delete.
    /// @return `Expected<bool, FileError>` containing `true` if deleted, or `FileError::NotFound` / `IsDirectory`.
    static gp::Expected<bool, FileError> removeFile(const gp::StringView path) noexcept;

    /// @brief Deletes an empty directory at `path`.
    /// @param[in] path Physical UTF-8 directory path to delete.
    /// @return `Expected<bool, FileError>` containing `true` if removed, or `FileError::DirectoryNotEmpty` /
    ///         `NotFound`.
    static gp::Expected<bool, FileError> removeDirectory(const gp::StringView path) noexcept;

    /// @brief Recursively deletes a directory and all contained files and subdirectories (`rm -rf` semantics).
    /// @param[in] path Physical UTF-8 root directory path to delete.
    /// @return `Expected<bool, FileError>` containing `true` on complete removal, or a `FileError`.
    static gp::Expected<bool, FileError> removeDirectoryTree(const gp::StringView path) noexcept;

    /// @brief Renames or moves a file or directory from `sourcePath` to `destinationPath`.
    /// @param[in] sourcePath Existing physical UTF-8 path.
    /// @param[in] destinationPath Target physical UTF-8 path.
    /// @param[in] overwrite If true (default), atomically replaces an existing file at `destinationPath`; if false,
    ///                      fails with `FileError::AlreadyExists`.
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    static gp::Expected<bool, FileError> move(
        const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite = true
    ) noexcept;

    /// @brief Copies a regular file from `sourcePath` to `destinationPath`.
    /// @param[in] sourcePath Existing source file path.
    /// @param[in] destinationPath Target destination file path.
    /// @param[in] overwrite If true (default), overwrites any existing file at `destinationPath`; if false, fails with
    ///                      `FileError::AlreadyExists`.
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    static gp::Expected<bool, FileError> copyFile(
        const gp::StringView sourcePath, const gp::StringView destinationPath, const bool overwrite = true
    ) noexcept;

public:
    /// @section Zero-Allocation Stack Directory Iteration

    /// @brief Iterates over the immediate children of `directoryPath` with zero heap allocations (ADR-0060).
    /// @param[in] directoryPath Physical UTF-8 directory path to enumerate.
    /// @param[in] visitor Stateless C callback invoked for each entry (`.` and `..` are automatically excluded).
    /// @param[in] userData Opaque context pointer forwarded to `visitor`.
    /// @return `Expected<bool, FileError>` containing `true` on completion, or a `FileError`.
    static gp::Expected<bool, FileError> iterateDirectory(
        const gp::StringView directoryPath, const DirectoryVisitorFn visitor, void* const userData = nullptr
    ) noexcept;

    /// @brief Iterates over the immediate children of `directoryPath` invoking any C++ callable/lambda
    ///        `(const DirectoryEntry&) -> DirectoryVisitControl` with zero heap allocations (ADR-0060).
    /// @tparam InVisitor Callable matching `DirectoryVisitControl(const DirectoryEntry&)`.
    /// @param[in] directoryPath Physical UTF-8 directory path to enumerate.
    /// @param[in,out] visitor Callable invoked for each child entry.
    /// @return `Expected<bool, FileError>` containing `true` on completion, or a `FileError`.
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

    /// @brief Recursively iterates over `directoryPath` and all nested subdirectories in depth-first pre-order.
    /// @param[in] directoryPath Physical UTF-8 root directory path to enumerate.
    /// @param[in] visitor Stateless C callback returning `Continue`, `SkipSubdirectory`, or `Stop`.
    /// @param[in] userData Opaque context pointer forwarded to `visitor`.
    /// @return `Expected<bool, FileError>` containing `true` on completion, or a `FileError`.
    static gp::Expected<bool, FileError> iterateDirectoryRecursive(
        const gp::StringView directoryPath, const DirectoryVisitorFn visitor, void* const userData = nullptr
    ) noexcept;

    /// @brief Recursively iterates over `directoryPath` invoking any C++ callable/lambda
    ///        `(const DirectoryEntry&) -> DirectoryVisitControl` with zero heap allocations (ADR-0060).
    /// @tparam InVisitor Callable matching `DirectoryVisitControl(const DirectoryEntry&)`.
    /// @param[in] directoryPath Physical UTF-8 root directory path to enumerate.
    /// @param[in,out] visitor Callable invoked for each visited entry.
    /// @return `Expected<bool, FileError>` containing `true` on completion, or a `FileError`.
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
    /// @section Bulk & Crash-Safe Atomic File Utilities

    /// @brief Reads up to `bufferCapacity` bytes from `path` into a caller-provided raw memory buffer.
    /// @param[in] path Physical UTF-8 file path to read.
    /// @param[out] outBuffer Destination memory buffer.
    /// @param[in] bufferCapacity Capacity of `outBuffer` in bytes.
    /// @return `Expected<gp::USize, FileError>` containing the number of bytes read into `outBuffer`.
    [[nodiscard]] static gp::Expected<gp::USize, FileError>
        readBytes(const gp::StringView path, void* const outBuffer, const gp::USize bufferCapacity) noexcept;

    /// @brief Writes `sizeInBytes` from `data` to `path`, creating or truncating the file.
    /// @param[in] path Physical UTF-8 file path to write.
    /// @param[in] data Source memory buffer (may be `nullptr` only if `sizeInBytes == 0`).
    /// @param[in] sizeInBytes Number of bytes to write.
    /// @return `Expected<bool, FileError>` containing `true` if all bytes were written, or a `FileError`.
    static gp::Expected<bool, FileError>
        writeAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @brief Writes a UTF-8 string view `text` to `path`, creating or truncating the file.
    /// @param[in] path Physical UTF-8 file path to write.
    /// @param[in] text UTF-8 text content to write.
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    static gp::Expected<bool, FileError> writeAllText(const gp::StringView path, const gp::StringView text) noexcept;

    /// @brief Appends `sizeInBytes` from `data` to the end of `path` (creating the file if it does not exist).
    /// @param[in] path Physical UTF-8 file path to append to.
    /// @param[in] data Source memory buffer.
    /// @param[in] sizeInBytes Number of bytes to append.
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    static gp::Expected<bool, FileError>
        appendAllBytes(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @brief Appends a UTF-8 string view `text` to the end of `path` (creating the file if it does not exist).
    /// @param[in] path Physical UTF-8 file path to append to.
    /// @param[in] text UTF-8 text content to append.
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    static gp::Expected<bool, FileError> appendAllText(const gp::StringView path, const gp::StringView text) noexcept;

    /// @brief Crash-safe atomic write transaction: writes to a temporary sibling file, flushes to disk, and atomically
    ///        renames over `path` (ADR-0060).
    /// @details Guarantees that `path` is never left half-written or truncated to zero bytes if a crash or disk-full
    ///          error occurs mid-write.
    /// @param[in] path Target physical UTF-8 file path to atomically create or replace.
    /// @param[in] data Source memory buffer to write.
    /// @param[in] sizeInBytes Number of bytes to write.
    /// @return `Expected<bool, FileError>` containing `true` once committed and renamed, or a `FileError`.
    static gp::Expected<bool, FileError>
        writeAtomic(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @brief Reads an entire binary file into any 1-byte element container (`gp::Vector<gp::Byte>`, `gp::String`).
    /// @tparam InByteContainer Dynamic byte container type with `sizeof(ValueType) == 1`.
    /// @param[in] path Physical UTF-8 file path to read.
    /// @param[out] outBuffer Destination container resized once and populated with the file bytes.
    /// @return `Expected<gp::USize, FileError>` containing the number of bytes read into `outBuffer`.
    template <typename InByteContainer>
    requires(sizeof(typename InByteContainer::ValueType) == 1u) static gp::Expected<gp::USize, FileError>
        readAllBytes(const gp::StringView path, InByteContainer& outBuffer) noexcept
    {
        return readAllBytesImpl<FileSystem, InByteContainer>(path, outBuffer);
    }

    /// @brief Reads an entire UTF-8 text file into any `gp::container::BasicString<char, InAllocator>` (ADR-0060).
    /// @tparam InAllocator Engine allocator satisfying `gp::concepts::IsAllocator`.
    /// @param[in] path Physical UTF-8 file path to read.
    /// @param[out] outString Destination string resized once via `resizeUninitialized` and populated with file bytes.
    /// @return `Expected<gp::USize, FileError>` containing the number of bytes read into `outString`.
    template <gp::concepts::IsAllocator InAllocator>
    static gp::Expected<gp::USize, FileError>
        readAllText(const gp::StringView path, gp::container::BasicString<char, InAllocator>& outString) noexcept
    {
        return readAllTextImpl<FileSystem, InAllocator>(path, outString);
    }

    /// @brief Crash-safe atomic text write overload for `gp::StringView` payloads (ADR-0060).
    /// @param[in] path Target physical UTF-8 file path to atomically create or replace.
    /// @param[in] text UTF-8 text payload to commit atomically.
    /// @return `Expected<bool, FileError>` containing `true` once committed and renamed, or a `FileError`.
    static gp::Expected<bool, FileError> writeAtomicText(const gp::StringView path, const gp::StringView text) noexcept;

public:
    /// @section Templated Composite & Allocator-Aware Helpers (Resolved on Derived `InSelf`)

    /// @brief Recursively creates `path` and all missing parent directories using `InSelf::createDirectory`.
    /// @tparam InSelf Derived platform `FileSystem` class (`windows::FileSystem`, `unix::FileSystem`, etc.).
    /// @param[in] path Physical UTF-8 directory tree path to create.
    /// @return `Expected<bool, FileError>` containing `true` once the full directory chain exists, or a `FileError`.
    template <typename InSelf>
    static gp::Expected<bool, FileError> createDirectoryTreeImpl(const gp::StringView path) noexcept
    {
        if (path.isEmpty())
        {
            return gp::makeUnexpected(FileError::InvalidPath);
        }

        if (InSelf::isDirectory(path))
        {
            return true;
        }

        PathBuffer normalized(path);
        Paths::normalizeDirectoryName(normalized);
        if (normalized.isEmpty())
        {
            return gp::makeUnexpected(FileError::InvalidPath);
        }

        gp::StringView volume{};
        gp::StringView remainder{};
        const gp::StringView normView = normalized.view();
        Paths::splitVolumeSpecifier(normView, volume, remainder);

        gp::USize scanPos = volume.size();
        while (scanPos < normView.size() && Paths::isSeparator(normView[scanPos]))
        {
            ++scanPos;
        }

        while (scanPos < normView.size())
        {
            gp::USize nextSep = scanPos;
            while (nextSep < normView.size() && !Paths::isSeparator(normView[nextSep]))
            {
                ++nextSep;
            }

            const gp::StringView prefix = normView.substr(0u, nextSep);
            if (!InSelf::isDirectory(prefix))
            {
                const gp::Expected<bool, FileError> mkdirResult = InSelf::createDirectory(prefix);
                if (!mkdirResult.hasValue() && mkdirResult.error() != FileError::AlreadyExists)
                {
                    if (!InSelf::isDirectory(prefix))
                    {
                        return mkdirResult;
                    }
                }
            }

            scanPos = nextSep;
            while (scanPos < normView.size() && Paths::isSeparator(normView[scanPos]))
            {
                ++scanPos;
            }
        }

        return true;
    }

    /// @brief Recursively deletes a directory and all contained files/subdirectories using `InSelf` primitives.
    /// @tparam InSelf Derived platform `FileSystem` class.
    /// @param[in] path Physical UTF-8 root directory path to delete.
    /// @param[in] depth Current recursion depth (aborts with `MaxRecursionDepthExceeded` if `> 32`).
    /// @return `Expected<bool, FileError>` containing `true` on complete removal, or a `FileError`.
    template <typename InSelf>
    static gp::Expected<bool, FileError>
        removeDirectoryTreeImpl(const gp::StringView path, const gp::UInt32 depth = 0u) noexcept
    {
        if (path.isEmpty())
        {
            return gp::makeUnexpected(FileError::InvalidPath);
        }
        if (depth > kMaxDirectoryRecursionDepth)
        {
            return gp::makeUnexpected(FileError::MaxRecursionDepthExceeded);
        }

        const gp::Expected<FileStats, FileError> rootStats = InSelf::getStats(path);
        if (!rootStats.hasValue())
        {
            return gp::makeUnexpected(rootStats.error());
        }
        if (!rootStats.value().isDirectory())
        {
            return gp::makeUnexpected(FileError::NotDirectory);
        }

        struct RemoveTreeContext
        {
            FileError firstError{ FileError::None };
            gp::UInt32 currentDepth{ 0u };
        };

        RemoveTreeContext ctx{};
        ctx.currentDepth = depth;

        const DirectoryVisitorFn visitor = [](const DirectoryEntry& entry, void* const rawCtx) -> DirectoryVisitControl
        {
            RemoveTreeContext* const state = static_cast<RemoveTreeContext*>(rawCtx);
            if (entry.isDirectory() && !entry.stats.isSymbolicLink())
            {
                const gp::Expected<bool, FileError> subRes =
                    removeDirectoryTreeImpl<InSelf>(entry.path, state->currentDepth + 1u);
                if (!subRes.hasValue())
                {
                    state->firstError = subRes.error();
                    return DirectoryVisitControl::Stop;
                }
            }
            else
            {
                if (entry.stats.isReadOnly())
                {
                    (void)InSelf::setReadOnly(entry.path, false);
                }
                const gp::Expected<bool, FileError> rmRes = InSelf::removeFile(entry.path);
                if (!rmRes.hasValue())
                {
                    state->firstError = rmRes.error();
                    return DirectoryVisitControl::Stop;
                }
            }
            return DirectoryVisitControl::Continue;
        };

        const gp::Expected<bool, FileError> iterRes = InSelf::iterateDirectory(path, visitor, &ctx);
        if (!iterRes.hasValue())
        {
            return iterRes;
        }
        if (ctx.firstError != FileError::None)
        {
            return gp::makeUnexpected(ctx.firstError);
        }

        if (rootStats.value().isReadOnly())
        {
            (void)InSelf::setReadOnly(path, false);
        }
        return InSelf::removeDirectory(path);
    }

    /// @brief Recursively traverses `directoryPath` in depth-first pre-order using `InSelf::iterateDirectory`.
    /// @tparam InSelf Derived platform `FileSystem` class.
    /// @param[in] directoryPath Physical UTF-8 root directory path to enumerate.
    /// @param[in] visitor Stateless C callback returning `Continue`, `SkipSubdirectory`, or `Stop`.
    /// @param[in] userData Opaque context pointer forwarded to `visitor`.
    /// @param[in] depth Current recursion depth (`0` at root).
    /// @param[out] outStopped Set to true if `visitor` returned `DirectoryVisitControl::Stop`.
    /// @return `Expected<bool, FileError>` containing `true` on completion, or a `FileError`.
    template <typename InSelf>
    static gp::Expected<bool, FileError> iterateDirectoryRecursiveImpl(
        const gp::StringView directoryPath,
        const DirectoryVisitorFn visitor,
        void* const userData,
        const gp::UInt32 depth,
        bool& outStopped
    ) noexcept
    {
        if (visitor == nullptr)
        {
            return gp::makeUnexpected(FileError::InvalidArgument);
        }
        if (depth > kMaxDirectoryRecursionDepth)
        {
            return gp::makeUnexpected(FileError::MaxRecursionDepthExceeded);
        }

        struct RecursiveWalkState
        {
            DirectoryVisitorFn userVisitor{ nullptr };
            void* userContext{ nullptr };
            gp::UInt32 currentDepth{ 0u };
            FileError childError{ FileError::None };
            bool stopped{ false };
        };

        RecursiveWalkState state{};
        state.userVisitor = visitor;
        state.userContext = userData;
        state.currentDepth = depth;

        const DirectoryVisitorFn stepVisitor = [](const DirectoryEntry& rawEntry,
                                                  void* const rawState) -> DirectoryVisitControl
        {
            RecursiveWalkState* const walk = static_cast<RecursiveWalkState*>(rawState);
            DirectoryEntry entryWithDepth = rawEntry;
            entryWithDepth.depth = walk->currentDepth;

            const DirectoryVisitControl ctrl = walk->userVisitor(entryWithDepth, walk->userContext);
            if (ctrl == DirectoryVisitControl::Stop)
            {
                walk->stopped = true;
                return DirectoryVisitControl::Stop;
            }

            if (entryWithDepth.isDirectory() && !entryWithDepth.stats.isSymbolicLink() &&
                ctrl != DirectoryVisitControl::SkipSubdirectory)
            {
                const gp::Expected<bool, FileError> childRes = iterateDirectoryRecursiveImpl<InSelf>(
                    entryWithDepth.path, walk->userVisitor, walk->userContext, walk->currentDepth + 1u, walk->stopped
                );
                if (!childRes.hasValue())
                {
                    walk->childError = childRes.error();
                    return DirectoryVisitControl::Stop;
                }
                if (walk->stopped)
                {
                    return DirectoryVisitControl::Stop;
                }
            }

            return DirectoryVisitControl::Continue;
        };

        const gp::Expected<bool, FileError> iterRes = InSelf::iterateDirectory(directoryPath, stepVisitor, &state);
        if (!iterRes.hasValue())
        {
            return iterRes;
        }
        if (state.childError != FileError::None)
        {
            return gp::makeUnexpected(state.childError);
        }

        outStopped = state.stopped;
        return true;
    }

    /// @brief Reads an entire binary file into any `gp::concepts::IsAllocator`-compatible byte/element container
    ///        exposing `clear()`, `resizeUninitialized(USize)` (or `resize(USize)`), and `data()` (ADR-0060).
    /// @tparam InSelf Derived platform `FileSystem` class.
    /// @tparam InByteContainer Dynamic byte container type (e.g., `gp::Vector<gp::Byte, Alloc>` or `gp::String`).
    /// @param[in] path Physical UTF-8 file path to read.
    /// @param[out] outBuffer Destination byte container populated with the file payload.
    /// @return `Expected<gp::USize, FileError>` containing the number of bytes read into `outBuffer`.
    template <typename InSelf, typename InByteContainer>
    requires(sizeof(typename InByteContainer::ValueType) == 1u) static gp::Expected<gp::USize, FileError>
        readAllBytesImpl(const gp::StringView path, InByteContainer& outBuffer) noexcept
    {
        outBuffer.clear();
        gp::Expected<FileHandle, FileError> openRes = InSelf::openRead(path);
        if (!openRes.hasValue())
        {
            return gp::makeUnexpected(openRes.error());
        }

        FileHandle& handle = openRes.value();
        const gp::Expected<gp::UInt64, FileError> sizeRes = handle.size();
        if (!sizeRes.hasValue())
        {
            return gp::makeUnexpected(sizeRes.error());
        }

        const gp::USize byteCount = static_cast<gp::USize>(sizeRes.value());
        if (byteCount == 0u)
        {
            return 0u;
        }

        if constexpr (requires { outBuffer.resizeUninitialized(byteCount); })
        {
            outBuffer.resizeUninitialized(byteCount);
        }
        else
        {
            outBuffer.resize(byteCount);
        }

        const gp::Expected<gp::USize, FileError> readRes = handle.read(outBuffer.data(), byteCount);
        if (!readRes.hasValue())
        {
            outBuffer.clear();
            return gp::makeUnexpected(readRes.error());
        }

        if (readRes.value() < byteCount)
        {
            outBuffer.resize(readRes.value());
        }

        return readRes.value();
    }

    /// @brief Reads an entire UTF-8 text file into any `gp::container::BasicString<char, InAllocator>` (ADR-0060).
    /// @tparam InSelf Derived platform `FileSystem` class.
    /// @tparam InAllocator Engine allocator satisfying `gp::concepts::IsAllocator` (e.g., `DefaultAllocator`,
    ///                     `ThreadScratchAllocator`, `LinearAllocator`).
    /// @param[in] path Physical UTF-8 file path to read.
    /// @param[out] outString Destination string resized once via `resizeUninitialized` and populated with file bytes.
    /// @return `Expected<gp::USize, FileError>` containing the number of bytes read into `outString`.
    template <typename InSelf, gp::concepts::IsAllocator InAllocator>
    static gp::Expected<gp::USize, FileError>
        readAllTextImpl(const gp::StringView path, gp::container::BasicString<char, InAllocator>& outString) noexcept
    {
        outString.clear();
        gp::Expected<FileHandle, FileError> openRes = InSelf::openRead(path);
        if (!openRes.hasValue())
        {
            return gp::makeUnexpected(openRes.error());
        }

        FileHandle& handle = openRes.value();
        const gp::Expected<gp::UInt64, FileError> sizeRes = handle.size();
        if (!sizeRes.hasValue())
        {
            return gp::makeUnexpected(sizeRes.error());
        }

        const gp::USize byteCount = static_cast<gp::USize>(sizeRes.value());
        if (byteCount == 0u)
        {
            return 0u;
        }

        outString.resizeUninitialized(byteCount);
        const gp::Expected<gp::USize, FileError> readRes = handle.read(outString.data(), byteCount);
        if (!readRes.hasValue())
        {
            outString.clear();
            return gp::makeUnexpected(readRes.error());
        }

        if (readRes.value() < byteCount)
        {
            outString.resize(readRes.value());
        }

        return readRes.value();
    }

    /// @brief Bridges any C++ callable `(const DirectoryEntry&) -> DirectoryVisitControl` to `InSelf::iterateDirectory`
    ///        via a zero-allocation type-erased trampoline.
    /// @tparam InSelf Derived platform `FileSystem` class.
    /// @tparam InVisitor Callable type.
    /// @param[in] directoryPath Physical UTF-8 directory path to enumerate.
    /// @param[in,out] visitor Callable instance.
    /// @return `Expected<bool, FileError>` from `InSelf::iterateDirectory`.
    template <typename InSelf, typename InVisitor>
    static gp::Expected<bool, FileError>
        iterateDirectoryCallableImpl(const gp::StringView directoryPath, InVisitor& visitor) noexcept
    {
        const DirectoryVisitorFn trampoline = [](const DirectoryEntry& entry,
                                                 void* const rawCallable) -> DirectoryVisitControl
        {
            InVisitor* const fn = static_cast<InVisitor*>(rawCallable);
            return (*fn)(entry);
        };
        return InSelf::iterateDirectory(directoryPath, trampoline, &visitor);
    }

    /// @brief Bridges any C++ callable `(const DirectoryEntry&) -> DirectoryVisitControl` to
    ///        `InSelf::iterateDirectoryRecursive` via a zero-allocation trampoline.
    /// @tparam InSelf Derived platform `FileSystem` class.
    /// @tparam InVisitor Callable type.
    /// @param[in] directoryPath Physical UTF-8 root directory path to enumerate.
    /// @param[in,out] visitor Callable instance.
    /// @return `Expected<bool, FileError>` from `InSelf::iterateDirectoryRecursive`.
    template <typename InSelf, typename InVisitor>
    static gp::Expected<bool, FileError>
        iterateDirectoryRecursiveCallableImpl(const gp::StringView directoryPath, InVisitor& visitor) noexcept
    {
        const DirectoryVisitorFn trampoline = [](const DirectoryEntry& entry,
                                                 void* const rawCallable) -> DirectoryVisitControl
        {
            InVisitor* const fn = static_cast<InVisitor*>(rawCallable);
            return (*fn)(entry);
        };
        return InSelf::iterateDirectoryRecursive(directoryPath, trampoline, &visitor);
    }

    /// @brief Crash-safe atomic write transaction implemented on top of `InSelf` primitives (ADR-0060).
    /// @details Uses a process-wide atomic sequence suffix (`<path>.tmp.<seq>`) so concurrent `writeAtomic()` calls
    ///          targeting the same path from multiple threads never collide on a shared staging filename, flushes
    ///          all dirty kernel pages to non-volatile storage (`FlushFileBuffers` / `fsync`) before closing, and
    ///          atomically swaps the staging file over `path` (`MoveFileExW` / `rename`).
    /// @tparam InSelf Derived platform `FileSystem` class.
    /// @param[in] path Target physical UTF-8 file path to atomically create or replace.
    /// @param[in] data Source memory buffer to write.
    /// @param[in] sizeInBytes Number of bytes to write.
    /// @return `Expected<bool, FileError>` containing `true` once committed and renamed, or a `FileError`.
    template <typename InSelf>
    static gp::Expected<bool, FileError>
        writeAtomicImpl(const gp::StringView path, const void* const data, const gp::USize sizeInBytes) noexcept
    {
        if (path.isEmpty())
        {
            return gp::makeUnexpected(FileError::InvalidPath);
        }
        if (sizeInBytes > 0u && data == nullptr)
        {
            return gp::makeUnexpected(FileError::InvalidArgument);
        }

        static gp::thread::Atomic<gp::UInt64> s_atomicWriteSeq(1ULL);
        const gp::UInt64 seq = s_atomicWriteSeq.fetchAdd(1ULL, gp::memory::MemoryOrder::Relaxed);

        // Format ".tmp.<seq>" on the stack with zero heap allocations and check PathBuffer capacity explicitly.
        char suffixBuf[32]{};
        suffixBuf[0] = '.';
        suffixBuf[1] = 't';
        suffixBuf[2] = 'm';
        suffixBuf[3] = 'p';
        suffixBuf[4] = '.';
        char revDigits[24]{};
        gp::UInt64 tempSeq = seq;
        gp::USize digitCount = 0u;
        do
        {
            revDigits[digitCount++] = static_cast<char>('0' + (tempSeq % 10ULL));
            tempSeq /= 10ULL;
        }
        while (tempSeq > 0ULL);

        gp::USize suffixLen = 5u;
        for (gp::USize i = digitCount; i > 0u; --i)
        {
            suffixBuf[suffixLen++] = revDigits[i - 1u];
        }

        if ((path.size() + suffixLen) > kMaxPathLength)
        {
            return gp::makeUnexpected(FileError::InvalidPath);
        }

        PathBuffer tempPath(path);
        tempPath.append(gp::StringView(suffixBuf, suffixLen));

        {
            gp::Expected<FileHandle, FileError> openRes =
                InSelf::open(tempPath.view(), FileAccessMode::Write, FileOpenMode::CreateNew, FileShareMode::None);
            if (!openRes.hasValue())
            {
                return gp::makeUnexpected(openRes.error());
            }

            FileHandle& handle = openRes.value();
            if (sizeInBytes > 0u)
            {
                const gp::Expected<gp::USize, FileError> writeRes = handle.write(data, sizeInBytes);
                if (!writeRes.hasValue())
                {
                    (void)handle.close();
                    (void)InSelf::removeFile(tempPath.view());
                    return gp::makeUnexpected(writeRes.error());
                }
                if (writeRes.value() != sizeInBytes)
                {
                    (void)handle.close();
                    (void)InSelf::removeFile(tempPath.view());
                    return gp::makeUnexpected(FileError::DiskFull);
                }
            }

            const gp::Expected<bool, FileError> flushRes = handle.flush();
            if (!flushRes.hasValue())
            {
                (void)handle.close();
                (void)InSelf::removeFile(tempPath.view());
                return gp::makeUnexpected(flushRes.error());
            }

            const gp::Expected<bool, FileError> closeRes = handle.close();
            if (!closeRes.hasValue())
            {
                (void)InSelf::removeFile(tempPath.view());
                return gp::makeUnexpected(closeRes.error());
            }
        }

        const gp::Expected<bool, FileError> moveRes = InSelf::move(tempPath.view(), path, true);
        if (!moveRes.hasValue())
        {
            (void)InSelf::removeFile(tempPath.view());
            return gp::makeUnexpected(moveRes.error());
        }

        return true;
    }
};

}   // namespace gp::platform::generic
