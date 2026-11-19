// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileHandle.hpp"
#include "platforms/base/system/FileTypes.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "templates/Expected.hpp"
#include "vfs/IMountBackend.hpp"
#include "vfs/VfsEnums.hpp"
#include "vfs/VfsTypes.hpp"
#include <concepts>
#include <type_traits>

namespace gp::vfs
{

/// @brief Layer 1 Priority-Layered Virtual File System and Logical URI Router (`gp::vfs::VirtualFileSystem`)
///        (ADR-0044, ADR-0060).
/// @details Binds logical URI schemes (`engine://`, `game://`, `user://`, `appdata://`, `cache://`, `temp://`) to
///          priority-ordered `IMountBackend` instances (`PhysicalMountBackend`, `MemoryMountBackend`, or custom
///          archive backends). Enforces strict sandbox escape prevention (`VfsError::SandboxEscapeViolation`),
///          priority-ordered read resolution (`DeveloperOverride > ModOverride > Patch > DLC > Base`), single-target
///          write routing to the highest-priority `MountAccess::ReadWrite` mount, and deduplicated multi-mount
///          directory enumeration.
class GP_CORE_API VirtualFileSystem
{
public:
    VirtualFileSystem() = delete;
    ~VirtualFileSystem() = delete;
    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;
    VirtualFileSystem(VirtualFileSystem&&) = delete;
    VirtualFileSystem& operator=(VirtualFileSystem&&) = delete;

public:
    /// @section Lifecycle & Bootstrap Mounting

    /// @brief Mounts the six canonical ADR-0044 URI schemes (`engine://`, `game://`, `user://`, `appdata://`,
    ///        `cache://`, `temp://`) using Layer 0 `gp::platform::Paths`.
    /// @param[in] appName Application subfolder name appended to `documentsDir()`, `appDataDir()`,
    ///                    `localAppDataDir()`, and `tempDir()` (defaults to `Paths::executableName(true)` if empty).
    /// @return `Expected<bool, VfsError>` containing `true` on successful bootstrap, or a `VfsError`.
    static gp::Expected<bool, VfsError> initializeDefaultMounts(const gp::StringView appName = {}) noexcept;

    /// @brief Unmounts all registered physical and custom backends and resets the VFS mount table.
    static void shutdown() noexcept;

    /// @brief Mounts a physical OS directory at `physicalRootDir` onto `scheme` with the specified priority and access.
    /// @param[in] scheme Logical URI scheme (e.g., `"game://"` or `"game"`).
    /// @param[in] physicalRootDir Physical OS directory path.
    /// @param[in] priority Overlay priority tier (`MountPriority::Base`, `ModOverride`, etc.).
    /// @param[in] access Read/Write access policy (`MountAccess::ReadOnly` or `MountAccess::ReadWrite`).
    /// @return `Expected<bool, VfsError>` containing `true` on success, or `VfsError::MountTableFull` / `InvalidUri`.
    static gp::Expected<bool, VfsError> mount(
        const gp::StringView scheme,
        const gp::StringView physicalRootDir,
        const MountPriority priority = MountPriority::Base,
        const MountAccess access = MountAccess::ReadOnly
    ) noexcept;

    /// @brief Mounts an external or custom `IMountBackend` instance (such as `MemoryMountBackend`) onto `scheme`.
    /// @note The caller retains ownership of `*backend`, which must remain alive until unmounted or `shutdown()`.
    /// @param[in] scheme Logical URI scheme (e.g., `"game://"`).
    /// @param[in] backend Non-null pointer to the `IMountBackend` implementation.
    /// @param[in] priority Overlay priority tier.
    /// @param[in] access Read/Write access policy.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError> mountBackend(
        const gp::StringView scheme,
        IMountBackend* const backend,
        const MountPriority priority = MountPriority::Base,
        const MountAccess access = MountAccess::ReadOnly
    ) noexcept;

