// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogDesc.hpp"
#include "logging/LogEnums.hpp"

namespace gp::log
{

/// @brief Standard terminal console output sink (`stdout` / `stderr`) with optional ANSI severity color formatting
///        ([ADR-0054]).
/// @details Formats records into a stack-resident `gp::FixedString<640>` buffer and writes to `stdout` (for `Trace`,
///          `Debug`, and `Info`) or `stderr` (for `Warning`, `Error`, and `Fatal` by default) with zero heap
///          allocations.
class GP_CORE_API ConsoleSink final : public ILogSink
{
private:
    LogLevel m_stderrThreshold{ LogLevel::Warning };
    LogSinkFlags m_flags{ LogSinkFlags::DefaultTextFormat | LogSinkFlags::EnableAnsiColors };

public:
    /// @brief Constructs a `ConsoleSink` from a configuration descriptor.
    /// @param[in] desc Console sink configuration (`minLevel`, `stderrThreshold`, `flags`).
    [[nodiscard]] explicit ConsoleSink(const ConsoleSinkDesc& desc = {}) noexcept;

    /// @brief Virtual destructor flushing output streams.
    virtual ~ConsoleSink() noexcept override;

public:
    /// @copydoc ILogSink::write()
    /// @note Writes directly to OS `stdout` or `stderr` using stack-buffered formatting.
    virtual void write(const LogRecordView& record) noexcept override;

    /// @copydoc ILogSink::flush()
    virtual void flush() noexcept override;

    /// @brief Queries the active formatting flags for this console sink.
    /// @return Active `LogSinkFlags` bitmask.
    [[nodiscard]] constexpr LogSinkFlags getFlags() const noexcept
    {
        return m_flags;
    }

    /// @brief Updates the formatting flags for this console sink.
    /// @param[in] flags New `LogSinkFlags` bitmask.
    constexpr void setFlags(const LogSinkFlags flags) noexcept
    {
        m_flags = flags;
    }

    /// @brief Queries the minimum severity level routed to `stderr` instead of `stdout`.
    /// @return `stderr` threshold `LogLevel`.
    [[nodiscard]] constexpr LogLevel getStderrThreshold() const noexcept
    {
        return m_stderrThreshold;
    }
};

}   // namespace gp::log
