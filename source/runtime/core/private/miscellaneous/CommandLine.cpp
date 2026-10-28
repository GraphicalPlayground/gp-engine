// File: source/runtime/core/private/miscellaneous/CommandLine.cpp
// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "miscellaneous/CommandLine.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <limits>
#include <ranges>   // IWYU pragma: keep
#include <system_error>

namespace gp
{

namespace detail
{

/// @brief Global static .bss storage for tokenized arguments and raw command-line reconstruction.
char s_arena[CommandLine::kMaxBufferBytes]{};
USize s_arenaOffset = 0u;

char s_reconstructedBuffer[CommandLine::kMaxBufferBytes]{};
USize s_reconstructedLength = 0u;

StringView s_args[CommandLine::kMaxArguments]{};
UInt32 s_argCount = 0u;

/// @brief Converts an ASCII character to lowercase without locale overhead.
/// @param[in] ch Input character.
/// @return Lowercase ASCII character.
[[nodiscard]] constexpr char toLowerAscii(const char ch) noexcept
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return static_cast<char>(ch + ('a' - 'A'));
    }
    return ch;
}

/// @brief Checks whether a character is an ASCII whitespace character.
/// @param[in] ch Input character.
/// @return True if whitespace, false otherwise.
[[nodiscard]] constexpr bool isAsciiWhitespace(const char ch) noexcept
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

/// @brief Checks whether a character is an ASCII decimal digit.
/// @param[in] ch Input character.
/// @return True if in ['0'..'9'], false otherwise.
[[nodiscard]] constexpr bool isAsciiDigit(const char ch) noexcept
{
    return ch >= '0' && ch <= '9';
}

/// @brief Compares two StringViews for equality with optional ASCII case-insensitivity using std::ranges.
/// @param[in] lhs First string view.
/// @param[in] rhs Second string view.
/// @param[in] caseSensitive True for exact byte match, false for ASCII case-insensitive match.
/// @return True if equal under the specified sensitivity policy.
[[nodiscard]] constexpr bool equalsView(const StringView lhs, const StringView rhs, const bool caseSensitive) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    if (caseSensitive)
    {
        return std::ranges::equal(lhs, rhs);
    }

    const auto caseInsensitiveCmp = [](const char a, const char b) noexcept -> bool
    {
        return toLowerAscii(a) == toLowerAscii(b);
    };

    return std::ranges::equal(lhs, rhs, caseInsensitiveCmp);
}

/// @brief Strips up to two leading '-' characters from a key or token (e.g. "--map" -> "map", "-map" -> "map").
/// @param[in] token Input key or flag view.
/// @return Sub-view with leading dashes removed.
[[nodiscard]] constexpr StringView stripLeadingDashes(const StringView token) noexcept
{
    if (token.size() >= 2u && token[0] == '-' && token[1] == '-')
    {
        return StringView(token.data() + 2u, token.size() - 2u);
    }
    if (token.size() >= 1u && token[0] == '-')
    {
        return StringView(token.data() + 1u, token.size() - 1u);
    }
    return token;
}

/// @brief Strips matching surrounding double or single quotes from a value view.
/// @param[in] value Input string view.
/// @return Unquoted string view.
[[nodiscard]] constexpr StringView stripOuterQuotes(const StringView value) noexcept
{
    if (value.size() >= 2u)
    {
        const char first = value[0];
        const char last = value[value.size() - 1u];
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            return StringView(value.data() + 1u, value.size() - 2u);
        }
    }
    return value;
}

/// @brief Determines whether a raw command-line token represents a flag/option rather than a value or negative number.
/// @details A token starting with '-' is treated as a negative numeric literal (not a flag) if the character
///          immediately following '-' is a digit ('0'..'9') or a decimal point ('.') followed by a digit.
/// @param[in] token Raw argument token.
/// @return True if the token is a flag/option, false if it is a value or positional argument.
[[nodiscard]] constexpr bool isFlagToken(const StringView token) noexcept
{
    if (token.size() < 2u || token[0] != '-')
    {
        return false;
    }

    if (token == StringView("--"))
    {
        return false;
    }

    const char second = token[1];
    if (isAsciiDigit(second))
    {
        return false;
    }

    if (second == '.' && token.size() >= 3u && isAsciiDigit(token[2]))
    {
        return false;
    }

    return true;
}

