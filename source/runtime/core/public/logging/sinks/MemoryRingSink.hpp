// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogEnums.hpp"
#include "logging/LogTypes.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::log
{

/// @brief Fixed-capacity, zero-allocation in-memory circular log sink for unit testing, crash history inspection, and
///        in-engine developer overlays ([ADR-0054]).
/// @details Retains the most recent `kCapacity` (`64`) `LogEntry` records in a circular inline array while tracking
///          total written and flushed counts via atomic counters.
class GP_CORE_API MemoryRingSink final : public ILogSink
{
public:
    /// @brief Number of `LogEntry` records retained in the circular history buffer (power of two).
    static constexpr gp::USize kCapacity = 64u;

private:
    gp::thread::Atomic<gp::USize> m_writeCount{ 0u };
    gp::thread::Atomic<gp::USize> m_flushCount{ 0u };
    LogEntry m_entries[kCapacity]{};

public:
    /// @brief Constructs an empty `MemoryRingSink` with the specified minimum severity filter.
    /// @param[in] minLevel Minimum `LogLevel` captured by the ring buffer (`LogLevel::Trace` by default).
    [[nodiscard]] explicit MemoryRingSink(const LogLevel minLevel = LogLevel::Trace) noexcept;

    /// @brief Virtual destructor.
    virtual ~MemoryRingSink() noexcept override = default;

public:
    /// @copydoc ILogSink::write()
    virtual void write(const LogRecordView& record) noexcept override;

    /// @copydoc ILogSink::flush()
    virtual void flush() noexcept override;

    /// @brief Clears all stored entries and resets `getTotalWriteCount()` and `getFlushCount()` to zero.
    void clear() noexcept;

    /// @brief Queries the total number of records written to this sink since construction or `clear()`.
    /// @return Cumulative write count (may exceed `kCapacity` if the ring has wrapped).
    [[nodiscard]] constexpr gp::USize getTotalWriteCount() const noexcept
    {
        return m_writeCount.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Queries the number of retained records currently accessible in the circular buffer (`<= kCapacity`).
    /// @return Retained entry count in `[0, kCapacity]`.
    [[nodiscard]] constexpr gp::USize getSize() const noexcept
    {
        const gp::USize total = m_writeCount.load(gp::MemoryOrder::Acquire);
        return (total < kCapacity) ? total : kCapacity;
    }

    /// @brief Queries the number of times `flush()` has been invoked on this sink.
    /// @return Total flush count.
    [[nodiscard]] constexpr gp::USize getFlushCount() const noexcept
    {
        return m_flushCount.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Retrieves the retained `LogEntry` at chronological index `index` (`0` = oldest retained entry,
    ///        `getSize() - 1` = newest entry).
    /// @param[in] index Chronological index in `[0, getSize())`.
    /// @return Const reference to the stored `LogEntry`.
    [[nodiscard]] const LogEntry& getEntry(const gp::USize index) const noexcept;

    /// @brief Retrieves a `LogRecordView` of the most recently written entry.
    /// @return `LogRecordView` of the latest entry, or a default-constructed view if `getSize() == 0`.
    [[nodiscard]] LogRecordView getLastRecord() const noexcept;
};

}   // namespace gp::log
