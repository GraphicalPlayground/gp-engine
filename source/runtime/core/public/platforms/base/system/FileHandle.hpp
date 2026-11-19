// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileTypes.hpp"
#include "templates/Expected.hpp"

namespace gp::platform
{

/// @brief Non-polymorphic, zero-allocation RAII operating system file handle (ADR-0060).
/// @details Wraps a native OS descriptor (`HANDLE` on Windows, `int fd` on POSIX) directly in an 8-byte value type
///          with zero heap allocations and zero virtual dispatch overhead. Exposes both sequential cursor I/O
///          (`read`, `write`, `seek`, `tell`) and lock-free positional I/O (`readAt`, `writeAt` via Win32 `OVERLAPPED`
///          offsets and POSIX `pread`/`pwrite`) so multiple worker fibers can stream disjoint chunks from a single
///          open handle concurrently without mutex contention.
class GP_CORE_API FileHandle final
{
public:
    /// @brief Default-constructs an unopened, invalid `FileHandle`.
    [[nodiscard]] FileHandle() noexcept = default;

    /// @brief Constructs a `FileHandle` taking ownership of an open `NativeFileDescriptor`.
    /// @param[in] descriptor Raw OS file handle/descriptor to adopt.
    [[nodiscard]] explicit FileHandle(const NativeFileDescriptor descriptor) noexcept
        : m_descriptor(descriptor)
    {}

    /// @brief Destructor. Automatically closes the underlying OS file descriptor if valid.
    ~FileHandle() noexcept
    {
        if (isValid())
        {
            close();
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    /// @brief Move constructor. Transfers ownership of the native OS file descriptor from `other`.
    /// @param[in,out] other Source handle to move from; left in a closed (`kInvalidNativeFileDescriptor`) state.
    [[nodiscard]] FileHandle(FileHandle&& other) noexcept
        : m_descriptor(other.m_descriptor)
    {
        other.m_descriptor = kInvalidNativeFileDescriptor;
    }

    /// @brief Move assignment operator. Closes any currently held descriptor and adopts `other`'s descriptor.
    /// @param[in,out] other Source handle to move from; left in a closed state.
    /// @return Reference to `*this`.
    FileHandle& operator=(FileHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (isValid())
            {
                close();
            }
            m_descriptor = other.m_descriptor;
            other.m_descriptor = kInvalidNativeFileDescriptor;
        }
        return *this;
    }

public:
    /// @brief Checks whether this handle currently owns an open native OS file descriptor.
    /// @return True if the descriptor is valid (`!= kInvalidNativeFileDescriptor`), false otherwise.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_descriptor != kInvalidNativeFileDescriptor;
    }

    /// @brief Boolean conversion operator returning `isValid()`.
    /// @return True if open and valid, false otherwise.
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Returns the underlying native OS file descriptor without releasing ownership.
    /// @return `NativeFileDescriptor` (`HANDLE` on Windows, `Int32` fd on POSIX).
    [[nodiscard]] NativeFileDescriptor nativeDescriptor() const noexcept
    {
        return m_descriptor;
    }

    /// @brief Releases ownership of the underlying native OS file descriptor without closing it.
    /// @return The previously held `NativeFileDescriptor`.
    [[nodiscard]] NativeFileDescriptor release() noexcept
    {
        const NativeFileDescriptor raw = m_descriptor;
        m_descriptor = kInvalidNativeFileDescriptor;
        return raw;
    }

    /// @brief Explicitly closes the underlying OS file descriptor.
    /// @return `Expected<bool, FileError>` containing `true` on success (or if already closed), or a `FileError`.
    gp::Expected<bool, FileError> close() noexcept;