/// @brief Splits a flag token into its normalized key name and optional inline value ("-key=val" or "-key:val").
/// @param[in] token Raw token guaranteed to start with '-'.
/// @param[out] outNormalizedKey Receives the key with leading '-'/'--' stripped.
/// @param[out] outInlineValue Receives the inline value if '=' or ':' was present.
/// @param[out] outHasInlineValue Set to true if '=' or ':' separator was found.
constexpr void splitFlagToken(
    const StringView token, StringView& outNormalizedKey, StringView& outInlineValue, bool& outHasInlineValue
) noexcept
{
    const StringView body = stripLeadingDashes(token);

    for (USize i = 0u; i < body.size(); ++i)
    {
        if (body[i] == '=' || body[i] == ':')
        {
            outNormalizedKey = StringView(body.data(), i);
            outInlineValue = stripOuterQuotes(StringView(body.data() + i + 1u, body.size() - i - 1u));
            outHasInlineValue = true;
            return;
        }
    }

    outNormalizedKey = body;
    outInlineValue = StringView{};
    outHasInlineValue = false;
}

/// @brief Copies a token into the static .bss arena and registers it in s_args.
/// @param[in] tokenView Token characters to copy.
/// @return True if stored, false if capacity was exhausted.
bool pushTokenToArena(const StringView tokenView) noexcept
{
    if (s_argCount >= CommandLine::kMaxArguments)
    {
        return false;
    }

    const StringView cleanToken = stripOuterQuotes(tokenView);
    if ((s_arenaOffset + cleanToken.size() + 1u) > CommandLine::kMaxBufferBytes)
    {
        return false;
    }

    char* const dest = s_arena + s_arenaOffset;
    if (!cleanToken.isEmpty())
    {
        std::memcpy(dest, cleanToken.data(), cleanToken.size());
    }
    dest[cleanToken.size()] = '\0';

    s_args[s_argCount] = StringView(dest, cleanToken.size());
    ++s_argCount;
    s_arenaOffset += cleanToken.size() + 1u;
    return true;
}

/// @brief Rebuilds s_reconstructedBuffer from tokens [1..s_argCount).
void rebuildReconstructedCommandLine() noexcept
{
    s_reconstructedLength = 0u;
    s_reconstructedBuffer[0] = '\0';

    for (UInt32 i = 1u; i < s_argCount; ++i)
    {
        const StringView tok = s_args[i];
        const bool needsSpace = (i > 1u);
        const USize required = (needsSpace ? 1u : 0u) + tok.size();

        if ((s_reconstructedLength + required + 1u) > CommandLine::kMaxBufferBytes)
        {
            break;
        }

        if (needsSpace)
        {
            s_reconstructedBuffer[s_reconstructedLength] = ' ';
            ++s_reconstructedLength;
        }

        if (!tok.isEmpty())
        {
            std::memcpy(s_reconstructedBuffer + s_reconstructedLength, tok.data(), tok.size());
            s_reconstructedLength += tok.size();
        }
    }

    s_reconstructedBuffer[s_reconstructedLength] = '\0';
}

/// @brief Tokenizes a raw command-line string into the static .bss arena, respecting quotes.
/// @param[in] rawString Input command-line string view.
/// @return Number of tokens appended.
UInt32 tokenizeRawStringIntoArena(const StringView rawString) noexcept
{
    UInt32 addedCount = 0u;
    const USize totalLen = rawString.size();
    USize cursor = 0u;

    while (cursor < totalLen && s_argCount < CommandLine::kMaxArguments)
    {
        while (cursor < totalLen && isAsciiWhitespace(rawString[cursor]))
        {
            ++cursor;
        }

        if (cursor >= totalLen)
        {
            break;
        }

        char tempToken[CommandLine::kMaxBufferBytes]{};
        USize tokenLen = 0u;
        char activeQuote = '\0';

        while (cursor < totalLen)
        {
            const char ch = rawString[cursor];
            if (activeQuote != '\0')
            {
                if (ch == activeQuote)
                {
                    activeQuote = '\0';
                    ++cursor;
                    continue;
                }
                if (tokenLen + 1u < CommandLine::kMaxBufferBytes)
                {
                    tempToken[tokenLen] = ch;
                    ++tokenLen;
                }
                ++cursor;
            }
            else
            {
                if (isAsciiWhitespace(ch))
                {
                    break;
                }
                if (ch == '"' || ch == '\'')
                {
                    activeQuote = ch;
                    ++cursor;
                    continue;
                }
                if (tokenLen + 1u < CommandLine::kMaxBufferBytes)
                {
                    tempToken[tokenLen] = ch;
                    ++tokenLen;
                }
                ++cursor;
            }
        }

        if (pushTokenToArena(StringView(tempToken, tokenLen)))
        {
            ++addedCount;
        }
        else
        {
            break;
        }
    }

    return addedCount;
}

