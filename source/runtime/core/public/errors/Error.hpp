// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicString.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "errors/ErrorCode.hpp"
#include "errors/ErrorRecord.hpp"
#include "errors/ErrorSeverity.hpp"
#include "errors/ErrorSystem.hpp"
#include <expected>
#include <source_location>

namespace gp::error
{

/// @brief Raises an error with the specified severity, code, message, cause, and source location.
/// @param[in] severity The severity level of the error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] code The specific error code representing the type of error.
/// @param[in] message A descriptive message providing details about the error.
/// @param[in] cause An optional shared pointer to an ErrorRecord representing the cause of this error (can be nullptr).
/// @param[in] location The source location where the error was raised (defaults to the current location if not
/// provided).
inline void raise(
    Severity severity,
    ErrorCode code,
    gp::String message,
    std::shared_ptr<const ErrorRecord> cause = nullptr,
    std::source_location location = std::source_location::current()
)
{
    ErrorSystem::dispatch(severity, code, std::move(message), location, std::move(cause));
}

/// @brief Raises an error with the specified severity and message.
/// @param[in] severity The severity level of the error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] message A descriptive message providing details about the error.
/// @param[in] location The source location where the error was raised (defaults to the current location if not
/// provided).
inline void
    raise(Severity severity, gp::String message, std::source_location location = std::source_location::current())
{
    ErrorSystem::dispatch(severity, codes::kUnknown, std::move(message), location, nullptr);
}

/// @brief Wraps an existing error record with additional context and raises a new error.
/// @param[in] inner The existing ErrorRecord to be wrapped as the cause of the new error.
/// @param[in] severity The severity level of the new error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] code The specific error code representing the type of the new error.
/// @param[in] message A descriptive message providing details about the new error.
/// @param[in] location The source location where the new error was raised (defaults to the current location if not
/// provided).
inline void wrap(
    const ErrorRecord& inner,
    Severity severity,
    ErrorCode code,
    gp::String message,
    std::source_location location = std::source_location::current()
)
{
    ErrorSystem::dispatch(severity, code, std::move(message), location, std::make_shared<ErrorRecord>(inner));
}

}   // namespace gp::error

namespace gp
{

/// @brief A template alias for a type that represents either a successful value of type T or an error record.
/// @tparam T The type of the successful value.
template <typename T>
using Expected = std::expected<T, error::ErrorRecord>;

/// @brief Creates an ErrorRecord with the specified severity, code, message, and source location.
/// @param[in] severity The severity level of the error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] code The specific error code representing the type of error.
/// @param[in] message A descriptive message providing details about the error.
/// @param[in] location The source location where the error was created (defaults to the current location if not
/// provided).
/// @return An ErrorRecord containing the provided information and additional metadata such as timestamps and thread
/// info.
GP_NODISCARD inline error::ErrorRecord makeError(
    error::Severity severity,
    error::ErrorCode code,
    gp::String message,
    std::source_location location = std::source_location::current()
)
{
    error::ErrorRecord r;
    r.severity = severity;
    r.code = code;
    r.message = std::move(message);
    r.location = location;
    r.wallTime = std::chrono::system_clock::now();
    r.engineTime = std::chrono::steady_clock::now();
    r.threadId = std::this_thread::get_id();
    r.threadName = error::ErrorContext::current().threadName();
    return r;
}

/// @brief Creates an Expected<T> representing a successful value.
/// @param[in] value The value to be wrapped in the Expected<T>.
/// @return An Expected<T> containing the provided value, indicating success.
template <typename T>
GP_NODISCARD constexpr Expected<T> makeOk(T&& value)
{
    return Expected<T>{ std::forward<T>(value) };
}
}   // namespace gp

/// @brief Macro to raise an error with the specified severity, code, and message, automatically capturing the source
/// location.
/// @param[in] sev_ The severity level of the error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] code_ The specific error code representing the type of error.
/// @param[in] msg_ A descriptive message providing details about the error.
#define GP_DETAIL_RAISE(sev_, code_, msg_) \
    ::gp::error::ErrorSystem::dispatch((sev_),(code_),(msg_),std::source_location::current())

/// @brief Macro to raise an error with a specific severity and code, using the GP_DETAIL_RAISE macro.
/// @param[in] sev_ The severity level of the error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] code_ The specific error code representing the type of error.
/// @param[in] msg_ A descriptive message providing details about the error.
#define GP_RAISE_CODE(sev_, code_, msg_) GP_DETAIL_RAISE((sev_),(code_),(msg_))

/// @brief Macro to raise an error with a specific severity and message, using the GP_DETAIL_RAISE macro with an unknown
/// error code.
/// @param[in] sev_ The severity level of the error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] msg_ A descriptive message providing details about the error.
#define GP_RAISE(sev_, msg_) GP_DETAIL_RAISE((sev_),::gp::error::codes::kUnknown,(msg_))

/// @brief Macro to raise a formatted error message with a specific severity and code, using the GP_DETAIL_RAISE macro.
/// @param[in] sev_ The severity level of the error (e.g., trace, debug, info, warning, error, fatal).
/// @param[in] code_ The specific error code representing the type of error.
/// @param[in] fmt_ A format string for the error message.
/// @param[in] ... Additional arguments to be formatted into the error message.
#define GP_RAISE_FMT(sev_, code_, fmt_, ...) GP_DETAIL_RAISE((sev_),(code_),gp::String::format((fmt_),__VA_ARGS__))

/// @brief Macro to raise a trace-level error with a specific message.
/// @param[in] m_ A descriptive message providing details about the trace event.
#define GP_TRACE(m_) GP_RAISE(::gp::error::trace, (m_))

/// @brief Macro to raise a debug-level error with a specific message.
/// @param[in] m_ A descriptive message providing details about the debug event.
#define GP_DEBUG(m_) GP_RAISE(::gp::error::debug,(m_))

/// @brief Macro to raise an info-level error with a specific message.
/// @param[in] m_ A descriptive message providing details about the info event.
#define GP_INFO(m_)  GP_RAISE(::gp::error::info,(m_))

/// @brief Macro to raise a warning-level error with a specific message.
/// @param[in] m_ A descriptive message providing details about the warning event.
#define GP_WARN(m_)  GP_RAISE(::gp::error::warning,(m_))

/// @brief Macro to raise an error-level error with a specific message.
/// @param[in] m_ A descriptive message providing details about the error event.
#define GP_ERROR(m_) GP_RAISE(::gp::error::error,(m_))

/// @brief Macro to raise a fatal-level error with a specific message.
/// @param[in] m_ A descriptive message providing details about the fatal event.
#define GP_FATAL(m_) GP_RAISE(::gp::error::fatal,(m_))

/// @brief Macro to raise a critical-level error (panic) with a specific message.
/// @param[in] m_ A descriptive message providing details about the critical event.
#define GP_PANIC(m_) GP_RAISE(::gp::error::critical,(m_))
