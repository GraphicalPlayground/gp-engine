// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/CharTraits.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "logging/LogEnums.hpp"
#include "logging/LogTypes.hpp"
#include <charconv>
#include <type_traits>

namespace gp::log
{

namespace detail
{

/// @brief Parsed integer/float formatting options extracted from `{:...}`.
struct ParsedFormatSpec
{
    /// @brief Minimum field width (`0` if unspecified).
    gp::UInt32 width = 0u;

    /// @brief Floating-point fractional digit precision (`-1` if unspecified).
    gp::Int32 precision = -1;

    /// @brief Base/presentation character (`'d'`, `'x'`, `'X'`, `'b'`, `'B'`, `'f'`, `'F'`, `'e'`, `'E'`, or `'\0'`).
    char typeChar = '\0';

    /// @brief True if `'#'` alternate prefix (`0x`, `0X`, `0b`, `0B`) was requested.
    bool alternateForm = false;

    /// @brief True if leading `'0'` padding was requested.
    bool zeroPad = false;
};

/// @brief Parses a format specifier slice (after stripping any leading `':'`) into `ParsedFormatSpec`.
/// @param[in] spec Inner format specifier view (e.g., `"#08x"` or `".3f"`).
/// @return Populated `ParsedFormatSpec`.
[[nodiscard]] constexpr ParsedFormatSpec parseFormatSpec(const gp::StringView spec) noexcept
{
    ParsedFormatSpec out{};
    gp::USize i = 0u;
    const gp::USize len = spec.size();

    if (i < len && spec[i] == '#')
    {
        out.alternateForm = true;
        ++i;
    }

    if (i < len && spec[i] == '0')
    {
        out.zeroPad = true;
        ++i;
    }

    while (i < len && gp::container::CharTraits<char>::isDigit(spec[i]))
    {
        out.width = (out.width * 10u) + static_cast<gp::UInt32>(spec[i] - '0');
        ++i;
    }

    if (i < len && spec[i] == '.')
    {
        ++i;
        out.precision = 0;
        while (i < len && gp::container::CharTraits<char>::isDigit(spec[i]))
        {
            out.precision = (out.precision * 10) + static_cast<gp::Int32>(spec[i] - '0');
            ++i;
        }
    }

    if (i < len)
    {
        out.typeChar = spec[i];
    }

    return out;
}

/// @brief Appends a single character to a fixed-capacity buffer, truncating safely if full.
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @param[in,out] outBuffer Target fixed string buffer.
/// @param[in] ch Character to append.
template <gp::USize N>
constexpr void appendCharSafe(gp::container::BasicFixedString<char, N>& outBuffer, const char ch) noexcept
{
    if (outBuffer.size() < N)
    {
        outBuffer.pushBack(ch);
    }
}

/// @brief Appends a `StringView` slice to a fixed-capacity buffer, truncating safely if full.
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @param[in,out] outBuffer Target fixed string buffer.
/// @param[in] view Characters to append.
template <gp::USize N>
constexpr void appendViewSafe(gp::container::BasicFixedString<char, N>& outBuffer, const gp::StringView view) noexcept
{
    const gp::USize currentSize = outBuffer.size();
    if (currentSize >= N || view.isEmpty())
    {
        return;
    }

    const gp::USize remaining = N - currentSize;
    const gp::USize toCopy = (view.size() <= remaining) ? view.size() : remaining;
    outBuffer.append(view.substr(0u, toCopy));
}

/// @brief Formats an unsigned integer in base 16 or base 2 with optional prefix and width padding.
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @tparam InUIntType Unsigned integral type.
/// @param[in,out] outBuffer Target fixed string buffer.
/// @param[in] value Unsigned integer value to format.
/// @param[in] base Radix (`16` for hex, `2` for binary).
/// @param[in] includePrefix If true, prepends `"0x"`/`"0X"` or `"0b"`/`"0B"`.
/// @param[in] uppercase If true, uses uppercase letters for prefix and hex digits.
/// @param[in] width Minimum total field width (`0` for no padding).
/// @param[in] zeroPad If true, pads with `'0'` after the prefix; otherwise pads with `' '` before the prefix.
template <gp::USize N, typename InUIntType>
constexpr void appendRadixUnsigned(
    gp::container::BasicFixedString<char, N>& outBuffer,
    InUIntType value,
    const gp::UInt32 base,
    const bool includePrefix,
    const bool uppercase,
    const gp::UInt32 width = 0u,
    const bool zeroPad = false
) noexcept
{
    char digits[68]{};
    gp::USize digitCount = 0u;
    const char* const alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0u)
    {
        digits[digitCount++] = '0';
    }
    else if (base == 2u)
    {
        while (value > 0u && digitCount < sizeof(digits))
        {
            digits[digitCount++] = static_cast<char>('0' + static_cast<gp::USize>(value & static_cast<InUIntType>(1u)));
            value >>= 1u;
        }
    }
    else
    {
        while (value > 0u && digitCount < sizeof(digits))
        {
            digits[digitCount++] = alphabet[static_cast<gp::USize>(value & static_cast<InUIntType>(0x0Fu))];
            value >>= 4u;
        }
    }

