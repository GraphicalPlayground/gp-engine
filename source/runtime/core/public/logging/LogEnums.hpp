// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::log
{

/// @brief Severity levels for engine diagnostic log records, ordered from most verbose (`Trace`) to disabled (`Off`)
///        ([ADR-0054]).
enum class LogLevel : gp::UInt8
{
    /// @brief Fine-grained per-frame or per-packet diagnostic trace messages.
    Trace = 0,

    /// @brief Developer debugging telemetry excluded from shipping builds by default.
    Debug = 1,

    /// @brief Informational lifecycle milestones and normal subsystem status messages.
    Info = 2,

    /// @brief Recoverable anomalies, fallback paths, or performance budget warnings.
    Warning = 3,

    /// @brief Non-fatal subsystem or asset errors requiring engineering attention.
    Error = 4,

    /// @brief Unrecoverable engine failures that force an immediate synchronous flush across all sinks.
    Fatal = 5,

    /// @brief Sentinel level that suppresses all log output for a category or sink.
    Off = 6
};

/// @brief Dispatch execution mode for `gp::log::Logger` ([ADR-0054]).
enum class LogExecutionMode : gp::UInt8
{
    /// @brief Formats and dispatches log records directly on the calling thread under a spinlock.
    /// @note Used automatically prior to `Logger::initialize()`, during `LogLevel::Fatal`, and in unit tests.
    Synchronous = 0,

    /// @brief Enqueues formatted `LogEntry` records into a lock-free `MPMCRingQueue` drained by a dedicated background
    ///        flush thread.
    AsyncRing = 1
};

/// @brief Backpressure behavior when the asynchronous `MPMCRingQueue` is temporarily full ([ADR-0054]).
enum class LogOverflowPolicy : gp::UInt8
{
    /// @brief Drains queued entries and dispatches the incoming record synchronously so zero records are dropped.
    FlushSynchronously = 0,

    /// @brief Drops non-fatal records and increments `Logger::getDroppedRecordCount()` without stalling the caller.
    /// @note `LogLevel::Error` and `LogLevel::Fatal` records still flush synchronously to preserve error fidelity.
    DropOldestNonCritical = 1
};

/// @brief Bitmask flags controlling sink formatting and output behavior ([ADR-0054]).
enum class LogSinkFlags : gp::UInt8
{
    /// @brief No special flags enabled.
    None = 0x00,

    /// @brief Emits ANSI 16/24-bit color escape sequences around severity badges (`ConsoleSink`).
    EnableAnsiColors = 0x01,

    /// @brief Includes the source file basename and line number (`[File.cpp:123]`) in formatted sink output.
    IncludeSourceLocation = 0x02,

    /// @brief Includes the calling OS thread ID and cooperative fiber ID (`[T:1024 F:3]`) in formatted sink output.
    IncludeThreadAndFiber = 0x04,

    /// @brief Includes the UTC/local timestamp prefix (`[HH:MM:SS.mmm]`) in formatted sink output.
    IncludeTimestamp = 0x08,

    /// @brief Flushes the underlying OS stream after every record rather than only on `Warning`/`Error`/`Fatal`.
    FlushEveryRecord = 0x10,

    /// @brief Default standard formatting flags for human-readable text sinks.
    DefaultTextFormat = IncludeTimestamp | IncludeThreadAndFiber
};

/// @brief Explicit status codes returned by `Logger` and `ILogSink` lifecycle operations ([ADR-0054]).
enum class [[nodiscard]] LogResult : gp::UInt8
{
    /// @brief The operation completed successfully.
    Success = 0,

    /// @brief One or more configuration parameters or pointers were invalid.
    InvalidArguments = 1,

    /// @brief The logger or sink is already initialized.
    AlreadyInitialized = 2,

    /// @brief The logger is not initialized.
    NotInitialized = 3,

    /// @brief The maximum number of registered sinks (`kMaxLogSinks`) has been reached.
    SinkCapacityExceeded = 4,

    /// @brief The specified sink was not found in the active registry.
    SinkNotFound = 5,

    /// @brief Opening or writing to the underlying OS file or stream failed.
    IoError = 6,

    /// @brief Starting the background asynchronous consumer thread failed.
    ThreadCreationFailed = 7
};

/// @brief Converts a `LogLevel` enumerator into its canonical uppercase string view (`"TRACE"`, `"INFO"`, etc.).
/// @param[in] level Severity level to convert.
/// @return Non-owning `StringView` of the canonical uppercase label.
[[nodiscard]] constexpr gp::StringView logLevelToString(const LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    case LogLevel::Off:
        // falls through
    default:
        return "OFF";
    }
}

/// @brief Parses a case-insensitive string view (e.g., from `-log-level=debug`) into a `LogLevel`.
/// @param[in] text Input string view to parse.
/// @param[out] outLevel Receives the parsed `LogLevel` on success.
/// @return True if `text` matched a valid `LogLevel` name, false otherwise.
[[nodiscard]] constexpr bool tryParseLogLevel(const gp::StringView text, LogLevel& outLevel) noexcept
{
    if (text.equalsIgnoreCase("trace") || text.equalsIgnoreCase("verbose"))
    {
        outLevel = LogLevel::Trace;
        return true;
    }
    if (text.equalsIgnoreCase("debug"))
    {
        outLevel = LogLevel::Debug;
        return true;
    }
    if (text.equalsIgnoreCase("info") || text.equalsIgnoreCase("log"))
    {
        outLevel = LogLevel::Info;
        return true;
    }
    if (text.equalsIgnoreCase("warn") || text.equalsIgnoreCase("warning"))
    {
        outLevel = LogLevel::Warning;
        return true;
    }
    if (text.equalsIgnoreCase("error") || text.equalsIgnoreCase("err"))
    {
        outLevel = LogLevel::Error;
        return true;
    }
    if (text.equalsIgnoreCase("fatal") || text.equalsIgnoreCase("critical"))
    {
        outLevel = LogLevel::Fatal;
        return true;
    }
    if (text.equalsIgnoreCase("off") || text.equalsIgnoreCase("none") || text.equalsIgnoreCase("silent"))
    {
        outLevel = LogLevel::Off;
        return true;
    }
    return false;
}

}   // namespace gp::log

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::log::LogSinkFlags);