    /// @brief Reads up to `bytesToRead` bytes sequentially from the current file cursor into `outBuffer`.
    /// @param[out] outBuffer Destination memory buffer (must be non-null if `bytesToRead > 0`).
    /// @param[in] bytesToRead Maximum number of bytes to read.
    /// @return `Expected<gp::USize, FileError>` containing the number of bytes actually read (`0` at EOF), or error.
    [[nodiscard]] gp::Expected<gp::USize, FileError> read(void* const outBuffer, const gp::USize bytesToRead) noexcept;

    /// @brief Writes `bytesToWrite` bytes sequentially at the current file cursor from `buffer`.
    /// @param[in] buffer Source memory buffer (must be non-null if `bytesToWrite > 0`).
    /// @param[in] bytesToWrite Number of bytes to write.
    /// @return `Expected<gp::USize, FileError>` containing the total bytes written, or a `FileError`.
    [[nodiscard]] gp::Expected<gp::USize, FileError>
        write(const void* const buffer, const gp::USize bytesToWrite) noexcept;

    /// @brief Performs a thread-safe positional read at an explicit 64-bit byte offset (`OVERLAPPED` / `pread`).
    /// @note Does not require an external mutex when multiple worker fibers read disjoint byte ranges from the same
    ///       open `FileHandle` concurrently (ADR-0060).
    /// @param[in] offset Absolute byte offset from the beginning of the file.
    /// @param[out] outBuffer Destination memory buffer (must be non-null if `bytesToRead > 0`).
    /// @param[in] bytesToRead Maximum number of bytes to read.
    /// @return `Expected<gp::USize, FileError>` containing the number of bytes actually read (`0` at/past EOF).
    [[nodiscard]] gp::Expected<gp::USize, FileError>
        readAt(const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead) const noexcept;

    /// @brief Performs a positional write at an explicit 64-bit byte offset (`OVERLAPPED` / `pwrite`).
    /// @param[in] offset Absolute byte offset from the beginning of the file.
    /// @param[in] buffer Source memory buffer (must be non-null if `bytesToWrite > 0`).
    /// @param[in] bytesToWrite Number of bytes to write.
    /// @return `Expected<gp::USize, FileError>` containing the total bytes written, or a `FileError`.
    [[nodiscard]] gp::Expected<gp::USize, FileError>
        writeAt(const gp::UInt64 offset, const void* const buffer, const gp::USize bytesToWrite) noexcept;

    /// @brief Repositions the sequential file cursor relative to `origin`.
    /// @param[in] offset Signed byte offset relative to `origin`.
    /// @param[in] origin Reference point (`FileSeekOrigin::Begin`, `Current`, or `End`).
    /// @return `Expected<gp::UInt64, FileError>` containing the new absolute byte offset from the start of the file.
    [[nodiscard]] gp::Expected<gp::UInt64, FileError>
        seek(const gp::Int64 offset, const FileSeekOrigin origin = FileSeekOrigin::Begin) noexcept;

    /// @brief Queries the current sequential file cursor offset from the beginning of the file.
    /// @return `Expected<gp::UInt64, FileError>` containing the current byte position.
    [[nodiscard]] gp::Expected<gp::UInt64, FileError> tell() const noexcept;

    /// @brief Queries the current logical byte size of the open file.
    /// @return `Expected<gp::UInt64, FileError>` containing the total file size in bytes.
    [[nodiscard]] gp::Expected<gp::UInt64, FileError> size() const noexcept;

    /// @brief Flushes any buffered kernel pages for this file to non-volatile storage (`FlushFileBuffers` / `fsync`).
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    gp::Expected<bool, FileError> flush() noexcept;

    /// @brief Resizes (truncates or extends) the open file to `newSize` bytes (`SetEndOfFile` / `ftruncate`).
    /// @param[in] newSize Target file size in bytes.
    /// @return `Expected<bool, FileError>` containing `true` on success, or a `FileError`.
    gp::Expected<bool, FileError> truncate(const gp::UInt64 newSize) noexcept;

private:
    NativeFileDescriptor m_descriptor{ kInvalidNativeFileDescriptor };
};

}   // namespace gp::platform