    const gp::USize prefixLen = includePrefix ? 2u : 0u;
    const gp::USize totalUnpadded = prefixLen + digitCount;
    const gp::USize padCount =
        (static_cast<gp::USize>(width) > totalUnpadded) ? (static_cast<gp::USize>(width) - totalUnpadded) : 0u;

    if (!zeroPad)
    {
        for (gp::USize p = 0u; p < padCount; ++p)
        {
            appendCharSafe(outBuffer, ' ');
        }
    }

    if (includePrefix)
    {
        appendCharSafe(outBuffer, '0');
        if (base == 2u)
        {
            appendCharSafe(outBuffer, uppercase ? 'B' : 'b');
        }
        else
        {
            appendCharSafe(outBuffer, uppercase ? 'X' : 'x');
        }
    }

    if (zeroPad)
    {
        for (gp::USize p = 0u; p < padCount; ++p)
        {
            appendCharSafe(outBuffer, '0');
        }
    }

    while (digitCount > 0u)
    {
        --digitCount;
        appendCharSafe(outBuffer, digits[digitCount]);
    }
}

/// @brief Formats a single typed argument into `outBuffer` according to an optional format specifier (`spec`).
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @tparam InArgType Argument type being formatted.
/// @param[in,out] outBuffer Target fixed string buffer.
/// @param[in] arg Value to format.
/// @param[in] spec Inner format specifier view between `'{'` and `'}'` (e.g., `"#08x"` or `".2f"`).
template <gp::USize N, typename InArgType>
constexpr void formatSingleArgument(
    gp::container::BasicFixedString<char, N>& outBuffer, const InArgType& arg, const gp::StringView spec
) noexcept
{
    using DecayedType = std::remove_cvref_t<InArgType>;

    if constexpr (std::is_same_v<DecayedType, bool>)
    {
        appendViewSafe(outBuffer, arg ? gp::StringView("true") : gp::StringView("false"));
    }
    else if constexpr (std::is_same_v<DecayedType, char>)
    {
        appendCharSafe(outBuffer, arg);
    }
    else if constexpr (std::is_same_v<DecayedType, std::nullptr_t>)
    {
        appendViewSafe(outBuffer, "nullptr");
    }
    else if constexpr (std::is_same_v<DecayedType, LogLevel>)
    {
        appendViewSafe(outBuffer, logLevelToString(arg));
    }
    else if constexpr (std::is_same_v<DecayedType, gp::Name>)
    {
        const gp::FixedString<128> formattedName = arg.template toFixedString<128>();
        appendViewSafe(outBuffer, formattedName.view());
    }
    else if constexpr (std::is_pointer_v<DecayedType>)
    {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<DecayedType>>, char>)
        {
            if (arg != nullptr)
            {
                appendViewSafe(outBuffer, gp::StringView(arg));
            }
            else
            {
                appendViewSafe(outBuffer, "(null)");
            }
        }
        else
        {
            if (arg == nullptr)
            {
                appendViewSafe(outBuffer, "nullptr");
            }
            else
            {
                const gp::UIntPtr addr = reinterpret_cast<gp::UIntPtr>(arg);
                appendRadixUnsigned(outBuffer, addr, 16u, true, false);
            }
        }
    }
    else if constexpr (std::is_convertible_v<const DecayedType&, gp::StringView>)
    {
        const gp::StringView view(arg);
        appendViewSafe(outBuffer, view);
    }
    else if constexpr (std::is_integral_v<DecayedType>)
    {
        const ParsedFormatSpec parsed = parseFormatSpec(spec);
        if (parsed.typeChar == 'x' || parsed.typeChar == 'X')
        {
            using UnsignedEquiv = std::make_unsigned_t<DecayedType>;
            appendRadixUnsigned(
                outBuffer,
                static_cast<UnsignedEquiv>(arg),
                16u,
                parsed.alternateForm,
                parsed.typeChar == 'X',
                parsed.width,
                parsed.zeroPad
            );
            return;
        }

        if (parsed.typeChar == 'b' || parsed.typeChar == 'B')
        {
            using UnsignedEquiv = std::make_unsigned_t<DecayedType>;
            appendRadixUnsigned(
                outBuffer,
                static_cast<UnsignedEquiv>(arg),
                2u,
                parsed.alternateForm,
                parsed.typeChar == 'B',
                parsed.width,
                parsed.zeroPad
            );
            return;
        }

        char scratch[32]{};
        const std::to_chars_result res = std::to_chars(scratch, scratch + sizeof(scratch), arg);
        if (res.ec == std::errc{})
        {
            const gp::USize written = static_cast<gp::USize>(res.ptr - scratch);
            const gp::USize padCount = (static_cast<gp::USize>(parsed.width) > written)
                                           ? (static_cast<gp::USize>(parsed.width) - written)
                                           : 0u;
            if (padCount > 0u)
            {
                if (parsed.zeroPad && written > 0u && scratch[0] == '-')
                {
                    appendCharSafe(outBuffer, '-');
                    for (gp::USize p = 0u; p < padCount; ++p)
                    {
                        appendCharSafe(outBuffer, '0');
                    }
                    appendViewSafe(outBuffer, gp::StringView(scratch + 1, written - 1u));
                    return;
                }

                const char padChar = parsed.zeroPad ? '0' : ' ';
                for (gp::USize p = 0u; p < padCount; ++p)
                {
                    appendCharSafe(outBuffer, padChar);
                }
            }
            appendViewSafe(outBuffer, gp::StringView(scratch, written));
        }
    }
    else if constexpr (std::is_floating_point_v<DecayedType>)
    {
        char scratch[64]{};
        std::to_chars_result res{};
        const ParsedFormatSpec parsed = parseFormatSpec(spec);
        const bool isScientific = (parsed.typeChar == 'e' || parsed.typeChar == 'E');
        const std::chars_format fmtMode = isScientific ? std::chars_format::scientific : std::chars_format::fixed;

        if (parsed.precision >= 0)
        {
            res = std::to_chars(scratch, scratch + sizeof(scratch), arg, fmtMode, parsed.precision);
        }
        else if (isScientific)
        {
            res = std::to_chars(scratch, scratch + sizeof(scratch), arg, std::chars_format::scientific);
        }
        else
        {
            res = std::to_chars(scratch, scratch + sizeof(scratch), arg);
        }

        if (res.ec == std::errc{})
        {
            const gp::USize written = static_cast<gp::USize>(res.ptr - scratch);
            appendViewSafe(outBuffer, gp::StringView(scratch, written));
        }
    }
    else if constexpr (std::is_enum_v<DecayedType>)
    {
        using Underlying = std::underlying_type_t<DecayedType>;
        formatSingleArgument(outBuffer, static_cast<Underlying>(arg), spec);
    }
    else
    {
        appendViewSafe(outBuffer, "<unformattable>");
    }
}

