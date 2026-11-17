// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/FixedString.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogEnums.hpp"
#include "logging/LogTypes.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::log
{

/// @brief Zero-allocation in-memory text sink capturing formatted log lines into a contiguous buffer.
/// @details Analogous to Unreal's `FStringOutputDevice`, without dynamic memory allocation. Lines accumulate up to
///          `kCapacity` bytes in an inline fixed buffer. Once full, `isTruncated()` flips to `true` and extra writes
///          append only as many characters as fit cleanly, keeping a trailing null terminator.
class GP_CORE_API StringLogSink final : public ILogSink
{
public:
    /// @brief Maximum UTF-8 character capacity (excluding null terminator) retained in the inline buffer.
    static constexpr gp::USize kCapacity = 16384u;

private:
    gp::FixedString<kCapacity> m_buffer{};
    gp::thread::Atomic<gp::USize> m_writeCount{ 0u };
    gp::thread::Atomic<gp::USize> m_flushCount{ 0u };
    LogSinkFlags m_flags{ LogSinkFlags::None };
    bool m_isTruncated{ false };

public:
    /// @brief Constructs an empty `StringLogSink` with optional level and formatting filters.
    /// @param[in] minLevel Minimum `LogLevel` captured by this sink (`LogLevel::Trace` by default).
    /// @param[in] flags Formatting flags passed to `formatLogRecordLine()` (`LogSinkFlags::None` by default).
    [[nodiscard]] explicit StringLogSink(
        const LogLevel minLevel = LogLevel::Trace, const LogSinkFlags flags = LogSinkFlags::None
    ) noexcept;

    /// @brief Virtual destructor.
    virtual ~StringLogSink() noexcept override = default;

public:
    /// @copydoc ILogSink::write()
    virtual void write(const LogRecordView& record) noexcept override;

    /// @copydoc ILogSink::flush()
    virtual void flush() noexcept override;

    /// @brief Clears all accumulated text, resets truncation state, and zeroes write/flush counters.
    void clear() noexcept;

    /// @brief Returns a non-owning view of the accumulated log text.
    /// @return `gp::StringView` spanning all captured characters.
    [[nodiscard]] gp::StringView view() const noexcept
    {
        return m_buffer.view();
    }

    /// @brief Returns a null-terminated pointer to the accumulated log text.
    /// @return Null-terminated UTF-8 string pointer.
    [[nodiscard]] const char* cString() const noexcept
    {
        return m_buffer.cString();
    }

    /// @brief Returns the number of characters currently stored in the buffer.
    /// @return Current byte length (`<= kCapacity`).
    [[nodiscard]] gp::USize size() const noexcept
    {
        return m_buffer.size();
    }

    /// @brief Returns the maximum character capacity of the inline buffer (`kCapacity`).
    /// @return Fixed capacity in bytes.
    [[nodiscard]] constexpr gp::USize capacity() const noexcept
    {
        return kCapacity;
    }

    /// @brief Returns the remaining free character space before truncation begins.
    /// @return Remaining capacity in bytes.
    [[nodiscard]] gp::USize remainingCapacity() const noexcept
    {
        return kCapacity - m_buffer.size();
    }

    /// @brief Checks whether any characters have been written since construction or `clear()`.
    /// @return True if empty, false otherwise.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_buffer.isEmpty();
    }

    /// @brief Reports whether any write overflowed `kCapacity` and was clipped.
    /// @return True if at least one log line was truncated or dropped due to full capacity.
    [[nodiscard]] bool isTruncated() const noexcept
    {
        return m_isTruncated;
    }

    /// @brief Searches the captured text for a substring.
    /// @param[in] needle Substring to search for.
    /// @return True if `needle` appears in the captured output.
    [[nodiscard]] bool contains(const gp::StringView needle) const noexcept
    {
        return m_buffer.view().contains(needle);
    }

    /// @brief Returns the total number of `write()` calls accepted by this sink since construction or `clear()`.
    /// @return Cumulative accepted write count.
    [[nodiscard]] gp::USize getTotalWriteCount() const noexcept
    {
        return m_writeCount.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Returns the total number of `flush()` calls received since construction or `clear()`.
    /// @return Cumulative flush count.
    [[nodiscard]] gp::USize getFlushCount() const noexcept
    {
        return m_flushCount.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Returns the active `LogSinkFlags` formatting options.
    /// @return Current formatting flags.
    [[nodiscard]] LogSinkFlags getFlags() const noexcept
    {
        return m_flags;
    }

    /// @brief Updates the `LogSinkFlags` used for subsequent `write()` calls.
    /// @param[in] flags New formatting flags.
    void setFlags(const LogSinkFlags flags) noexcept
    {
        m_flags = flags;
    }
};

}   // namespace gp::log
