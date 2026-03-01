// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Utilities/StringUtilities.hpp"

namespace GP::Utilities
{

namespace Internal
{

/// \brief Invokes callback(Container::TBasicStringView<CharType>) for each detected word in str.
///
/// Word boundaries are detected by:
///   1. Delimiter characters: space, underscore, hyphen, period.
///   2. Lowercase-to-Uppercase transition ("helloWorld" -> "hello" | "World").
///   3. Acronym-end transition: uppercase char whose *next* char is lowercase and the
///      current accumulated word has length > 1 ("HTMLParser" -> "HTML" | "Parser",
///      "getHTTPSUrl" -> "get" | "HTTPS" | "Url").
///
/// \tparam Fn Callable with signature void(Container::TBasicStringView<CharType>).
template <Concepts::IsCharacter CharType, typename Fn>
void CollectWords(Container::TBasicStringView<CharType> str, Fn&& callback)
{
    constexpr SizeT NPOS = Container::TBasicStringView<CharType>::NPOS;

    SizeT n = str.Size();
    SizeT wordStart = NPOS;

    auto FlushWord = [&](SizeT end)
    {
        if (wordStart != NPOS)
        {
            callback(str.Substr(wordStart, end - wordStart));
            wordStart = NPOS;
        }
    };

    for (SizeT i = 0; i < n; ++i)
    {
        CharType ch = str[i];

        if (ch == CharType('_') || ch == CharType('-') || ch == CharType(' ') || ch == CharType('.'))
        {
            FlushWord(i);
            continue;
        }

        if (wordStart == NPOS)
        {
            wordStart = i;
            continue;
        }

        CharType prev = str[i - 1];
        CharType next = (i + 1 < n) ? str[i + 1] : CharType(0);

        if (IsUpper(ch) && IsLower(prev))
        {
            FlushWord(i);
            wordStart = i;
            continue;
        }

        if (IsUpper(ch) && IsLower(next) && IsUpper(prev) && (i - wordStart) >= 1)
        {
            FlushWord(i);
            wordStart = i;
        }
    }

    FlushWord(n);
}

/// \brief Appends a word to dst applying a per-character transform.
/// \param dst          The string to append to.
/// \param word         The word view to append.
/// \param firstTransform Transform applied to the first character of the word.
/// \param restTransform  Transform applied to every subsequent character.
template <Concepts::IsCharacter CharType>
void AppendWord(
    Container::TBasicString<CharType>& dst,
    Container::TBasicStringView<CharType> word,
    CharType (*firstTransform)(CharType),
    CharType (*restTransform)(CharType)
)
{
    if (word.IsEmpty()) { return; }
    dst.PushBack(firstTransform(word[0]));
    for (SizeT i = 1; i < word.Size(); ++i) { dst.PushBack(restTransform(word[i])); }
}

}   // namespace Internal

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsAlpha(CharType ch) noexcept
{
    // Cast to UInt32 to avoid UB with negative signed chars and stay portable.
    UInt32 c = static_cast<UInt32>(static_cast<unsigned char>(ch));
    return (c | 0x20u) - UInt32('a') < 26u;
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsDigit(CharType ch) noexcept
{
    UInt32 c = static_cast<UInt32>(static_cast<unsigned char>(ch));
    return c - UInt32('0') < 10u;
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsAlNum(CharType ch) noexcept
{
    return IsAlpha(ch) || IsDigit(ch);
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsSpace(CharType ch) noexcept
{
    UInt32 c = static_cast<UInt32>(static_cast<unsigned char>(ch));
    // Space (0x20), tab (0x09), LF (0x0A), VT (0x0B), FF (0x0C), CR (0x0D)
    return c == UInt32(' ') || (c - UInt32('\t')) < 5u;
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsUpper(CharType ch) noexcept
{
    UInt32 c = static_cast<UInt32>(static_cast<unsigned char>(ch));
    return c - UInt32('A') < 26u;
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsLower(CharType ch) noexcept
{
    UInt32 c = static_cast<UInt32>(static_cast<unsigned char>(ch));
    return c - UInt32('a') < 26u;
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsHexDigit(CharType ch) noexcept
{
    UInt32 c = static_cast<UInt32>(static_cast<unsigned char>(ch));
    return IsDigit(ch) || (c | 0x20u) - UInt32('a') < 6u;
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE bool IsPrint(CharType ch) noexcept
{
    UInt32 c = static_cast<UInt32>(static_cast<unsigned char>(ch));
    return c - 0x20u < 0x5Fu;   // 0x20 (' ') through 0x7E ('~') inclusive
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE CharType ToUpper(CharType ch) noexcept
{
    return IsLower(ch) ? static_cast<CharType>(static_cast<unsigned char>(ch) - 0x20u) : ch;
}

template <Concepts::IsCharacter CharType>
GP_FORCEINLINE CharType ToLower(CharType ch) noexcept
{
    return IsUpper(ch) ? static_cast<CharType>(static_cast<unsigned char>(ch) + 0x20u) : ch;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToUpperStr(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result(str);
    for (SizeT i = 0; i < result.Size(); ++i) { result[i] = ToUpper(result[i]); }
    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToLowerStr(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result(str);
    for (SizeT i = 0; i < result.Size(); ++i) { result[i] = ToLower(result[i]); }
    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToTitleCase(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result(str);
    bool capitalizeNext = true;
    for (SizeT i = 0; i < result.Size(); ++i)
    {
        if (IsSpace(result[i])) { capitalizeNext = true; }
        else if (capitalizeNext)
        {
            result[i] = ToUpper(result[i]);
            capitalizeNext = false;
        }
        else { result[i] = ToLower(result[i]); }
    }
    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToCamelCase(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result;
    result.Reserve(str.Size());
    bool isFirst = true;

    Internal::CollectWords<CharType>(
        str,
        [&](Container::TBasicStringView<CharType> word)
        {
            if (isFirst)
            {
                // First word: all lowercase
                Internal::AppendWord(result, word, ToLower<CharType>, ToLower<CharType>);
                isFirst = false;
            }
            else
            {
                // Subsequent words: first char uppercase, rest lowercase
                Internal::AppendWord(result, word, ToUpper<CharType>, ToLower<CharType>);
            }
        }
    );

    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToPascalCase(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result;
    result.Reserve(str.Size());

    Internal::CollectWords<CharType>(
        str,
        [&](Container::TBasicStringView<CharType> word)
        { Internal::AppendWord(result, word, ToUpper<CharType>, ToLower<CharType>); }
    );

    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToSnakeCase(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result;
    result.Reserve(str.Size() + 4);
    bool isFirst = true;

    Internal::CollectWords<CharType>(
        str,
        [&](Container::TBasicStringView<CharType> word)
        {
            if (!isFirst) { result.PushBack(CharType('_')); }
            for (SizeT i = 0; i < word.Size(); ++i) { result.PushBack(ToLower(word[i])); }
            isFirst = false;
        }
    );

    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToKebabCase(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result;
    result.Reserve(str.Size() + 4);
    bool isFirst = true;

    Internal::CollectWords<CharType>(
        str,
        [&](Container::TBasicStringView<CharType> word)
        {
            if (!isFirst) { result.PushBack(CharType('-')); }
            for (SizeT i = 0; i < word.Size(); ++i) { result.PushBack(ToLower(word[i])); }
            isFirst = false;
        }
    );

    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ToScreamingSnakeCase(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result;
    result.Reserve(str.Size() + 4);
    bool isFirst = true;

    Internal::CollectWords<CharType>(
        str,
        [&](Container::TBasicStringView<CharType> word)
        {
            if (!isFirst) { result.PushBack(CharType('_')); }
            for (SizeT i = 0; i < word.Size(); ++i) { result.PushBack(ToUpper(word[i])); }
            isFirst = false;
        }
    );

    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicStringView<CharType> TrimLeftView(Container::TBasicStringView<CharType> str) noexcept
{
    SizeT start = 0;
    while (start < str.Size() && IsSpace(str[start])) { ++start; }
    return str.Substr(start);
}

template <Concepts::IsCharacter CharType>
Container::TBasicStringView<CharType> TrimRightView(Container::TBasicStringView<CharType> str) noexcept
{
    SizeT end = str.Size();
    while (end > 0 && IsSpace(str[end - 1])) { --end; }
    return str.Substr(0, end);
}

template <Concepts::IsCharacter CharType>
Container::TBasicStringView<CharType> TrimView(Container::TBasicStringView<CharType> str) noexcept
{
    return TrimRightView(TrimLeftView(str));
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> TrimLeft(Container::TBasicStringView<CharType> str)
{
    return Container::TBasicString<CharType>(TrimLeftView(str));
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> TrimRight(Container::TBasicStringView<CharType> str)
{
    return Container::TBasicString<CharType>(TrimRightView(str));
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> Trim(Container::TBasicStringView<CharType> str)
{
    return Container::TBasicString<CharType>(TrimView(str));
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType>
    PadLeft(Container::TBasicStringView<CharType> str, SizeT totalWidth, CharType fillChar)
{
    if (str.Size() >= totalWidth) { return Container::TBasicString<CharType>(str); }
    SizeT padCount = totalWidth - str.Size();
    Container::TBasicString<CharType> result;
    result.Reserve(totalWidth);
    result.Append(padCount, fillChar);
    result.Append(str);
    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType>
    PadRight(Container::TBasicStringView<CharType> str, SizeT totalWidth, CharType fillChar)
{
    if (str.Size() >= totalWidth) { return Container::TBasicString<CharType>(str); }
    SizeT padCount = totalWidth - str.Size();
    Container::TBasicString<CharType> result(str);
    result.Append(padCount, fillChar);
    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType>
    PadCenter(Container::TBasicStringView<CharType> str, SizeT totalWidth, CharType fillChar)
{
    if (str.Size() >= totalWidth) { return Container::TBasicString<CharType>(str); }
    SizeT padTotal = totalWidth - str.Size();
    SizeT leftPad = padTotal / 2;
    SizeT rightPad = padTotal - leftPad;
    Container::TBasicString<CharType> result;
    result.Reserve(totalWidth);
    result.Append(leftPad, fillChar);
    result.Append(str);
    result.Append(rightPad, fillChar);
    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> Repeat(Container::TBasicStringView<CharType> str, SizeT count)
{
    if (count == 0 || str.IsEmpty()) { return Container::TBasicString<CharType>(); }
    Container::TBasicString<CharType> result;
    result.Reserve(str.Size() * count);
    for (SizeT i = 0; i < count; ++i) { result.Append(str); }
    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> ReplaceAll(
    Container::TBasicStringView<CharType> str,
    Container::TBasicStringView<CharType> from,
    Container::TBasicStringView<CharType> to
)
{
    if (from.IsEmpty()) { return Container::TBasicString<CharType>(str); }

    Container::TBasicString<CharType> result;
    result.Reserve(str.Size());

    SizeT pos = 0;
    SizeT fLen = from.Size();

    while (pos < str.Size())
    {
        SizeT found = str.Find(from, pos);
        if (found == Container::TBasicStringView<CharType>::NPOS)
        {
            result.Append(str.Substr(pos));
            break;
        }
        result.Append(str.Substr(pos, found - pos));
        result.Append(to);
        pos = found + fLen;
    }

    return result;
}

template <Concepts::IsCharacter CharType>
SizeT CountOccurrences(Container::TBasicStringView<CharType> str, Container::TBasicStringView<CharType> sub) noexcept
{
    if (sub.IsEmpty()) { return 0; }
    SizeT count = 0;
    SizeT pos = 0;
    while (true)
    {
        pos = str.Find(sub, pos);
        if (pos == Container::TBasicStringView<CharType>::NPOS) { break; }
        ++count;
        pos += sub.Size();
    }
    return count;
}

template <Concepts::IsCharacter CharType>
SizeT CountOccurrences(Container::TBasicStringView<CharType> str, CharType ch) noexcept
{
    SizeT count = 0;
    for (SizeT i = 0; i < str.Size(); ++i)
    {
        if (str[i] == ch) { ++count; }
    }
    return count;
}

template <Concepts::IsCharacter CharType>
Container::TBasicStringView<CharType>
    StripPrefix(Container::TBasicStringView<CharType> str, Container::TBasicStringView<CharType> prefix) noexcept
{
    if (str.StartsWith(prefix)) { return str.Substr(prefix.Size()); }
    return str;
}

template <Concepts::IsCharacter CharType>
Container::TBasicStringView<CharType>
    StripSuffix(Container::TBasicStringView<CharType> str, Container::TBasicStringView<CharType> suffix) noexcept
{
    if (str.EndsWith(suffix)) { return str.Substr(0, str.Size() - suffix.Size()); }
    return str;
}

template <Concepts::IsCharacter CharType, typename OutputIt>
void Split(Container::TBasicStringView<CharType> str, CharType delimiter, OutputIt out, SizeT maxParts)
{
    constexpr SizeT NPOS = Container::TBasicStringView<CharType>::NPOS;

    SizeT partCount = 0;
    SizeT start = 0;

    for (SizeT i = 0; i <= str.Size(); ++i)
    {
        bool atDelimiter = (i < str.Size() && str[i] == delimiter);
        bool atEnd = (i == str.Size());

        if (atDelimiter || atEnd)
        {
            if (maxParts != NPOS && partCount + 1 >= maxParts)
            {
                // Last part: emit the remainder of the string
                *out++ = str.Substr(start);
                return;
            }
            *out++ = str.Substr(start, i - start);
            ++partCount;
            start = i + 1;
        }
    }
}

template <Concepts::IsCharacter CharType, typename OutputIt>
void Split(
    Container::TBasicStringView<CharType> str,
    Container::TBasicStringView<CharType> delimiter,
    OutputIt out,
    SizeT maxParts
)
{
    constexpr SizeT NPOS = Container::TBasicStringView<CharType>::NPOS;

    if (delimiter.IsEmpty())
    {
        *out++ = str;
        return;
    }

    SizeT partCount = 0;
    SizeT start = 0;
    SizeT dLen = delimiter.Size();

    while (true)
    {
        SizeT found = str.Find(delimiter, start);

        if (found == NPOS)
        {
            *out++ = str.Substr(start);
            return;
        }

        if (maxParts != NPOS && partCount + 1 >= maxParts)
        {
            *out++ = str.Substr(start);
            return;
        }

        *out++ = str.Substr(start, found - start);
        ++partCount;
        start = found + dLen;
    }
}

template <Concepts::IsCharacter CharType, typename InputIt>
Container::TBasicString<CharType> Join(InputIt first, InputIt last, Container::TBasicStringView<CharType> separator)
{
    Container::TBasicString<CharType> result;
    if (first == last) { return result; }

    // Pre-compute total length to avoid reallocations
    SizeT totalLen = 0;
    SizeT count = 0;
    for (InputIt it = first; it != last; ++it, ++count)
    {
        Container::TBasicStringView<CharType> sv(*it);
        totalLen += sv.Size();
    }
    if (count > 1) { totalLen += separator.Size() * (count - 1); }
    result.Reserve(totalLen);

    bool isFirst = true;
    for (InputIt it = first; it != last; ++it)
    {
        if (!isFirst) { result.Append(separator); }
        result.Append(Container::TBasicStringView<CharType>(*it));
        isFirst = false;
    }

    return result;
}

template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType> Reverse(Container::TBasicStringView<CharType> str)
{
    Container::TBasicString<CharType> result(str);
    SizeT n = result.Size();
    for (SizeT i = 0; i < n / 2; ++i) { std::swap(result[i], result[n - 1 - i]); }
    return result;
}

template <Concepts::IsCharacter CharType>
bool IsAllAlpha(Container::TBasicStringView<CharType> str) noexcept
{
    if (str.IsEmpty()) { return false; }
    for (SizeT i = 0; i < str.Size(); ++i)
    {
        if (!IsAlpha(str[i])) { return false; }
    }
    return true;
}

template <Concepts::IsCharacter CharType>
bool IsAllDigits(Container::TBasicStringView<CharType> str) noexcept
{
    if (str.IsEmpty()) { return false; }
    for (SizeT i = 0; i < str.Size(); ++i)
    {
        if (!IsDigit(str[i])) { return false; }
    }
    return true;
}

template <Concepts::IsCharacter CharType>
bool IsAllAlNum(Container::TBasicStringView<CharType> str) noexcept
{
    if (str.IsEmpty()) { return false; }
    for (SizeT i = 0; i < str.Size(); ++i)
    {
        if (!IsAlNum(str[i])) { return false; }
    }
    return true;
}

template <Concepts::IsCharacter CharType>
bool IsAllWhitespace(Container::TBasicStringView<CharType> str) noexcept
{
    for (SizeT i = 0; i < str.Size(); ++i)
    {
        if (!IsSpace(str[i])) { return false; }
    }
    return true;   // Empty strings are considered "all whitespace"
}

template <Concepts::IsCharacter CharType>
bool IsAllUpper(Container::TBasicStringView<CharType> str) noexcept
{
    if (str.IsEmpty()) { return false; }
    for (SizeT i = 0; i < str.Size(); ++i)
    {
        if (IsAlpha(str[i]) && !IsUpper(str[i])) { return false; }
    }
    return true;
}

template <Concepts::IsCharacter CharType>
bool IsAllLower(Container::TBasicStringView<CharType> str) noexcept
{
    if (str.IsEmpty()) { return false; }
    for (SizeT i = 0; i < str.Size(); ++i)
    {
        if (IsAlpha(str[i]) && !IsLower(str[i])) { return false; }
    }
    return true;
}

}   // namespace GP::Utilities