/// @brief Evaluates a string literal ("true", "false", "1", "0", "on", "off", "yes", "no") as a boolean.
/// @param[in] literal Value string view.
/// @return Optional<bool> if recognized, or isEmpty Optional if unrecognized.
[[nodiscard]] constexpr Optional<bool> parseBooleanLiteral(const StringView literal) noexcept
{
    if (equalsView(literal, "true", false) || equalsView(literal, "1", false) || equalsView(literal, "on", false) ||
        equalsView(literal, "yes", false))
    {
        return Optional<bool>(true);
    }

    if (equalsView(literal, "false", false) || equalsView(literal, "0", false) || equalsView(literal, "off", false) ||
        equalsView(literal, "no", false))
    {
        return Optional<bool>(false);
    }

    return Optional<bool>{};
}

/// @brief Finds the index of the standalone "--" option terminator if present.
/// @return Index of "--", or s_argCount if none exists.
[[nodiscard]] UInt32 findDoubleDashTerminatorIndex() noexcept
{
    for (UInt32 i = 1u; i < s_argCount; ++i)
    {
        if (s_args[i] == StringView("--"))
        {
            return i;
        }
    }
    return s_argCount;
}

}   // namespace detail

void CommandLine::initialize(const Int32 argc, const char* const* const argv) noexcept
{
    shutdown();

    if (argc <= 0 || argv == nullptr)
    {
        return;
    }

    const UInt32 limit = (static_cast<UInt32>(argc) < kMaxArguments) ? static_cast<UInt32>(argc) : kMaxArguments;

    for (UInt32 i = 0u; i < limit; ++i)
    {
        if (argv[i] != nullptr)
        {
            const USize len = std::strlen(argv[i]);
            if (!detail::pushTokenToArena(StringView(argv[i], len)))
            {
                break;
            }
        }
    }

    detail::rebuildReconstructedCommandLine();
}

void CommandLine::initialize(const StringView rawCommandLine) noexcept
{
    shutdown();

    if (rawCommandLine.isEmpty())
    {
        return;
    }

    (void)detail::tokenizeRawStringIntoArena(rawCommandLine);
    detail::rebuildReconstructedCommandLine();
}

UInt32 CommandLine::append(const StringView additionalArgs) noexcept
{
    if (additionalArgs.isEmpty())
    {
        return 0u;
    }

    // If CommandLine was never initialized, reserve slot 0 for a placeholder executable name
    if (detail::s_argCount == 0u)
    {
        (void)detail::pushTokenToArena(StringView("gp-engine"));
    }

    const UInt32 added = detail::tokenizeRawStringIntoArena(additionalArgs);
    if (added > 0u)
    {
        detail::rebuildReconstructedCommandLine();
    }
    return added;
}

void CommandLine::shutdown() noexcept
{
    for (UInt32 i = 0u; i < detail::s_argCount; ++i)
    {
        detail::s_args[i] = StringView{};
    }
    detail::s_argCount = 0u;
    detail::s_arenaOffset = 0u;
    detail::s_arena[0] = '\0';
    detail::s_reconstructedLength = 0u;
    detail::s_reconstructedBuffer[0] = '\0';
}

bool CommandLine::isInitialized() noexcept
{
    return detail::s_argCount > 0u;
}

StringView CommandLine::getOriginalCommandLine() noexcept
{
    return StringView(detail::s_reconstructedBuffer, detail::s_reconstructedLength);
}

StringView CommandLine::getExecutablePath() noexcept
{
    return (detail::s_argCount > 0u) ? detail::s_args[0] : StringView{};
}

UInt32 CommandLine::getArgumentCount() noexcept
{
    return detail::s_argCount;
}

StringView CommandLine::getArgument(const UInt32 index) noexcept
{
    return (index < detail::s_argCount) ? detail::s_args[index] : StringView{};
}

