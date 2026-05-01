// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "errors/ErrorSeverity.hpp"
#include "errors/ErrorSink.hpp"

namespace gp::error
{

/// @brief A sink that terminates the process when an error record with severity at or above a specified threshold is
/// received.
class AbortSink final : public Sink
{
public:
    enum class Mode
    {
        Abort,
        Terminate
    };

private:
    Severity m_abortAt;
    Mode m_mode;

public:
    /// @brief Constructs an AbortSink that terminates the process when a record with severity at or above @p abortAt is
    /// received.
    /// @param[in] abortAt The severity threshold at which to trigger process termination (inclusive).
    /// @param[in] mode The method of termination: std::abort() or std::terminate().
    explicit AbortSink(Severity abortAt = Severity::Fatal, Mode mode = Mode::Abort);

public:
    /// @brief Called by the ErrorSystem for every record that passes the global filter.
    /// @param[in] record The error record to process.
    void onRecord(const ErrorRecord& record) override;
};

}   // namespace gp::error
