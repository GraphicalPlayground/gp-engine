// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/FixedString.hpp"
#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "logging/LogEnums.hpp"
#include "logging/LogForward.hpp"
#include "platforms/base/system/Time.hpp"   // IWYU pragma: keep
#include "threading/fibers/FiberTypes.hpp"
#include "time/TimeTypes.hpp"
#include <source_location>
#include <type_traits>

namespace gp::log
{

/// @brief Maximum UTF-8 character capacity (excluding null terminator) of a single formatted log message ([ADR-0054]).
inline constexpr gp::USize kMaxLogMessageLength = 439u;

/// @brief Maximum number of concurrent `ILogSink` instances registered with `Logger` ([ADR-0054]).
inline constexpr gp::UInt32 kMaxLogSinks = 8u;

/// @brief Default power-of-two slot capacity for the lock-free asynchronous `MPMCRingQueue<LogEntry>` ([ADR-0054]).
inline constexpr gp::USize kDefaultAsyncLogQueueCapacity = 256u;

/// @brief Fixed-capacity stack string type used during zero-allocation log message formatting.
using LogMessageBuffer = gp::FixedString<kMaxLogMessageLength>;

/// @brief Compile-time source code coordinates attached to a log record ([ADR-0054]).
struct LogSourceLocation
{
    /// @brief Null-terminated UTF-8 source file path (`__FILE__`).
    const char* file = "";

    /// @brief Null-terminated UTF-8 function signature or name (`__func__`).
    const char* function = "";

    /// @brief 1-based source line number (`__LINE__`).
    gp::UInt32 line = 0u;

    /// @brief Default-constructs an empty source location.
    [[nodiscard]] constexpr LogSourceLocation() noexcept = default;

    /// @brief Constructs a `LogSourceLocation` from explicit file, function, and line values.
    /// @param[in] inFile Null-terminated file path string.
    /// @param[in] inFunction Null-terminated function name string.
    /// @param[in] inLine 1-based source line number.
    [[nodiscard]] constexpr LogSourceLocation(
        const char* const inFile, const char* const inFunction, const gp::UInt32 inLine
    ) noexcept
        : file((inFile != nullptr) ? inFile : "")
        , function((inFunction != nullptr) ? inFunction : "")
        , line(inLine)
    {}

    /// @brief Captures the caller's source location via C++20 `std::source_location`.
    /// @param[in] loc Standard source location token (defaults to `std::source_location::current()`).
    /// @return Populated `LogSourceLocation`.
    [[nodiscard]] static constexpr LogSourceLocation
        current(const std::source_location loc = std::source_location::current()) noexcept
    {
        return LogSourceLocation(loc.file_name(), loc.function_name(), static_cast<gp::UInt32>(loc.line()));
    }
};

/// @brief Non-owning read-only view of a formatted log record passed to `ILogSink::write()` ([ADR-0054]).
struct LogRecordView
{
    /// @brief Monotonic engine timestamp when the log record was emitted.
    gp::time::Timestamp timestamp{};

    /// @brief Wall-clock calendar date and time when the log record was emitted.
    gp::platform::DateTime dateTime{};

    /// @brief Interned category name (`gp::Name`) identifying the originating subsystem.
    gp::Name category{};

    /// @brief Non-owning UTF-8 view of the category name.
    gp::StringView categoryName{};

    /// @brief Non-owning UTF-8 view of the formatted log message body.
    gp::StringView message{};

    /// @brief Call-site source file, function, and line number.
    LogSourceLocation location{};

    /// @brief Calling OS thread identifier (`gp::platform::Thread::currentThreadId()`).
    gp::UInt64 threadId = 0ULL;

    /// @brief Calling cooperative fiber identifier (`gp::thread::Fiber::getCurrentFiberId()`).
    gp::thread::FiberId fiberId = gp::thread::kInvalidFiberId;

    /// @brief Severity level of the log record.
    LogLevel level = LogLevel::Info;
};

/// @brief Trivially copyable, 64-byte cache-line-aligned log record stored in the lock-free `MPMCRingQueue`
///        ([ADR-0054]).
/// @details Total size is `512` bytes (`8` hardware cache lines): `72` bytes of header metadata plus `440` bytes of
///          inline UTF-8 message storage (`439` characters + null terminator), with zero heap pointers.
struct alignas(64) LogEntry
{
    /// @brief Monotonic timestamp in nanoseconds (`8` bytes).
    gp::UInt64 timestampNs = 0ULL;

    /// @brief Calling OS thread identifier (`8` bytes).
    gp::UInt64 threadId = 0ULL;

    /// @brief Interned category `gp::Name` (`8` bytes).
    gp::Name category{};

    /// @brief Static category name literal pointer (`8` bytes).
    const char* categoryLiteral = "";

    /// @brief Static source file literal pointer (`8` bytes).
    const char* file = "";

    /// @brief Static function name literal pointer (`8` bytes).
    const char* function = "";

    /// @brief 1-based source line number (`4` bytes).
    gp::UInt32 line = 0u;

    /// @brief Cooperative fiber identifier (`4` bytes).
    gp::thread::FiberId fiberId = gp::thread::kInvalidFiberId;

    /// @brief Wall-clock date/time snapshot (`12` bytes).
    gp::platform::DateTime dateTime{};

    /// @brief Byte length of the formatted UTF-8 string stored in `message` (`2` bytes).
    gp::UInt16 messageLength = 0u;

    /// @brief Byte length of `categoryLiteral` (`1` byte).
    gp::UInt8 categoryLength = 0u;

    /// @brief Severity level (`1` byte).
    LogLevel level = LogLevel::Info;