    /// @brief Unmounts a specific physical root directory or backend identifier from `scheme`.
    /// @param[in] scheme Logical URI scheme.
    /// @param[in] rootPath Physical root directory or backend `rootPath()` identifier to remove.
    /// @return True if a matching mount point was found and removed, false otherwise.
    static bool unmount(const gp::StringView scheme, const gp::StringView rootPath) noexcept;

    /// @brief Unmounts a specific `IMountBackend` pointer from `scheme`.
    /// @param[in] scheme Logical URI scheme.
    /// @param[in] backend Pointer to the `IMountBackend` instance to remove.
    /// @return True if the backend was found and removed, false otherwise.
    static bool unmountBackend(const gp::StringView scheme, const IMountBackend* const backend) noexcept;

    /// @brief Unmounts all mount points currently bound to `scheme`.
    /// @param[in] scheme Logical URI scheme to clear.
    /// @return Number of mount points removed.
    static gp::USize unmountScheme(const gp::StringView scheme) noexcept;

    /// @brief Checks whether at least one mount point is registered for `scheme`.
    /// @param[in] scheme Logical URI scheme (e.g., `"game://"` or `"game"`).
    /// @return True if `scheme` has one or more active mounts, false otherwise.
    [[nodiscard]] static bool isSchemeMounted(const gp::StringView scheme) noexcept;

    /// @brief Returns the total number of active mount points across all schemes (or for a specific `scheme` if
    ///        non-empty).
    /// @param[in] scheme Optional scheme filter (empty view counts all registered mounts).
    /// @return Active mount point count.
    [[nodiscard]] static gp::USize getMountCount(const gp::StringView scheme = {}) noexcept;

public:
    /// @section URI Parsing, Sandbox Validation & Physical Path Resolution

    /// @brief Normalizes a raw scheme string (`"game"`, `"GAME:"`, `"game://"`) into canonical lowercase `"scheme://"`.
    /// @param[in] rawScheme Input scheme string view.
    /// @param[out] outScheme Receives the normalized `"scheme://"` prefix.
    /// @return True if `rawScheme` is a syntactically valid URI scheme name, false otherwise.
    [[nodiscard]] static bool normalizeScheme(const gp::StringView rawScheme, SchemeBuffer& outScheme) noexcept;

    /// @brief Parses a logical URI (`"game://textures/../models/hero.mesh"`) into its canonical scheme and
    ///        sandbox-validated relative path (ADR-0044 Invariant 1).
    /// @param[in] uri Input logical VFS URI.
    /// @return `Expected<ParsedUri, VfsError>` containing the normalized scheme and collapsed relative path, or
    ///         `VfsError::SandboxEscapeViolation` / `VfsError::InvalidUri`.
    [[nodiscard]] static gp::Expected<ParsedUri, VfsError> parseUri(const gp::StringView uri) noexcept;

    /// @brief Resolves a logical URI to a physical OS `PathBuffer` for reading (searching highest-to-lowest priority
    ///        physical mounts where the file exists, or the highest-priority physical mount if none exists yet).
    /// @param[in] uri Logical VFS URI to resolve.
    /// @return `Expected<gp::PathBuffer, VfsError>` containing the resolved physical OS path.
    [[nodiscard]] static gp::Expected<gp::PathBuffer, VfsError> resolvePhysicalPath(const gp::StringView uri) noexcept;

    /// @brief Resolves a logical URI to a physical OS `PathBuffer` on the highest-priority `MountAccess::ReadWrite`
    ///        mount for `uri`'s scheme.
    /// @param[in] uri Logical VFS URI to resolve for writing.
    /// @return `Expected<gp::PathBuffer, VfsError>` containing the writable physical OS path, or
    ///         `VfsError::ReadOnlyMount`.
    [[nodiscard]] static gp::Expected<gp::PathBuffer, VfsError>
        resolveWritablePhysicalPath(const gp::StringView uri) noexcept;

public:
    /// @section VFS Existence & Metadata Queries

    /// @brief Checks whether a file or directory exists at `uri` in any mounted backend for the scheme.
    /// @param[in] uri Logical VFS URI (e.g., `"game://configs/default.ini"`).
    /// @return True if the entry exists in at least one mounted layer, false otherwise.
    [[nodiscard]] static bool exists(const gp::StringView uri) noexcept;