/// @brief Advances through `fmt` until the next unescaped `{...}` placeholder, appending literal characters to
///        `outBuffer` and extracting `outSpec`.
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @param[in,out] outBuffer Target fixed string buffer.
/// @param[in] fmt Full format string view.
/// @param[in,out] ioCursor Current character index within `fmt`.
/// @param[out] outSpec Receives the inner specifier substring if a placeholder was found.
/// @return True if an unescaped `{...}` placeholder was consumed, false if the end of `fmt` was reached.
template <gp::USize N>
constexpr bool consumeUntilNextPlaceholder(
    gp::container::BasicFixedString<char, N>& outBuffer,
    const gp::StringView fmt,
    gp::USize& ioCursor,
    gp::StringView& outSpec
) noexcept
{
    const gp::USize len = fmt.size();
    outSpec = gp::StringView{};

    while (ioCursor < len)
    {
        const char ch = fmt[ioCursor];
        if (ch == '{')
        {
            if (ioCursor + 1u < len && fmt[ioCursor + 1u] == '{')
            {
                appendCharSafe(outBuffer, '{');
                ioCursor += 2u;
                continue;
            }

            ++ioCursor;
            const gp::USize specStart = ioCursor;
            while (ioCursor < len && fmt[ioCursor] != '}')
            {
                ++ioCursor;
            }

            const gp::USize specLen = ioCursor - specStart;
            if (ioCursor < len && fmt[ioCursor] == '}')
            {
                ++ioCursor;
            }

            gp::StringView rawSpec = fmt.substr(specStart, specLen);
            if (!rawSpec.isEmpty() && rawSpec[0] == ':')
            {
                rawSpec.removePrefix(1u);
            }
            outSpec = rawSpec;
            return true;
        }
        else if (ch == '}' && ioCursor + 1u < len && fmt[ioCursor + 1u] == '}')
        {
            appendCharSafe(outBuffer, '}');
            ioCursor += 2u;
        }
        else
        {
            appendCharSafe(outBuffer, ch);
            ++ioCursor;
        }
    }

    return false;
}

