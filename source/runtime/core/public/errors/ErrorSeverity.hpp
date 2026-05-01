// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/Array.hpp"
#include "container/BasicStringView.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "macros/MacroUtilities.hpp"
#include "utils/Enums.hpp"

namespace gp::error
{

/// @brief Represents the severity level of an error or warning.
enum class Severity : gp::UInt8
{
    Trace = 0,      //<! Extremely verbose internal tracing (disabled in all non-Debug builds).
    Debug = 1,      //<! General developer diagnostics.
    Info = 2,       //<! High-level lifecycle / milestone messages.
    Warning = 3,    //<! Recoverable anomalies; execution continues normally.
    Error = 4,      //<! Non-fatal failure; subsystem degraded but engine lives.
    Fatal = 5,      //<! Unrecoverable subsystem failure; triggers controlled shutdown.
    Critical = 6,   //<! Immediate abort (hardware fault, memory corruption, etc.).

    COUNT = 7       //<! Sentinel, do NOT use as a real severity.
};

namespace detail
{

/// @brief Metadata associated with each severity level, used for formatting and behavior control.
struct SeverityMeta
{
    gp::StringView name;        //<! Short identifier used in log output.
    gp::StringView display;     //<! Human-readable label.
    gp::StringView ansiColor;   //<! ANSI escape sequence for terminal output.
    bool breaksDebugger;        //<! Should this fire __debugbreak / raise(SIGTRAP)?
    bool abortsProcess;         //<! Should this call std::abort() after handlers?
};

// clang-format off
inline constexpr gp::Array<SeverityMeta, static_cast<gp::USize>(Severity::COUNT)> kSeverityMeta
{{
    /* Trace    */ { "TRC", "TRACE",    "\033[90m",  false, false },
    /* Debug    */ { "DBG", "DEBUG",    "\033[36m",  false, false },
    /* Info     */ { "INF", "INFO",     "\033[32m",  false, false },
    /* Warning  */ { "WRN", "WARNING",  "\033[33m",  false, false },
    /* Error    */ { "ERR", "ERROR",    "\033[31m",  true,  false },
    /* Fatal    */ { "FTL", "FATAL",    "\033[35m",  true,  true  },
    /* Critical */ { "CRT", "CRITICAL", "\033[1;31m",true,  true  },
}};
// clang-format on

}   // namespace detail

/// @brief Gets the short identifier for a given severity level, used in log output and diagnostics.
/// @param[in] severity The severity level to check.
/// @return The short identifier for the specified severity level.
GP_NODISCARD constexpr gp::StringView getSeverityName(Severity severity) noexcept
{
    return detail::kSeverityMeta[static_cast<gp::USize>(severity)].name;
}

/// @brief Gets the human-readable label for a given severity level, used in log output and diagnostics.
/// @param[in] severity The severity level to check.
/// @return The human-readable label for the specified severity level.
GP_NODISCARD constexpr gp::StringView getSeverityDisplay(Severity severity) noexcept
{
    return detail::kSeverityMeta[static_cast<gp::USize>(severity)].display;
}

/// @brief Gets the ANSI escape sequence for a given severity level, used for colored terminal output.
/// @param[in] severity The severity level to check.
/// @return The ANSI escape sequence for the specified severity level.
GP_NODISCARD constexpr gp::StringView getSeverityAnsiColor(Severity severity) noexcept
{
    return detail::kSeverityMeta[static_cast<gp::USize>(severity)].ansiColor;
}

/// @brief Determines if the given severity level should trigger a breakpoint in the debugger.
/// @param[in] severity The severity level to check.
/// @return True if the severity level should trigger a breakpoint, false otherwise.
GP_NODISCARD constexpr bool doesSeverityBreakDebugger(Severity severity) noexcept
{
    return detail::kSeverityMeta[static_cast<gp::USize>(severity)].breaksDebugger;
}

/// @brief Determines if the given severity level should trigger a process abort after handlers are executed.
/// @param[in] severity The severity level to check.
/// @return True if the severity level should trigger a process abort, false otherwise.
GP_NODISCARD constexpr bool doesSeverityAbortProcess(Severity severity) noexcept
{
    return detail::kSeverityMeta[static_cast<gp::USize>(severity)].abortsProcess;
}

}   // namespace gp::error

// Enable comparison operators for Severity enum (e.g., severity >= Severity::Warning).
GP_ENABLE_ENUM_COMPARISON_OPERATIONS(gp::error::Severity);