    /// @brief Checks whether `uri` exists and refers to a regular file in the highest-priority matching mount.
    /// @param[in] uri Logical VFS URI.
    /// @return True if `uri` exists and is a regular file, false otherwise.
    [[nodiscard]] static bool isFile(const gp::StringView uri) noexcept;

    /// @brief Checks whether `uri` exists and refers to a directory in any mounted backend for the scheme.
    /// @param[in] uri Logical VFS URI.
    /// @return True if `uri` exists and is a directory, false otherwise.
    [[nodiscard]] static bool isDirectory(const gp::StringView uri) noexcept;

    /// @brief Queries `FileStats` for `uri` from the highest-priority mounted backend that contains the entry.
    /// @param[in] uri Logical VFS URI.
    /// @return `Expected<FileStats, VfsError>` containing the entry's metadata, or `VfsError::NotFound`.
    [[nodiscard]] static gp::Expected<gp::platform::FileStats, VfsError> getStats(const gp::StringView uri) noexcept;

    /// @brief Queries the logical byte size of a regular file at `uri` from the highest-priority matching mount.
    /// @param[in] uri Logical VFS URI.
    /// @return `Expected<gp::UInt64, VfsError>` containing the file size in bytes, or a `VfsError`.
    [[nodiscard]] static gp::Expected<gp::UInt64, VfsError> fileSize(const gp::StringView uri) noexcept;

public:
    /// @section VFS File Handle Opening & Mutations

    /// @brief Opens a Layer 0 `FileHandle` for reading from the highest-priority physical mount containing `uri`.
    /// @param[in] uri Logical VFS URI to open.
    /// @param[in] shareMode Concurrent sharing permissions (defaults to `FileShareMode::ReadWrite`).
    /// @return `Expected<FileHandle, VfsError>` containing the open read-only `FileHandle`.
    [[nodiscard]] static gp::Expected<gp::platform::FileHandle, VfsError> openRead(
        const gp::StringView uri, const gp::platform::FileShareMode shareMode = gp::platform::FileShareMode::ReadWrite
    ) noexcept;

    /// @brief Opens a Layer 0 `FileHandle` for writing on the highest-priority `MountAccess::ReadWrite` mount for
    ///        `uri`'s scheme.
    /// @param[in] uri Logical VFS URI to open for writing.
    /// @param[in] append If true, appends to the end of the file; if false (default), truncates/creates via
    ///                   `CreateAlways`.
    /// @param[in] allowRead If true, opens with `FileAccessMode::ReadWrite`.
    /// @param[in] shareMode Concurrent sharing permissions (defaults to `FileShareMode::Read`).
    /// @return `Expected<FileHandle, VfsError>` containing the open writable `FileHandle`, or
    /// `VfsError::ReadOnlyMount`.
    [[nodiscard]] static gp::Expected<gp::platform::FileHandle, VfsError> openWrite(
        const gp::StringView uri,
        const bool append = false,
        const bool allowRead = false,
        const gp::platform::FileShareMode shareMode = gp::platform::FileShareMode::Read
    ) noexcept;

    /// @brief Creates a single directory at `uri` on the highest-priority `ReadWrite` mount for the scheme.
    /// @param[in] uri Logical VFS directory URI.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or `VfsError::ReadOnlyMount`.
    static gp::Expected<bool, VfsError> createDirectory(const gp::StringView uri) noexcept;

    /// @brief Recursively creates `uri` and all missing parent directories on the highest-priority `ReadWrite` mount.
    /// @param[in] uri Logical VFS directory tree URI.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or `VfsError::ReadOnlyMount`.
    static gp::Expected<bool, VfsError> createDirectoryTree(const gp::StringView uri) noexcept;

    /// @brief Deletes a regular file at `uri` on the highest-priority `ReadWrite` mount containing it.
    /// @param[in] uri Logical VFS file URI to remove.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or `VfsError::ReadOnlyMount` / `NotFound`.
    static gp::Expected<bool, VfsError> removeFile(const gp::StringView uri) noexcept;