/// @brief Appends all remaining characters after the last replaced placeholder, unescaping `{{` and `}}`.
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @param[in,out] outBuffer Target fixed string buffer.
/// @param[in] fmt Full format string view.
/// @param[in,out] ioCursor Current character index within `fmt`.
template <gp::USize N>
constexpr void flushRemainingLiteralText(
    gp::container::BasicFixedString<char, N>& outBuffer, const gp::StringView fmt, gp::USize& ioCursor
) noexcept
{
    const gp::USize len = fmt.size();
    while (ioCursor < len)
    {
        const char ch = fmt[ioCursor];
        if ((ch == '{' || ch == '}') && ioCursor + 1u < len && fmt[ioCursor + 1u] == ch)
        {
            appendCharSafe(outBuffer, ch);
            ioCursor += 2u;
        }
        else
        {
            appendCharSafe(outBuffer, ch);
            ++ioCursor;
        }
    }
}

}   // namespace detail

/// @brief Formats `fmt` and `args...` into a caller-provided `BasicFixedString` with zero heap allocations
///        ([ADR-0054]).
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @tparam InArgs Variadic argument types.
/// @param[out] outBuffer Target fixed string buffer (cleared prior to formatting).
/// @param[in] fmt Format string view containing `{}` or `{:...}` placeholders.
/// @param[in] args Arguments to substitute into `fmt`.
/// @return Non-owning `StringView` spanning the formatted characters in `outBuffer`.
template <gp::USize N, typename... InArgs>
constexpr gp::StringView formatTo(
    gp::container::BasicFixedString<char, N>& outBuffer, const gp::StringView fmt, const InArgs&... args
) noexcept
{
    outBuffer.clear();
    gp::USize cursor = 0u;
    gp::StringView spec{};

    (
        [&outBuffer, fmt, &cursor, &spec, &args]() noexcept -> void
    {
        if (detail::consumeUntilNextPlaceholder(outBuffer, fmt, cursor, spec))
        {
            detail::formatSingleArgument(outBuffer, args, spec);
        }
    }(),
        ...);

    detail::flushRemainingLiteralText(outBuffer, fmt, cursor);
    return outBuffer.view();
}

