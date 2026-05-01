// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "errors/ErrorSink.hpp"
#include <cstdio>   // TODO: gp::IO?
#include <mutex>    // TODO: gp::Mutex?

namespace gp::error
{

/// @brief ConsoleSink, ANSI-coloured stderr (or stdout) output.
class ConsoleSink final : public Sink
{
private:
    std::mutex m_mutex;
    bool m_useColor{ true };
    FILE* m_stream{ nullptr };
    bool m_printStacktrace{ true };
    bool m_printCause{ true };

public:
    /// @brief Constructs a ConsoleSink.
    /// @param[in] useAnsiColor If true, ANSI color codes will be used to colorize the output based on severity. If
    /// false, no color codes will be used.
    /// @param[in] toStdout If true, output will be sent to stdout; otherwise, it will be sent to stderr.
    explicit ConsoleSink(bool useAnsiColor = true, bool toStdout = false);

public:
    /// @brief Called by the ErrorSystem for every record that passes the global filter.
    /// @param[in] record The error record to process.
    void onRecord(const ErrorRecord& record) override;
    /// @brief Flush any buffered output (file handles, network sockets, etc.).
    void flush() override;

    /// @brief Enable or disable printing of stack traces.
    /// @param[in] enabled If true, stack traces will be printed; otherwise, they will be omitted.
    void setPrintStacktrace(bool enabled) noexcept;

    /// @brief Enable or disable printing of error causes.
    /// @param[in] enabled If true, error causes will be printed; otherwise, they will be omitted.
    void setPrintCause(bool enabled) noexcept;
};

}   // namespace gp::error
