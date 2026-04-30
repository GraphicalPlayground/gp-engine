// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicString.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "errors/Severity.hpp"

namespace gp::error
{

/// @brief Configuration for the stacktrace capture behavior of the error system.
struct StacktraceConfig
{
    // Capture stack traces for records at or above this severity.
    // Set to Severity::Critical to capture only on process-ending events.
    Severity captureFrom{ Severity::Error };

    // Maximum number of frames to capture.  Higher = more overhead.
    gp::USize maxFrames{ 64u };

    // How many frames to skip from the bottom (hides the error-system internals).
    gp::USize skipFrames{ 3u };

    // Master switch, disable entirely for profiling/release.
    bool enabled{ true };
};

/// @brief Configuration for the error system's breakpoint behavior.
struct BreakpointConfig
{
    // Fire __debugbreak / SIGTRAP at or above this severity (Debug builds only).
    Severity breakFrom{ Severity::Error };

    // Only fire when a native debugger is attached (avoids crashing in CI).
    bool checkIsDebugged{ true };
};

/// @brief Configuration for the error system's process-abort behavior.
struct AbortConfig
{
    // Abort the process at or above this severity.
    Severity abortFrom{ Severity::Fatal };

    // When true, call std::terminate() instead of std::abort().
    bool useTerminate{ false };

    // When true, flush all sinks before aborting so no records are lost.
    bool flushBeforeAbort{ true };
};

/// @brief Configuration for the error system's filtering behavior.
struct FilterConfig
{
    // Global minimum severity, records below this are never dispatched to any sink.
    // Useful for shipping builds: set to Severity::Warning to strip all traces.
    Severity globalMinSeverity{ Severity::Trace };

    // When true, ignore duplicate records (same code + message + thread) within the dedup window below.
    bool deduplicate{ false };

    // Time window in milliseconds for deduplication.
    gp::USize dedupWindowMs{ 1000u };
};

/// @brief Configuration for the error system's thread handling.
struct ThreadConfig
{
    // Maximum thread-name length stored in ErrorRecord (truncated beyond this).
    gp::USize maxThreadNameLength{ 32u };
};

/// @brief Root configuration object for the error system, containing all sub-configurations and global settings.
struct ErrorSystemConfig
{
public:
    StacktraceConfig stacktrace;
    BreakpointConfig breakpoint;
    AbortConfig abort;
    FilterConfig filter;
    ThreadConfig thread;

    // When true, the ConsoleSink is automatically added during initialization.
    bool addDefaultConsoleSink{ true };

    // Path for the default FileSink (empty = no default file sink).
    gp::String defaultLogFilePath;   // TODO: gp::Path?

    // When true, emit a header banner to the console on initialization.
    bool printBannerOnInit{ true };

public:
    /// @brief Factory method for a preset development configuration (all features on).
    /// @return An ErrorSystemConfig instance with development settings.
    GP_NODISCARD static constexpr ErrorSystemConfig getDevelopmentConfig() noexcept
    {
        ErrorSystemConfig config;
        config.stacktrace.captureFrom = Severity::Warning;
        config.breakpoint.breakFrom = Severity::Error;
        config.abort.abortFrom = Severity::Fatal;
        config.filter.globalMinSeverity = Severity::Trace;
        config.filter.deduplicate = false;
        return config;
    }

    /// @brief Factory method for a preset staging configuration (some features on, some off).
    /// @return An ErrorSystemConfig instance with staging settings.
    GP_NODISCARD static constexpr ErrorSystemConfig getStagingConfig() noexcept
    {
        ErrorSystemConfig config;
        config.stacktrace.captureFrom = Severity::Error;
        config.breakpoint.breakFrom = Severity::Critical;   // Rarely fires in CI
        config.abort.abortFrom = Severity::Fatal;
        config.filter.globalMinSeverity = Severity::Info;
        config.filter.deduplicate = true;
        config.filter.dedupWindowMs = 2000;
        return config;
    }

    /// @brief Factory method for a preset shipping configuration (most features off for performance).
    /// @return An ErrorSystemConfig instance with shipping settings.
    GP_NODISCARD static constexpr ErrorSystemConfig getShippingConfig() noexcept
    {
        ErrorSystemConfig config;
        config.stacktrace.enabled = false;                     // No stack traces
        config.breakpoint.breakFrom = Severity::Critical;      // Never in practice
        config.abort.abortFrom = Severity::Fatal;
        config.filter.globalMinSeverity = Severity::Warning;   // Strip trace/debug/info
        config.filter.deduplicate = true;
        config.addDefaultConsoleSink = false;                  // No console in ship
        config.printBannerOnInit = false;
        return config;
    }

    /// @brief Factory method for a preset test configuration (strict settings for testing).
    /// @return An ErrorSystemConfig instance with test settings.
    GP_NODISCARD static constexpr ErrorSystemConfig getTestConfig() noexcept
    {
        ErrorSystemConfig config;
        config.stacktrace.captureFrom = Severity::Fatal;
        config.breakpoint.breakFrom = Severity::Critical;
        config.abort.abortFrom = Severity::Critical;
        config.filter.globalMinSeverity = Severity::Trace;
        config.addDefaultConsoleSink = false;
        config.printBannerOnInit = false;
        return config;
    }
};

}   // namespace gp::error