UInt32 CommandLine::getPositionalArguments(StringView* const outPositional, const UInt32 maxCapacity) noexcept
{
    if (outPositional == nullptr || maxCapacity == 0u || detail::s_argCount <= 1u)
    {
        return 0u;
    }

    UInt32 written = 0u;
    bool afterDoubleDash = false;

    for (UInt32 i = 1u; i < detail::s_argCount && written < maxCapacity; ++i)
    {
        const StringView tok = detail::s_args[i];

        if (!afterDoubleDash && tok == StringView("--"))
        {
            afterDoubleDash = true;
            continue;
        }

        if (afterDoubleDash)
        {
            outPositional[written] = tok;
            ++written;
            continue;
        }

        if (detail::isFlagToken(tok))
        {
            StringView normKey{};
            StringView inlineVal{};
            bool hasInline = false;
            detail::splitFlagToken(tok, normKey, inlineVal, hasInline);

            // If this flag had no '=' or ':' and the next token is a non-flag value, skip the consumed value
            if (!hasInline && (i + 1u) < detail::s_argCount && detail::s_args[i + 1u] != StringView("--") &&
                !detail::isFlagToken(detail::s_args[i + 1u]))
            {
                ++i;
            }
            continue;
        }

        outPositional[written] = tok;
        ++written;
    }

    return written;
}

bool CommandLine::hasArgument(const StringView key, const bool caseSensitive) noexcept
{
    const StringView targetKey = detail::stripLeadingDashes(key);
    if (targetKey.isEmpty() || detail::s_argCount <= 1u)
    {
        return false;
    }

    const UInt32 stopIdx = detail::findDoubleDashTerminatorIndex();

    for (UInt32 i = 1u; i < stopIdx; ++i)
    {
        if (!detail::isFlagToken(detail::s_args[i]))
        {
            continue;
        }

        StringView normKey{};
        StringView inlineVal{};
        bool hasInline = false;
        detail::splitFlagToken(detail::s_args[i], normKey, inlineVal, hasInline);

        if (detail::equalsView(normKey, targetKey, caseSensitive))
        {
            return true;
        }
    }

    return false;
}

Optional<StringView> CommandLine::tryGetString(const StringView key, const bool caseSensitive) noexcept
{
    const StringView targetKey = detail::stripLeadingDashes(key);
    if (targetKey.isEmpty() || detail::s_argCount <= 1u)
    {
        return Optional<StringView>{};
    }

    const UInt32 stopIdx = detail::findDoubleDashTerminatorIndex();

    // Reverse scan for AAA last-wins override behavior
    for (UInt32 idx = stopIdx; idx > 1u; --idx)
    {
        const UInt32 i = idx - 1u;
        if (!detail::isFlagToken(detail::s_args[i]))
        {
            continue;
        }

        StringView normKey{};
        StringView inlineVal{};
        bool hasInline = false;
        detail::splitFlagToken(detail::s_args[i], normKey, inlineVal, hasInline);

        if (!detail::equalsView(normKey, targetKey, caseSensitive))
        {
            continue;
        }

        if (hasInline)
        {
            return Optional<StringView>(inlineVal);
        }

        if ((i + 1u) < stopIdx && !detail::isFlagToken(detail::s_args[i + 1u]))
        {
            return Optional<StringView>(detail::s_args[i + 1u]);
        }

        // Key was present as a bare flag with no value
        return Optional<StringView>{};
    }

    return Optional<StringView>{};
}

StringView CommandLine::getValue(const StringView key, const StringView defaultValue, const bool caseSensitive) noexcept
{
    const Optional<StringView> found = tryGetString(key, caseSensitive);
    return found.hasValue() ? *found : defaultValue;
}

UInt32 CommandLine::getValues(
    const StringView key, StringView* const outValues, const UInt32 maxCapacity, const bool caseSensitive
) noexcept
{
    const StringView targetKey = detail::stripLeadingDashes(key);
    if (targetKey.isEmpty() || outValues == nullptr || maxCapacity == 0u || detail::s_argCount <= 1u)
    {
        return 0u;
    }

    const UInt32 stopIdx = detail::findDoubleDashTerminatorIndex();
    UInt32 count = 0u;

    for (UInt32 i = 1u; i < stopIdx && count < maxCapacity; ++i)
    {
        if (!detail::isFlagToken(detail::s_args[i]))
        {
            continue;
        }

        StringView normKey{};
        StringView inlineVal{};
        bool hasInline = false;
        detail::splitFlagToken(detail::s_args[i], normKey, inlineVal, hasInline);

        if (!detail::equalsView(normKey, targetKey, caseSensitive))
        {
            continue;
        }

        if (hasInline)
        {
            outValues[count] = inlineVal;
            ++count;
        }
        else if ((i + 1u) < stopIdx && !detail::isFlagToken(detail::s_args[i + 1u]))
        {
            outValues[count] = detail::s_args[i + 1u];
            ++count;
            ++i;
        }
    }

    return count;
}

