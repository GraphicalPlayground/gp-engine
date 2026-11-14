// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogDesc.hpp"
#include "logging/LogEnums.hpp"
#include "platforms/base/system/PathTypes.hpp"

namespace gp::log
{

/// @brief Buffered, zero-heap-allocation file output sink backed by `gp::platform::Paths` ([ADR-0054]).
/// @details Accumulates formatted log lines inside an inline `8 KiB` write buffer (`m_writeBuffer`) to minimize OS
///          write syscalls, flushing automatically when the buffer fills, when `record.level >= LogLevel::Error`, or
///          when `flush()` / `close()` is called.
class GP_CORE_API FileSink final : public ILogSink
{
public:
    /// @brief Capacity in bytes of the inline user-space write buffer (`8 KiB`).
    static constexpr gp::USize kWriteBufferCapacity = 8192u;

private:
    void* m_fileHandle{ nullptr };
    gp::platform::PathBuffer m_filePath{};
    LogSinkFlags m_flags{ LogSinkFlags::DefaultTextFormat | LogSinkFlags::IncludeSourceLocation };
    gp::USize m_bufferedBytes{ 0u };
    gp::UInt64 m_totalBytesWritten{ 0ULL };
    char m_writeBuffer[kWriteBufferCapacity]{};

public:
    /// @brief Constructs an unopened `FileSink` (`isOpen() == false`).
    [[nodiscard]] FileSink() noexcept = default;

    /// @brief Constructs and immediately opens a `FileSink` using the provided descriptor.
    /// @param[in] desc File sink configuration (`filePath`, `minLevel`, `flags`, `truncateOnOpen`).
    explicit FileSink(const FileSinkDesc& desc) noexcept;

    /// @brief Destructor that flushes any remaining buffered bytes and closes the OS file handle.
    virtual ~FileSink() noexcept override;

public:
    /// @brief Opens (or re-opens) the target log file specified in `desc`.
    /// @details If `desc.filePath` is empty, resolves `<tempDir>/<ExecutableName>.log` via `gp::platform::Paths`.
    /// @param[in] desc File sink configuration parameters.
    /// @return `LogResult::Success` if the file was opened, or `LogResult::IoError` on failure.
    [[nodiscard]] LogResult open(const FileSinkDesc& desc) noexcept;

    /// @brief Flushes any buffered output and closes the underlying file handle.
    void close() noexcept;

    /// @brief Checks whether the log file is currently open for writing.
    /// @return True if open, false otherwise.
    [[nodiscard]] constexpr bool isOpen() const noexcept
    {
        return m_fileHandle != nullptr;
    }

    /// @brief Queries the normalized filesystem path of the opened log file.
    /// @return `StringView` of the resolved file path (empty if never opened).
    [[nodiscard]] constexpr gp::StringView getFilePath() const noexcept
    {
        return m_filePath.view();
    }

    /// @brief Queries the total number of bytes flushed to disk since `open()`.
    /// @return Cumulative byte count written to the file.
    [[nodiscard]] constexpr gp::UInt64 getTotalBytesWritten() const noexcept
    {
        return m_totalBytesWritten;
    }

    /// @brief Queries the number of bytes currently staged in the inline write buffer awaiting flush.
    /// @return Buffered byte count in `[0, kWriteBufferCapacity)`.
    [[nodiscard]] constexpr gp::USize getBufferedBytes() const noexcept
    {
        return m_bufferedBytes;
    }

    /// @copydoc ILogSink::write()
    virtual void write(const LogRecordView& record) noexcept override;

    /// @copydoc ILogSink::flush()
    virtual void flush() noexcept override;

private:
    void flushInternalBuffer() noexcept;
};

}   // namespace gp::log
