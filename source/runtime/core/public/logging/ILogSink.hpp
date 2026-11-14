// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/LogEnums.hpp"
#include "logging/LogTypes.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::log
{

/// @brief Abstract multi-sink output contract for `gp::log::Logger` ([ADR-0054]).
/// @details Concrete implementations (`ConsoleSink`, `FileSink`, `DebugOutputSink`, `TracySink`, `MemoryRingSink`)
///          receive read-only `LogRecordView` packets serialized by `Logger` (either from the background flush thread
///          in `AsyncRing` mode or under the router spinlock in `Synchronous` mode).
class GP_CORE_API ILogSink
{
private:
    gp::thread::Atomic<LogLevel> m_minLevel{ LogLevel::Trace };

public:
    /// @brief Constructs a sink with an initial minimum severity filter.
    /// @param[in] minLevel Minimum `LogLevel` accepted by this sink (`LogLevel::Trace` by default).
    [[nodiscard]] constexpr explicit ILogSink(const LogLevel minLevel = LogLevel::Trace) noexcept
        : m_minLevel(minLevel)
    {}

    ILogSink(const ILogSink&) = delete;
    ILogSink& operator=(const ILogSink&) = delete;
    ILogSink(ILogSink&&) = delete;
    ILogSink& operator=(ILogSink&&) = delete;

    /// @brief Virtual destructor.
    virtual ~ILogSink() noexcept = default;

public:
    /// @brief Writes a single formatted log record to the underlying output target.
    /// @note Invoked by `Logger` strictly sequentially (never concurrently from multiple threads simultaneously).
    /// @param[in] record Non-owning view of the log record metadata and formatted UTF-8 message.
    virtual void write(const LogRecordView& record) noexcept = 0;

    /// @brief Flushes any internal user-space or OS buffers held by the sink.
    virtual void flush() noexcept = 0;

    /// @brief Atomically queries the minimum severity threshold configured for this sink.
    /// @return Active minimum `LogLevel`.
    [[nodiscard]] constexpr LogLevel getMinLevel() const noexcept
    {
        return m_minLevel.load(gp::MemoryOrder::Relaxed);
    }

    /// @brief Atomically updates the minimum severity threshold for this sink.
    /// @param[in] level New minimum `LogLevel` (`LogLevel::Off` disables the sink without unregistering it).
    constexpr void setMinLevel(const LogLevel level) noexcept
    {
        m_minLevel.store(level, gp::MemoryOrder::Relaxed);
    }

    /// @brief Checks whether this sink accepts a record of the specified `level`.
    /// @param[in] level Candidate record severity level.
    /// @return True if `level >= getMinLevel()` and `getMinLevel() != LogLevel::Off`.
    [[nodiscard]] constexpr bool shouldWrite(const LogLevel level) const noexcept
    {
        const LogLevel minLvl = m_minLevel.load(gp::MemoryOrder::Relaxed);
        if (minLvl == LogLevel::Off || level == LogLevel::Off)
        {
            return false;
        }
        return static_cast<gp::UInt8>(level) >= static_cast<gp::UInt8>(minLvl);
    }
};

}   // namespace gp::log