Optional<bool> CommandLine::tryGetBool(const StringView key, const bool caseSensitive) noexcept
{
    const StringView targetKey = detail::stripLeadingDashes(key);
    if (targetKey.isEmpty() || detail::s_argCount <= 1u)
    {
        return Optional<bool>{};
    }

    const UInt32 stopIdx = detail::findDoubleDashTerminatorIndex();

    // Reverse scan so later "-no-vsync" or "-vsync" overrides earlier flags
    for (UInt32 idx = stopIdx; idx > 1u; --idx)
    {
        const UInt32 i = idx - 1u;
        if (!detail::isFlagToken(detail::s_args[i]))
        {
            continue;
        }

        StringView normKey{};
        StringView inlineVal{};
        bool hasInline = false;
        detail::splitFlagToken(detail::s_args[i], normKey, inlineVal, hasInline);

        // Check direct key match (e.g., "-vsync", "-vsync=false", "-vsync off")
        if (detail::equalsView(normKey, targetKey, caseSensitive))
        {
            if (hasInline)
            {
                return detail::parseBooleanLiteral(inlineVal);
            }

            if ((i + 1u) < stopIdx && !detail::isFlagToken(detail::s_args[i + 1u]))
            {
                const Optional<bool> nextLiteral = detail::parseBooleanLiteral(detail::s_args[i + 1u]);
                if (nextLiteral.hasValue())
                {
                    return nextLiteral;
                }
            }

            return Optional<bool>(true);
        }

        // Check "-no-<key>" or "--no-<key>" negation prefix
        if (normKey.size() == (targetKey.size() + 3u))
        {
            const StringView prefix(normKey.data(), 3u);
            const StringView suffix(normKey.data() + 3u, targetKey.size());
            if (detail::equalsView(prefix, "no-", false) && detail::equalsView(suffix, targetKey, caseSensitive))
            {
                return Optional<bool>(false);
            }
        }
    }

    return Optional<bool>{};
}

Optional<Int64> CommandLine::tryGetInt64(const StringView key, const bool caseSensitive) noexcept
{
    const Optional<StringView> rawOpt = tryGetString(key, caseSensitive);
    if (!rawOpt.hasValue() || rawOpt->isEmpty())
    {
        return Optional<Int64>{};
    }

    StringView str = *rawOpt;
    if (str[0] == '+')
    {
        str = StringView(str.data() + 1u, str.size() - 1u);
        if (str.isEmpty())
        {
            return Optional<Int64>{};
        }
    }

    const bool isNegative = (str[0] == '-');
    const StringView magnitude = isNegative ? StringView(str.data() + 1u, str.size() - 1u) : str;

    int base = 10;
    StringView digits = str;

    if (magnitude.size() > 2u && magnitude[0] == '0' && (magnitude[1] == 'x' || magnitude[1] == 'X'))
    {
        base = 16;
        digits = StringView(magnitude.data() + 2u, magnitude.size() - 2u);
    }
    else if (magnitude.size() > 2u && magnitude[0] == '0' && (magnitude[1] == 'b' || magnitude[1] == 'B'))
    {
        base = 2;
        digits = StringView(magnitude.data() + 2u, magnitude.size() - 2u);
    }

    if (digits.isEmpty())
    {
        return Optional<Int64>{};
    }

    if (base != 10)
    {
        UInt64 unsignedVal = 0ULL;
        const char* const begin = digits.data();
        const char* const end = begin + digits.size();
        const std::from_chars_result res = std::from_chars(begin, end, unsignedVal, base);

        if (res.ec != std::errc{} || res.ptr != end)
        {
            return Optional<Int64>{};
        }

        if (isNegative)
        {
            constexpr UInt64 kMaxAbsNeg = static_cast<UInt64>(std::numeric_limits<Int64>::max()) + 1ULL;
            if (unsignedVal > kMaxAbsNeg)
            {
                return Optional<Int64>{};
            }
            if (unsignedVal == kMaxAbsNeg)
            {
                return Optional<Int64>(std::numeric_limits<Int64>::min());
            }
            return Optional<Int64>(-static_cast<Int64>(unsignedVal));
        }

        if (unsignedVal > static_cast<UInt64>(std::numeric_limits<Int64>::max()))
        {
            return Optional<Int64>{};
        }
        return Optional<Int64>(static_cast<Int64>(unsignedVal));
    }

    Int64 parsedVal = 0LL;
    const char* const begin = digits.data();
    const char* const end = begin + digits.size();
    const std::from_chars_result res = std::from_chars(begin, end, parsedVal, 10);

    if (res.ec != std::errc{} || res.ptr != end)
    {
        return Optional<Int64>{};
    }

    return Optional<Int64>(parsedVal);
}