    /// @brief Deletes an empty directory at `uri` on the highest-priority `ReadWrite` mount containing it.
    /// @param[in] uri Logical VFS directory URI to remove.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError> removeDirectory(const gp::StringView uri) noexcept;

    /// @brief Recursively deletes a directory and all its contents at `uri` on the highest-priority `ReadWrite` mount.
    /// @param[in] uri Logical VFS root directory URI to remove.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError> removeDirectoryTree(const gp::StringView uri) noexcept;

    /// @brief Renames or moves an entry from `sourceUri` to `destinationUri`.
    /// @param[in] sourceUri Existing logical VFS URI (must reside on a `ReadWrite` mount).
    /// @param[in] destinationUri Target logical VFS URI (must resolve to the same `ReadWrite` mount or support
    /// copy+rm).
    /// @param[in] overwrite If true (default), replaces any existing target file.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError>
        move(const gp::StringView sourceUri, const gp::StringView destinationUri, const bool overwrite = true) noexcept;

    /// @brief Copies a file from `sourceUri` (any readable mount, including `ReadOnly` `engine://` or `game://`) to
    ///        `destinationUri` (any `ReadWrite` mount such as `user://` or `cache://`).
    /// @param[in] sourceUri Existing logical VFS file URI.
    /// @param[in] destinationUri Target logical VFS file URI on a writable scheme.
    /// @param[in] overwrite If true (default), overwrites any existing file at `destinationUri`.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError> copyFile(
        const gp::StringView sourceUri, const gp::StringView destinationUri, const bool overwrite = true
    ) noexcept;

public:
    /// @section Merged Multi-Mount Directory Enumeration

    /// @brief Iterates over the merged children of `directoryUri` across all mounted layers for the scheme,
    ///        deduplicating shadowed filenames so highest-priority overrides win (ADR-0044).
    /// @param[in] directoryUri Logical VFS directory URI (e.g., `"game://textures"` or `"game://"`).
    /// @param[in] visitor Stateless C callback invoked once per unique child entry.
    /// @param[in] userData Opaque context pointer forwarded to `visitor`.
    /// @return `Expected<bool, VfsError>` containing `true` on completion, or a `VfsError`.
    static gp::Expected<bool, VfsError> iterateDirectory(
        const gp::StringView directoryUri, const VfsDirectoryVisitorFn visitor, void* const userData = nullptr
    ) noexcept;

    /// @brief Iterates over the merged children of `directoryUri` invoking any C++ callable/lambda
    ///        `(const VfsDirectoryEntry&) -> DirectoryVisitControl` with zero heap allocations (ADR-0044, ADR-0060).
    /// @tparam InVisitor Callable matching `gp::platform::DirectoryVisitControl(const VfsDirectoryEntry&)`.
    /// @param[in] directoryUri Logical VFS directory URI.
    /// @param[in,out] visitor Callable invoked once per unique merged child entry.
    /// @return `Expected<bool, VfsError>` containing `true` on completion, or a `VfsError`.
    template <typename InVisitor>
    requires(
        !std::is_convertible_v<InVisitor, VfsDirectoryVisitorFn> &&
        requires(InVisitor& fn, const VfsDirectoryEntry& entry) {
            {
                fn(entry)
            } -> std::same_as<gp::platform::DirectoryVisitControl>;
        }
    ) static gp::Expected<bool, VfsError>
        iterateDirectory(const gp::StringView directoryUri, InVisitor&& visitor) noexcept
    {
        const VfsDirectoryVisitorFn trampoline = [](const VfsDirectoryEntry& entry,
                                                    void* const rawCallable) -> gp::platform::DirectoryVisitControl
        {
            InVisitor* const fn = static_cast<InVisitor*>(rawCallable);
            return (*fn)(entry);
        };
        return iterateDirectory(directoryUri, trampoline, &visitor);
    }

    /// @brief Recursively iterates over `directoryUri` and all merged subdirectories in depth-first pre-order.
    /// @param[in] directoryUri Logical VFS root directory URI.
    /// @param[in] visitor Stateless C callback returning `Continue`, `SkipSubdirectory`, or `Stop`.
    /// @param[in] userData Opaque context pointer forwarded to `visitor`.
    /// @return `Expected<bool, VfsError>` containing `true` on completion, or a `VfsError`.
    static gp::Expected<bool, VfsError> iterateDirectoryRecursive(
        const gp::StringView directoryUri, const VfsDirectoryVisitorFn visitor, void* const userData = nullptr
    ) noexcept;

    /// @brief Recursively iterates over `directoryUri` invoking any C++ callable/lambda
    ///        `(const VfsDirectoryEntry&) -> DirectoryVisitControl` with zero heap allocations.
    /// @tparam InVisitor Callable matching `gp::platform::DirectoryVisitControl(const VfsDirectoryEntry&)`.
    /// @param[in] directoryUri Logical VFS root directory URI.
    /// @param[in,out] visitor Callable invoked once per unique visited entry.
    /// @return `Expected<bool, VfsError>` containing `true` on completion, or a `VfsError`.
    template <typename InVisitor>
    requires(
        !std::is_convertible_v<InVisitor, VfsDirectoryVisitorFn> &&
        requires(InVisitor& fn, const VfsDirectoryEntry& entry) {
            {
                fn(entry)
            } -> std::same_as<gp::platform::DirectoryVisitControl>;
        }
    ) static gp::Expected<bool, VfsError>
        iterateDirectoryRecursive(const gp::StringView directoryUri, InVisitor&& visitor) noexcept
    {
        const VfsDirectoryVisitorFn trampoline = [](const VfsDirectoryEntry& entry,
                                                    void* const rawCallable) -> gp::platform::DirectoryVisitControl
        {
            InVisitor* const fn = static_cast<InVisitor*>(rawCallable);
            return (*fn)(entry);
        };
        return iterateDirectoryRecursive(directoryUri, trampoline, &visitor);
    }

