// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogEnums.hpp"

namespace gp::log
{

/// @brief Native OS debugger output sink (`OutputDebugStringA` on Windows) ([ADR-0054]).
/// @details Routes formatted log lines (including source file and line for clickable IDE navigation in Visual Studio /
///          VS Code / Rider) to the attached debugger with zero heap allocations.
class GP_CORE_API DebugOutputSink final : public ILogSink
{
private:
    LogSinkFlags m_flags{ LogSinkFlags::IncludeTimestamp | LogSinkFlags::IncludeSourceLocation |
                          LogSinkFlags::IncludeThreadAndFiber };

public:
    /// @brief Constructs a `DebugOutputSink` with an optional minimum severity level and formatting flags.
    /// @param[in] minLevel Minimum severity level emitted to the OS debugger (`LogLevel::Trace` by default).
    /// @param[in] flags Formatting flags (`IncludeSourceLocation` enabled by default for IDE click-to-source).
    [[nodiscard]] explicit DebugOutputSink(
        const LogLevel minLevel = LogLevel::Trace,
        const LogSinkFlags flags = LogSinkFlags::IncludeTimestamp | LogSinkFlags::IncludeSourceLocation |
                                   LogSinkFlags::IncludeThreadAndFiber
    ) noexcept;

    /// @brief Virtual destructor.
    virtual ~DebugOutputSink() noexcept override = default;

public:
    /// @copydoc ILogSink::write()
    virtual void write(const LogRecordView& record) noexcept override;

    /// @copydoc ILogSink::flush()
    virtual void flush() noexcept override;
};

}   // namespace gp::log