    /// @brief Null-terminated UTF-8 formatted message payload (`440` bytes).
    char message[kMaxLogMessageLength + 1u]{};

    /// @brief Produces a non-owning `LogRecordView` referencing this entry's inline storage.
    /// @return `LogRecordView` spanning this entry.
    [[nodiscard]] LogRecordView toView() const noexcept
    {
        LogRecordView view{};
        view.timestamp = gp::time::Timestamp(timestampNs);
        view.dateTime = dateTime;
        view.category = category;
        if (categoryLiteral != nullptr && categoryLength > 0u)
        {
            view.categoryName = gp::StringView(categoryLiteral, static_cast<gp::USize>(categoryLength));
        }
        else if (!category.isNone())
        {
            view.categoryName = category.baseView();
        }
        view.message = gp::StringView(message, static_cast<gp::USize>(messageLength));
        view.location = LogSourceLocation(file, function, line);
        view.threadId = threadId;
        view.fiberId = fiberId;
        view.level = level;
        return view;
    }
};

static_assert(sizeof(LogEntry) == 512u, "LogEntry must be 512 bytes (8 cache lines).");
static_assert(alignof(LogEntry) == 64u, "LogEntry must be 64-byte cache-line aligned.");
static_assert(std::is_trivially_copyable_v<LogEntry>, "LogEntry must be trivially copyable for MPMCRingQueue.");

namespace detail
{

/// @brief Compile-time helper invoked when a `LogFormatString` fails syntax validation.
/// @note Non-constexpr so calling it inside a `consteval` constructor produces a compile-time diagnostic.
void onInvalidLogFormatString(const char* reason) noexcept;

/// @brief Validates a format specifier substring inside `{:...}` at compile time.
/// @param[in] spec Inner format specifier (after optional leading `':'`).
/// @return True if valid; calls `onInvalidLogFormatString` on unrecognized specifier characters.
consteval bool validateFormatSpecifier(const gp::StringView spec)
{
    for (gp::USize i = 0u; i < spec.size(); ++i)
    {
        const char ch = spec[i];
        const bool isDigit = (ch >= '0' && ch <= '9');
        const bool isAllowedSymbol =
            (ch == ':' || ch == '#' || ch == '.' || ch == 'x' || ch == 'X' || ch == 'b' || ch == 'B' || ch == 'd' ||
             ch == 'i' || ch == 'u' || ch == 'f' || ch == 'F' || ch == 'e' || ch == 'E' || ch == 's' || ch == 'p');
        if (!isDigit && !isAllowedSymbol)
        {
            onInvalidLogFormatString("Unsupported character inside log format specifier.");
            return false;
        }
    }
    return true;
}

/// @brief Validates a format string against `expectedArgs` placeholders at compile time.
/// @param[in] fmt Format string view to validate.
/// @param[in] expectedArgs Expected number of replacement arguments (`sizeof...(InArgs)`).
/// @return True if valid; calls `onInvalidLogFormatString` on failure.
consteval bool validateFormatString(const gp::StringView fmt, const gp::USize expectedArgs)
{
    gp::USize placeholderCount = 0u;
    const gp::USize len = fmt.size();
    gp::USize i = 0u;

    while (i < len)
    {
        if (fmt[i] == '{')
        {
            if (i + 1u < len && fmt[i + 1u] == '{')
            {
                i += 2u;
                continue;
            }

            ++i;
            const gp::USize specStart = i;
            while (i < len && fmt[i] != '}')
            {
                if (fmt[i] == '{')
                {
                    onInvalidLogFormatString("Nested '{' inside format specifier is not allowed.");
                    return false;
                }
                ++i;
            }

            if (i >= len || fmt[i] != '}')
            {
                onInvalidLogFormatString("Unclosed '{' in log format string.");
                return false;
            }

            if (!validateFormatSpecifier(fmt.substr(specStart, i - specStart)))
            {
                return false;
            }

            ++placeholderCount;
            ++i;
        }
        else if (fmt[i] == '}')
        {
            if (i + 1u < len && fmt[i + 1u] == '}')
            {
                i += 2u;
                continue;
            }

            onInvalidLogFormatString("Unmatched '}' in log format string.");
            return false;
        }
        else
        {
            ++i;
        }
    }

    if (placeholderCount != expectedArgs)
    {
        onInvalidLogFormatString("Log format string placeholder count '{}' does not match argument count.");
        return false;
    }

    return true;
}

}   // namespace detail

/// @brief Compile-time validated format string wrapper that captures caller `LogSourceLocation` ([ADR-0054]).
/// @tparam InArgs Variadic argument types passed to the logging call.
template <typename... InArgs>
struct LogFormatString
{
    /// @brief Validated format string view.
    gp::StringView format{};

    /// @brief Caller source code location captured at the call site.
    LogSourceLocation location{};

    /// @brief `consteval` constructor validating placeholder count and syntax at compile time.
    /// @tparam InStringType Convertible string literal or `StringView` type.
    /// @param[in] inStr Format string literal.
    /// @param[in] inLoc Call-site source location (captured automatically via default parameter).
    template <typename InStringType>
    requires std::is_convertible_v<const InStringType&, gp::StringView> consteval LogFormatString(
        const InStringType& inStr, const std::source_location inLoc = std::source_location::current()
    )
        : format(inStr)
        , location(LogSourceLocation::current(inLoc))
    {
        (void)detail::validateFormatString(format, sizeof...(InArgs));
    }
};

/// @brief Type alias preventing template argument deduction on `LogFormatString` parameters.
template <typename... InArgs>
using CheckedLogFormatString = LogFormatString<std::type_identity_t<InArgs>...>;

}   // namespace gp::log
