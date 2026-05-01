// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "errors/ErrorSeverity.hpp"
#include "errors/ErrorSink.hpp"

namespace gp::error
{

/// @brief A sink that triggers a debug break when an error record with severity at or above a specified threshold is
/// received. This is useful for developers who want to break into the debugger immediately when a certain level of
/// error occurs.
class BreakpointSink final : public Sink
{
private:
    Severity m_breakAt;

public:
    /// @brief Constructs a BreakpointSink that triggers a debug break at or above the specified severity level.
    /// @param[in] breakAt The severity level at which to trigger a debug break (inclusive).
    explicit BreakpointSink(Severity breakAt = Severity::Error);

public:
    /// @brief Called by the ErrorSystem for every record that passes the global filter.
    /// @param[in] record The error record to process.
    void onRecord(const ErrorRecord& record) override;
};

}   // namespace gp::error
