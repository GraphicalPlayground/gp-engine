// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogEnums.hpp"

namespace gp::log
{

/// @brief Profiler timeline log sink that forwards engine log records to Tracy (`GP_MESSAGE_C`) ([ADR-0054]).
/// @details Maps `LogLevel` severities to distinct RGB colors (`GP_U32_COLOR_*`) so warnings and errors stand out
///          visually in the Tracy frame timeline. Compiles to a lightweight no-op when `GP_USE_TRACY_PROFILER` is
///          disabled.
class GP_CORE_API TracySink final : public ILogSink
{
public:
    /// @brief Constructs a `TracySink` with an initial minimum severity threshold.
    /// @param[in] minLevel Minimum severity forwarded to Tracy (`LogLevel::Info` by default to avoid trace spam).
    [[nodiscard]] explicit TracySink(const LogLevel minLevel = LogLevel::Info) noexcept;

    /// @brief Virtual destructor.
    virtual ~TracySink() noexcept override = default;

public:
    /// @copydoc ILogSink::write()
    virtual void write(const LogRecordView& record) noexcept override;

    /// @copydoc ILogSink::flush()
    virtual void flush() noexcept override;
};

}   // namespace gp::log
