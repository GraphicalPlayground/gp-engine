// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreAPI.hpp"
#include "diagnostics/AssertTypes.hpp"
#include "platforms/base/Platform.hpp"

namespace gp::diagnostics
{

/// @brief Queries the operating system to check whether a native debugger is currently attached.
/// @return `true` if a debugger is attached to the current process, `false` otherwise.
[[nodiscard]] GP_CORE_API bool isDebuggerPresent() noexcept;

/// @brief Installs a custom assertion callback handler and returns the previous handler pointer.
/// @details Pass `nullptr` to restore the default engine handler, which logs to `stderr`, emits debug output on
///          Windows, breaks when a debugger is attached, and aborts on fatal checks.
/// @param[in] handler New callback function pointer, or `nullptr` for default behavior.
/// @return Previously installed handler pointer.
GP_CORE_API AssertHandlerFn setAssertHandler(AssertHandlerFn handler) noexcept;

/// @brief Returns the currently registered assertion handler pointer (`nullptr` if using the built-in default).
/// @return Active `AssertHandlerFn` pointer.
[[nodiscard]] GP_CORE_API AssertHandlerFn getAssertHandler() noexcept;

/// @brief Reports a failed fatal check without printf formatting overhead.
/// @param[in] kind Assertion category (`Check`, `CheckSlow`, `Verify`, `NoEntry`, `Unimplemented`).
/// @param[in] expression Stringified failing expression.
/// @param[in] message Literal failure description, or `nullptr`.
/// @param[in] file Source file path.
/// @param[in] line Source line number.
/// @param[in] function Enclosing function name.
GP_CORE_API void reportCheckFailure(
    AssertKind kind, const char* expression, const char* message, const char* file, Int32 line, const char* function
) noexcept;

/// @brief Reports a failed fatal check using a `printf`-style format string and arguments.
/// @param[in] kind Assertion category.
/// @param[in] expression Stringified failing expression.
/// @param[in] file Source file path.
/// @param[in] line Source line number.
/// @param[in] function Enclosing function name.
/// @param[in] format Printf-style message format string.
GP_CORE_API void reportCheckFailureFormatted(
    AssertKind kind, const char* expression, const char* file, Int32 line, const char* function, const char* format, ...
) noexcept;

/// @brief Atomically claims a once-per-site ensure flag and reports a non-fatal failure on the first hit.
/// @details Uses an atomic exchange on @p triggeredFlag so concurrent worker threads never race or spam reports for the
///          same call site.
/// @param[in,out] triggeredFlag Caller-static byte initialized to `0`. Set atomically to `1` on first failure.
/// @param[in] expression Stringified failing expression.
/// @param[in] message Optional literal message, or `nullptr`.
/// @param[in] file Source file path.
/// @param[in] line Source line number.
/// @param[in] function Enclosing function name.
GP_CORE_API void reportEnsureFailure(
    UInt8& triggeredFlag,
    const char* expression,
    const char* message,
    const char* file,
    Int32 line,
    const char* function
) noexcept;

/// @brief Atomically claims a once-per-site ensure flag and formats a `printf`-style message on the first hit.
/// @param[in,out] triggeredFlag Caller-static byte initialized to `0`. Set atomically to `1` on first failure.
/// @param[in] expression Stringified failing expression.
/// @param[in] file Source file path.
/// @param[in] line Source line number.
/// @param[in] function Enclosing function name.
/// @param[in] format Printf-style message format string.
GP_CORE_API void reportEnsureFailureFormatted(
    UInt8& triggeredFlag,
    const char* expression,
    const char* file,
    Int32 line,
    const char* function,
    const char* format,
    ...
) noexcept;

namespace detail
{

/// @brief Overload forwarding a single literal message cleanly without treating `%` as a format specifier.
/// @param[in] kind Assertion category.
/// @param[in] expression Stringified failing expression.
/// @param[in] file Source file path.
/// @param[in] line Source line number.
/// @param[in] function Enclosing function name.
/// @param[in] message Literal failure description.
inline void dispatchAssertFailure(
    const AssertKind kind,
    const char* const expression,
    const char* const file,
    const Int32 line,
    const char* const function,
    const char* const message = nullptr
) noexcept
{
    reportCheckFailure(kind, expression, message, file, line, function);
}

/// @brief Variadic overload forwarding a format string and one or more format arguments to
///        `reportCheckFailureFormatted`.
/// @tparam FirstArg First format argument type.
/// @tparam RestArgs Remaining format argument types.
/// @param[in] kind Assertion category.
/// @param[in] expression Stringified failing expression.
/// @param[in] file Source file path.
/// @param[in] line Source line number.
/// @param[in] function Enclosing function name.
/// @param[in] format Printf-style message format string.
/// @param[in] firstArg First argument matched against @p format.
/// @param[in] restArgs Remaining format arguments.
template <typename FirstArg, typename... RestArgs>
inline void dispatchAssertFailure(
    const AssertKind kind,
    const char* const expression,
    const char* const file,
    const Int32 line,
    const char* const function,
    const char* const format,
    FirstArg&& firstArg,
    RestArgs&&... restArgs
) noexcept
{
    reportCheckFailureFormatted(
        kind,
        expression,
        file,
        line,
        function,
        format,
        static_cast<FirstArg&&>(firstArg),
        static_cast<RestArgs&&>(restArgs)...
    );
}

}   // namespace detail

}   // namespace gp::diagnostics