public:
    /// @section Bulk & Crash-Safe Atomic VFS Utilities

    /// @brief Reads up to `bytesToRead` bytes starting at `offset` from `uri` (from the highest-priority mount
    ///        containing `uri`) into `outBuffer`.
    /// @param[in] uri Logical VFS file URI.
    /// @param[in] offset Byte offset from the start of the file.
    /// @param[out] outBuffer Destination memory buffer.
    /// @param[in] bytesToRead Maximum number of bytes to read.
    /// @return `Expected<gp::USize, VfsError>` containing the number of bytes read into `outBuffer`.
    [[nodiscard]] static gp::Expected<gp::USize, VfsError> readBytesAt(
        const gp::StringView uri, const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead
    ) noexcept;

    /// @brief Reads up to `bufferCapacity` bytes from the start of `uri` into `outBuffer`.
    /// @param[in] uri Logical VFS file URI.
    /// @param[out] outBuffer Destination memory buffer.
    /// @param[in] bufferCapacity Maximum number of bytes to read.
    /// @return `Expected<gp::USize, VfsError>` containing the number of bytes read.
    [[nodiscard]] static gp::Expected<gp::USize, VfsError>
        readBytes(const gp::StringView uri, void* const outBuffer, const gp::USize bufferCapacity) noexcept;

    /// @brief Reads an entire binary file from `uri` (highest-priority matching mount) into any 1-byte element
    ///        container (`gp::Vector<gp::Byte>`, `gp::String`, etc.).
    /// @tparam InByteContainer Dynamic byte container type with `sizeof(ValueType) == 1`.
    /// @param[in] uri Logical VFS file URI.
    /// @param[out] outBuffer Destination byte container resized once and populated with the file payload.
    /// @return `Expected<gp::USize, VfsError>` containing the number of bytes read into `outBuffer`.
    template <typename InByteContainer>
    requires(sizeof(typename InByteContainer::ValueType) == 1u)
    static gp::Expected<gp::USize, VfsError> readAllBytes(const gp::StringView uri, InByteContainer& outBuffer) noexcept
    {
        outBuffer.clear();
        const gp::Expected<gp::UInt64, VfsError> sizeRes = fileSize(uri);
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

        const gp::Expected<gp::USize, VfsError> readRes = readBytesAt(uri, 0ULL, outBuffer.data(), byteCount);
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

    /// @brief Reads an entire UTF-8 text file from `uri` (highest-priority matching mount) into any
    ///        `gp::container::BasicString<char, InAllocator>`.
    /// @tparam InAllocator Engine allocator satisfying `gp::concepts::IsAllocator`.
    /// @param[in] uri Logical VFS file URI.
    /// @param[out] outString Destination string resized once and populated with the file payload.
    /// @return `Expected<gp::USize, VfsError>` containing the number of bytes read into `outString`.
    template <gp::concepts::IsAllocator InAllocator>
    static gp::Expected<gp::USize, VfsError>
        readAllText(const gp::StringView uri, gp::container::BasicString<char, InAllocator>& outString) noexcept
    {
        outString.clear();
        const gp::Expected<gp::UInt64, VfsError> sizeRes = fileSize(uri);
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
        const gp::Expected<gp::USize, VfsError> readRes = readBytesAt(uri, 0ULL, outString.data(), byteCount);
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

    /// @brief Writes `sizeInBytes` from `data` to `uri` on the highest-priority `ReadWrite` mount for the scheme.
    /// @param[in] uri Logical VFS file URI.
    /// @param[in] data Source memory buffer.
    /// @param[in] sizeInBytes Number of bytes to write.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or `VfsError::ReadOnlyMount`.
    static gp::Expected<bool, VfsError>
        writeAllBytes(const gp::StringView uri, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @brief Writes a UTF-8 string view `text` to `uri` on the highest-priority `ReadWrite` mount for the scheme.
    /// @param[in] uri Logical VFS file URI.
    /// @param[in] text UTF-8 text payload to write.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError> writeAllText(const gp::StringView uri, const gp::StringView text) noexcept;

    /// @brief Appends `sizeInBytes` from `data` to `uri` on the highest-priority `ReadWrite` mount for the scheme.
    /// @param[in] uri Logical VFS file URI.
    /// @param[in] data Source memory buffer.
    /// @param[in] sizeInBytes Number of bytes to append.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError>
        appendAllBytes(const gp::StringView uri, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @brief Appends a UTF-8 string view `text` to `uri` on the highest-priority `ReadWrite` mount for the scheme.
    /// @param[in] uri Logical VFS file URI.
    /// @param[in] text UTF-8 text payload to append.
    /// @return `Expected<bool, VfsError>` containing `true` on success, or a `VfsError`.
    static gp::Expected<bool, VfsError> appendAllText(const gp::StringView uri, const gp::StringView text) noexcept;

    /// @brief Crash-safe atomic write transaction routed through the highest-priority `ReadWrite` mount for `uri`'s
    ///        scheme (ADR-0044, ADR-0060).
    /// @param[in] uri Logical VFS file URI (e.g., `"user://saves/slot01.sav"`).
    /// @param[in] data Source memory buffer.
    /// @param[in] sizeInBytes Number of bytes to commit atomically.
    /// @return `Expected<bool, VfsError>` containing `true` on atomic commit, or `VfsError::ReadOnlyMount`.
    static gp::Expected<bool, VfsError>
        writeAtomic(const gp::StringView uri, const void* const data, const gp::USize sizeInBytes) noexcept;

    /// @brief Crash-safe atomic text write overload for `gp::StringView` payloads.
    /// @param[in] uri Logical VFS file URI.
    /// @param[in] text UTF-8 text payload to commit atomically.
    /// @return `Expected<bool, VfsError>` containing `true` on atomic commit, or a `VfsError`.
    static gp::Expected<bool, VfsError> writeAtomicText(const gp::StringView uri, const gp::StringView text) noexcept;
};

}   // namespace gp::vfs