Optional<Int32> CommandLine::tryGetInt32(const StringView key, const bool caseSensitive) noexcept
{
    const Optional<Int64> val64 = tryGetInt64(key, caseSensitive);
    if (!val64.hasValue())
    {
        return Optional<Int32>{};
    }

    if (*val64 < static_cast<Int64>(std::numeric_limits<Int32>::min()) ||
        *val64 > static_cast<Int64>(std::numeric_limits<Int32>::max()))
    {
        return Optional<Int32>{};
    }

    return Optional<Int32>(static_cast<Int32>(*val64));
}

Optional<UInt64> CommandLine::tryGetUInt64(const StringView key, const bool caseSensitive) noexcept
{
    const Optional<StringView> rawOpt = tryGetString(key, caseSensitive);
    if (!rawOpt.hasValue() || rawOpt->isEmpty())
    {
        return Optional<UInt64>{};
    }

    StringView str = *rawOpt;
    if (str[0] == '-')
    {
        return Optional<UInt64>{};
    }

    if (str[0] == '+')
    {
        str = StringView(str.data() + 1u, str.size() - 1u);
    }

    int base = 10;
    if (str.size() > 2u && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        base = 16;
        str = StringView(str.data() + 2u, str.size() - 2u);
    }
    else if (str.size() > 2u && str[0] == '0' && (str[1] == 'b' || str[1] == 'B'))
    {
        base = 2;
        str = StringView(str.data() + 2u, str.size() - 2u);
    }

    if (str.isEmpty())
    {
        return Optional<UInt64>{};
    }

    UInt64 parsedVal = 0ULL;
    const char* const begin = str.data();
    const char* const end = begin + str.size();
    const std::from_chars_result res = std::from_chars(begin, end, parsedVal, base);

    if (res.ec != std::errc{} || res.ptr != end)
    {
        return Optional<UInt64>{};
    }

    return Optional<UInt64>(parsedVal);
}

Optional<UInt32> CommandLine::tryGetUInt32(const StringView key, const bool caseSensitive) noexcept
{
    const Optional<UInt64> val64 = tryGetUInt64(key, caseSensitive);
    if (!val64.hasValue())
    {
        return Optional<UInt32>{};
    }

    if (*val64 > static_cast<UInt64>(std::numeric_limits<UInt32>::max()))
    {
        return Optional<UInt32>{};
    }

    return Optional<UInt32>(static_cast<UInt32>(*val64));
}

Optional<Float32> CommandLine::tryGetFloat32(const StringView key, const bool caseSensitive) noexcept
{
    const Optional<StringView> rawOpt = tryGetString(key, caseSensitive);
    if (!rawOpt.hasValue() || rawOpt->isEmpty())
    {
        return Optional<Float32>{};
    }

    StringView str = *rawOpt;
    if (str[0] == '+')
    {
        str = StringView(str.data() + 1u, str.size() - 1u);
    }

    if (str.isEmpty())
    {
        return Optional<Float32>{};
    }

    Float32 parsedVal = 0.0f;
    const char* const begin = str.data();
    const char* const end = begin + str.size();
    const std::from_chars_result res = std::from_chars(begin, end, parsedVal);

    if (res.ec != std::errc{} || res.ptr != end)
    {
        return Optional<Float32>{};
    }

    return Optional<Float32>(parsedVal);
}

Optional<Float64> CommandLine::tryGetFloat64(const StringView key, const bool caseSensitive) noexcept
{
    const Optional<StringView> rawOpt = tryGetString(key, caseSensitive);
    if (!rawOpt.hasValue() || rawOpt->isEmpty())
    {
        return Optional<Float64>{};
    }

    StringView str = *rawOpt;
    if (str[0] == '+')
    {
        str = StringView(str.data() + 1u, str.size() - 1u);
    }

    if (str.isEmpty())
    {
        return Optional<Float64>{};
    }

    Float64 parsedVal = 0.0;
    const char* const begin = str.data();
    const char* const end = begin + str.size();
    const std::from_chars_result res = std::from_chars(begin, end, parsedVal);

    if (res.ec != std::errc{} || res.ptr != end)
    {
        return Optional<Float64>{};
    }

    return Optional<Float64>(parsedVal);
}

}   // namespace gp