/// @brief Formats a complete human-readable log line (`[HH:MM:SS.mmm][T:id][LEVEL][Category] message\n`) into a fixed
///        stack buffer for text sinks (`ConsoleSink`, `FileSink`, `DebugOutputSink`).
/// @tparam N Capacity of the destination `BasicFixedString`.
/// @param[out] outLine Destination fixed buffer receiving the formatted line.
/// @param[in] record Read-only log record view.
/// @param[in] flags Formatting flags (`IncludeTimestamp`, `IncludeThreadAndFiber`, `IncludeSourceLocation`,
///                  `EnableAnsiColors`).
/// @return `StringView` of the formatted line inside `outLine`.
template <gp::USize N>
gp::StringView formatLogRecordLine(
    gp::container::BasicFixedString<char, N>& outLine, const LogRecordView& record, const LogSinkFlags flags
) noexcept
{
    outLine.clear();

    const bool useColors = (flags & LogSinkFlags::EnableAnsiColors) != LogSinkFlags::None;
    if (useColors)
    {
        switch (record.level)
        {
        case LogLevel::Trace:
            detail::appendViewSafe(outLine, "\x1b[90m");
            break;
        case LogLevel::Debug:
            detail::appendViewSafe(outLine, "\x1b[36m");
            break;
        case LogLevel::Info:
            detail::appendViewSafe(outLine, "\x1b[32m");
            break;
        case LogLevel::Warning:
            detail::appendViewSafe(outLine, "\x1b[33m");
            break;
        case LogLevel::Error:
            detail::appendViewSafe(outLine, "\x1b[31m");
            break;
        case LogLevel::Fatal:
            detail::appendViewSafe(outLine, "\x1b[1;97;41m");
            break;
        case LogLevel::Off:
            // falls through
        default:
            break;
        }
    }

    if ((flags & LogSinkFlags::IncludeTimestamp) != LogSinkFlags::None)
    {
        const gp::platform::DateTime& dt = record.dateTime;
        char timeBuf[16]{};
        timeBuf[0] = '[';
        timeBuf[1] = static_cast<char>('0' + ((dt.hour / 10u) % 10u));
        timeBuf[2] = static_cast<char>('0' + (dt.hour % 10u));
        timeBuf[3] = ':';
        timeBuf[4] = static_cast<char>('0' + ((dt.minute / 10u) % 10u));
        timeBuf[5] = static_cast<char>('0' + (dt.minute % 10u));
        timeBuf[6] = ':';
        timeBuf[7] = static_cast<char>('0' + ((dt.second / 10u) % 10u));
        timeBuf[8] = static_cast<char>('0' + (dt.second % 10u));
        timeBuf[9] = '.';
        timeBuf[10] = static_cast<char>('0' + ((dt.millisecond / 100u) % 10u));
        timeBuf[11] = static_cast<char>('0' + ((dt.millisecond / 10u) % 10u));
        timeBuf[12] = static_cast<char>('0' + (dt.millisecond % 10u));
        timeBuf[13] = ']';
        detail::appendViewSafe(outLine, gp::StringView(timeBuf, 14u));
    }

    if ((flags & LogSinkFlags::IncludeThreadAndFiber) != LogSinkFlags::None)
    {
        detail::appendViewSafe(outLine, "[T:");
        detail::formatSingleArgument(outLine, record.threadId, gp::StringView{});
        if (record.fiberId != gp::thread::kInvalidFiberId)
        {
            detail::appendViewSafe(outLine, " F:");
            detail::formatSingleArgument(outLine, record.fiberId, gp::StringView{});
        }
        detail::appendCharSafe(outLine, ']');
    }

    detail::appendCharSafe(outLine, '[');
    detail::appendViewSafe(outLine, logLevelToString(record.level));
    detail::appendViewSafe(outLine, "][");

    if (!record.categoryName.isEmpty())
    {
        detail::appendViewSafe(outLine, record.categoryName);
    }
    else
    {
        detail::formatSingleArgument(outLine, record.category, gp::StringView{});
    }
    detail::appendCharSafe(outLine, ']');

    if ((flags & LogSinkFlags::IncludeSourceLocation) != LogSinkFlags::None && record.location.file != nullptr &&
        record.location.file[0] != '\0')
    {
        const gp::StringView fullPath(record.location.file);
        const gp::USize lastSlash = fullPath.findLastOf("/\\");
        const gp::StringView baseName =
            (lastSlash != gp::StringView::npos) ? fullPath.substr(lastSlash + 1u) : fullPath;
        detail::appendCharSafe(outLine, '[');
        detail::appendViewSafe(outLine, baseName);
        detail::appendCharSafe(outLine, ':');
        detail::formatSingleArgument(outLine, record.location.line, gp::StringView{});
        detail::appendCharSafe(outLine, ']');
    }

    detail::appendCharSafe(outLine, ' ');
    detail::appendViewSafe(outLine, record.message);

    if (useColors)
    {
        detail::appendViewSafe(outLine, "\x1b[0m");
    }

    detail::appendCharSafe(outLine, '\n');
    return outLine.view();
}

}   // namespace gp::log
