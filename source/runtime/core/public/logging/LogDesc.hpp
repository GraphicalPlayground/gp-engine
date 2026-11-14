// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "logging/LogEnums.hpp"
#include "logging/LogTypes.hpp"   // IWYU pragma: keep

namespace gp::log
{

/// @brief Configuration descriptor for `ConsoleSink` ([ADR-0054]).
struct ConsoleSinkDesc
{
    /// @brief Minimum severity level written to the console.
    LogLevel minLevel = LogLevel::Trace;

    /// @brief Minimum severity level routed to `stderr` instead of `stdout` (defaults to `Warning`).
    LogLevel stderrThreshold = LogLevel::Warning;

    /// @brief Formatting and ANSI color flags.
    LogSinkFlags flags = LogSinkFlags::DefaultTextFormat | LogSinkFlags::EnableAnsiColors;
};

/// @brief Configuration descriptor for `FileSink` ([ADR-0054]).
struct FileSinkDesc
{
    /// @brief Explicit file path for the log file. If empty, defaults to `<tempDir>/<ExecutableName>.log`.
    gp::StringView filePath{};

    /// @brief Minimum severity level written to the log file.
    LogLevel minLevel = LogLevel::Trace;

    /// @brief Formatting flags (ANSI colors disabled by default for plain-text log files).
    LogSinkFlags flags = LogSinkFlags::DefaultTextFormat | LogSinkFlags::IncludeSourceLocation;

    /// @brief If true, truncates any existing log file on open; if false, appends to the existing file.
    bool truncateOnOpen = true;
};

/// @brief Configuration descriptor for initializing `gp::log::Logger` ([ADR-0054]).
struct LoggerDesc
{
    /// @brief Execution mode (`Synchronous` vs. lock-free `AsyncRing` background thread).
    LogExecutionMode mode = LogExecutionMode::AsyncRing;

    /// @brief Backpressure policy when the asynchronous MPMC ring queue is full.
    LogOverflowPolicy overflowPolicy = LogOverflowPolicy::FlushSynchronously;

    /// @brief Global minimum severity threshold applied across all categories.
    LogLevel globalMinLevel = LogLevel::Trace;

    /// @brief Poll/sleep interval in microseconds for the background consumer thread when the queue is idle.
    gp::UInt32 asyncFlushIntervalMicroseconds = 1000u;

    /// @brief If true, queries `gp::CommandLine` (`-log-level=...`, `-log-sync`) to override descriptor fields.
    bool applyCommandLineOverrides = true;
};

}   // namespace gp::log
