// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Utilities/StringFormat.hpp"
#include "Utilities/StringUtilities.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

namespace GP::Utilities
{

namespace Internal
{

/// \brief Applies the width/alignment from spec to the already-formatted content string,
/// appending the result to out.
///
/// For string-like values, default alignment is Left. For numeric values the caller should
/// pass Alignment::Right as defaultAlign.
///
/// \warning This function does NOT handle zero-pad (that requires sign-aware logic). Call AppendNumericWithPad for
/// numeric types.
template <Concepts::IsCharacter CharType>
void ApplyAlignment(
    Container::TBasicString<CharType>& out,
    Container::TBasicStringView<CharType> content,
    const TFormatSpec<CharType>& spec,
    typename TFormatSpec<CharType>::Alignment defaultAlign
)
{
    using Alignment = typename TFormatSpec<CharType>::Alignment;

    Alignment align = spec.align;
    if (align == Alignment::Default) { align = defaultAlign; }

    if (spec.width <= 0 || static_cast<SizeT>(spec.width) <= content.Size())
    {
        out.Append(content);
        return;
    }

    SizeT padTotal = static_cast<SizeT>(spec.width) - content.Size();
    SizeT leftPad = 0;
    SizeT rightPad = 0;

    switch (align)
    {
    case Alignment::Left   : rightPad = padTotal; break;
    case Alignment::Right  :
    case Alignment::Default: leftPad = padTotal; break;
    case Alignment::Center:
        leftPad = padTotal / 2;
        rightPad = padTotal - leftPad;
        break;
    }

    out.Append(leftPad, spec.fillChar);
    out.Append(content);
    out.Append(rightPad, spec.fillChar);
}

/// \brief Converts a non-negative integer value to string in the given base, writing digits into
/// buf (in reverse order). Returns the number of digits written.
/// \param value The non-negative integer value.
/// \param base The numeric base (2, 8, 10, or 16).
/// \param uppercase Whether to use uppercase hex digits.
/// \param buf Output buffer of at least 66 chars.
/// \return Number of characters written into buf (written in reverse: buf[0] is least significant).
GP_INLINE Int32 IntToReversedDigits(UInt64 value, Int32 base, bool uppercase, char* buf) noexcept
{
    static constexpr char Lower[] = "0123456789abcdef";
    static constexpr char Upper[] = "0123456789ABCDEF";
    const char* Digits = uppercase ? Upper : Lower;

    if (value == 0)
    {
        buf[0] = '0';
        return 1;
    }

    Int32 len = 0;
    while (value > 0)
    {
        buf[len++] = Digits[value % static_cast<UInt64>(base)];
        value /= static_cast<UInt64>(base);
    }
    return len;
}

/// \brief Appends a formatted integer to out, applying width, alignment, sign, alt-form, and
/// zero-padding from spec.
/// \param out       Output string.
/// \param value     The absolute value of the integer (always non-negative).
/// \param isNeg     Whether the original value was negative.
/// \param spec      The parsed format specification.
template <Concepts::IsCharacter CharType>
void AppendInteger(Container::TBasicString<CharType>& out, UInt64 value, bool isNeg, const TFormatSpec<CharType>& spec)
{
    using Presentation = typename TFormatSpec<CharType>::Presentation;
    using Alignment = typename TFormatSpec<CharType>::Alignment;
    using Sign = typename TFormatSpec<CharType>::Sign;

    // Determine base and uppercase flag from presentation type.
    Int32 base = 10;
    bool uppercase = false;
    switch (spec.type)
    {
    case Presentation::Binary:
    case Presentation::BinaryUpper:
        base = 2;
        uppercase = (spec.type == Presentation::BinaryUpper);
        break;
    case Presentation::Octal: base = 8; break;
    case Presentation::HexLower:
        base = 16;
        uppercase = false;
        break;
    case Presentation::HexUpper:
        base = 16;
        uppercase = true;
        break;
    default: base = 10; break;
    }

    // Build reversed digit string.
    char digitsBuf[66];
    Int32 digitsLen = IntToReversedDigits(value, base, uppercase, digitsBuf);

    // Build sign/prefix string.
    char prefix[4] = {};
    Int32 prefixLen = 0;

    if (isNeg) { prefix[prefixLen++] = '-'; }
    else
    {
        switch (spec.sign)
        {
        case Sign::Plus : prefix[prefixLen++] = '+'; break;
        case Sign::Space: prefix[prefixLen++] = ' '; break;
        default         : break;
        }
    }

    if (spec.altForm)
    {
        if (base == 16)
        {
            prefix[prefixLen++] = '0';
            prefix[prefixLen++] = uppercase ? 'X' : 'x';
        }
        else if (base == 8 && !(digitsLen == 1 && digitsBuf[0] == '0')) { prefix[prefixLen++] = '0'; }
        else if (base == 2)
        {
            prefix[prefixLen++] = '0';
            prefix[prefixLen++] = uppercase ? 'B' : 'b';
        }
    }

    SizeT totalContent = static_cast<SizeT>(prefixLen + digitsLen);
    SizeT fieldWidth = (spec.width > 0) ? static_cast<SizeT>(spec.width) : 0;

    Alignment align = spec.align;
    if (align == Alignment::Default) { align = Alignment::Right; }

    // Zero-pad: sign/prefix first, then zeros, then digits (left-to-right in field).
    if (spec.zeroPad && fieldWidth > totalContent && align == Alignment::Right)
    {
        SizeT zeroPadCount = fieldWidth - totalContent;
        for (Int32 k = 0; k < prefixLen; ++k) { out.PushBack(static_cast<CharType>(prefix[k])); }
        out.Append(zeroPadCount, CharType('0'));
        for (Int32 k = digitsLen - 1; k >= 0; --k) { out.PushBack(static_cast<CharType>(digitsBuf[k])); }
        return;
    }

    // Normal padding: build full token, then apply alignment.
    Container::TBasicString<CharType> token;
    token.Reserve(totalContent);
    for (Int32 k = 0; k < prefixLen; ++k) { token.PushBack(static_cast<CharType>(prefix[k])); }
    for (Int32 k = digitsLen - 1; k >= 0; --k) { token.PushBack(static_cast<CharType>(digitsBuf[k])); }

    ApplyAlignment(out, Container::TBasicStringView<CharType>(token), spec, Alignment::Right);
}

/// \brief Appends a formatted floating-point value to out.
/// Uses snprintf internally for locale-independent, portable conversion.
/// \param out Output string.
/// \param value The double-precision value to format.
/// \param spec The parsed format specification.
template <Concepts::IsCharacter CharType>
void AppendFloat(Container::TBasicString<CharType>& out, Float64 value, const TFormatSpec<CharType>& spec)
{
    using Presentation = typename TFormatSpec<CharType>::Presentation;
    using Alignment = typename TFormatSpec<CharType>::Alignment;
    using Sign = typename TFormatSpec<CharType>::Sign;

    // Build a printf-style format specifier.
    char fmt[16];
    Int32 fi = 0;
    fmt[fi++] = '%';

    // Sign
    switch (spec.sign)
    {
    case Sign::Plus : fmt[fi++] = '+'; break;
    case Sign::Space: fmt[fi++] = ' '; break;
    default         : break;
    }

    // Alternate form ('#') keeps decimal point even without fractional digits.
    if (spec.altForm) { fmt[fi++] = '#'; }

    // Precision
    if (spec.precision >= 0)
    {
        fmt[fi++] = '.';
        // Append precision digits to the fmt string manually.
        if (spec.precision == 0) { fmt[fi++] = '0'; }
        else
        {
            Int32 p = spec.precision;
            char tmp[12];
            Int32 tl = 0;
            while (p > 0)
            {
                tmp[tl++] = static_cast<char>('0' + p % 10);
                p /= 10;
            }
            for (Int32 k = tl - 1; k >= 0; --k) { fmt[fi++] = tmp[k]; }
        }
    }

    // Type character
    switch (spec.type)
    {
    case Presentation::FloatFixed       : fmt[fi++] = 'f'; break;
    case Presentation::FloatFixedUpper  : fmt[fi++] = 'F'; break;
    case Presentation::FloatSci         : fmt[fi++] = 'e'; break;
    case Presentation::FloatSciUpper    : fmt[fi++] = 'E'; break;
    case Presentation::FloatGeneralUpper: fmt[fi++] = 'G'; break;
    default                             : fmt[fi++] = 'g'; break;
    }
    fmt[fi] = '\0';

    // snprintf into a local buffer; 64 bytes covers all normal float representations.
    // For extreme precision requests, fall back to a heap buffer.
    char smallBuf[128];
    Int32 needed = std::snprintf(smallBuf, sizeof(smallBuf), fmt, value);

    Container::TBasicStringView<CharType> view;
    Container::TBasicString<CharType> heapBuf;

    if (needed < static_cast<Int32>(sizeof(smallBuf)))
    {
        // Widen char -> CharType if necessary.
        if constexpr (std::is_same_v<CharType, char>)
        {
            view = Container::TBasicStringView<CharType>(smallBuf, static_cast<SizeT>(needed));
        }
        else
        {
            heapBuf.Reserve(static_cast<SizeT>(needed));
            for (Int32 k = 0; k < needed; ++k) { heapBuf.PushBack(static_cast<CharType>(smallBuf[k])); }
            view = Container::TBasicStringView<CharType>(heapBuf);
        }
    }
    else
    {
        // Edge case: extremely long precision string.
        heapBuf.Resize(static_cast<SizeT>(needed + 1), CharType(0));
        char* rawChar = nullptr;
        if constexpr (std::is_same_v<CharType, char>) { rawChar = heapBuf.Data(); }
        else
        {
            // Build into a temporary char array then widen.
            char* tmp = new char[static_cast<SizeT>(needed + 1)];
            std::snprintf(tmp, static_cast<SizeT>(needed + 1), fmt, value);
            heapBuf.Clear();
            heapBuf.Reserve(static_cast<SizeT>(needed));
            for (Int32 k = 0; k < needed; ++k) { heapBuf.PushBack(static_cast<CharType>(tmp[k])); }
            delete[] tmp;
        }
        if constexpr (std::is_same_v<CharType, char>)
        {
            std::snprintf(rawChar, static_cast<SizeT>(needed + 1), fmt, value);
            heapBuf.Resize(static_cast<SizeT>(needed));
        }
        view = Container::TBasicStringView<CharType>(heapBuf);
    }

    ApplyAlignment(out, view, spec, Alignment::Right);
}

/// \brief Applies precision truncation (max chars) and alignment for string values.
template <Concepts::IsCharacter CharType>
void AppendString(
    Container::TBasicString<CharType>& out,
    Container::TBasicStringView<CharType> value,
    const TFormatSpec<CharType>& spec
)
{
    using Alignment = typename TFormatSpec<CharType>::Alignment;

    Container::TBasicStringView<CharType> sv = value;
    if (spec.precision >= 0 && static_cast<SizeT>(spec.precision) < sv.Size())
    {
        sv = sv.Substr(0, static_cast<SizeT>(spec.precision));
    }
    ApplyAlignment(out, sv, spec, Alignment::Left);
}

}   // namespace Internal

template <Concepts::IsCharacter CharType>
TFormatSpec<CharType> ParseFormatSpec(Container::TBasicStringView<CharType> spec) noexcept
{
    using Alignment = typename TFormatSpec<CharType>::Alignment;
    using Sign = typename TFormatSpec<CharType>::Sign;
    using Presentation = typename TFormatSpec<CharType>::Presentation;

    TFormatSpec<CharType> result;
    SizeT i = 0;
    SizeT n = spec.Size();

    if (n == 0) { return result; }

    // -- Fill + Align: if the second character is an align symbol, first char is fill.
    if (i + 1 < n)
    {
        CharType second = spec[i + 1];
        if (second == CharType('<') || second == CharType('>') || second == CharType('^'))
        {
            result.fillChar = spec[i];
            ++i;
        }
    }

    // -- Align (may appear without an explicit fill character).
    if (i < n && (spec[i] == CharType('<') || spec[i] == CharType('>') || spec[i] == CharType('^')))
    {
        if (spec[i] == CharType('<')) { result.align = Alignment::Left; }
        else if (spec[i] == CharType('>')) { result.align = Alignment::Right; }
        else { result.align = Alignment::Center; }
        ++i;
    }

    // -- Sign
    if (i < n)
    {
        if (spec[i] == CharType('+'))
        {
            result.sign = Sign::Plus;
            ++i;
        }
        else if (spec[i] == CharType('-'))
        {
            result.sign = Sign::Minus;
            ++i;
        }
        else if (spec[i] == CharType(' '))
        {
            result.sign = Sign::Space;
            ++i;
        }
    }

    // -- Alternate form '#'
    if (i < n && spec[i] == CharType('#'))
    {
        result.altForm = true;
        ++i;
    }

    // -- Zero-pad '0' (only meaningful before width digits)
    if (i < n && spec[i] == CharType('0'))
    {
        result.zeroPad = true;
        // Only assign the fill character and default alignment when no explicit
        // fill+align was already parsed. If align != Default, a fill character
        // was already provided and must not be overwritten by the zero-pad flag.
        if (result.align == Alignment::Default)
        {
            result.fillChar = CharType('0');
            result.align = Alignment::Right;
        }
        ++i;
    }

    // -- Width (decimal digits)
    while (i < n && spec[i] >= CharType('0') && spec[i] <= CharType('9'))
    {
        Int32 digit = static_cast<Int32>(spec[i] - CharType('0'));
        result.width = (result.width < 0 ? 0 : result.width) * 10 + digit;
        ++i;
    }

    // -- Precision ('.' followed by decimal digits)
    if (i < n && spec[i] == CharType('.'))
    {
        ++i;
        result.precision = 0;
        while (i < n && spec[i] >= CharType('0') && spec[i] <= CharType('9'))
        {
            result.precision = result.precision * 10 + static_cast<Int32>(spec[i] - CharType('0'));
            ++i;
        }
    }

    // -- Presentation type character
    if (i < n)
    {
        switch (spec[i])
        {
        case CharType('b'): result.type = Presentation::Binary; break;
        case CharType('B'): result.type = Presentation::BinaryUpper; break;
        case CharType('o'): result.type = Presentation::Octal; break;
        case CharType('d'): result.type = Presentation::Decimal; break;
        case CharType('x'): result.type = Presentation::HexLower; break;
        case CharType('X'): result.type = Presentation::HexUpper; break;
        case CharType('f'): result.type = Presentation::FloatFixed; break;
        case CharType('F'): result.type = Presentation::FloatFixedUpper; break;
        case CharType('e'): result.type = Presentation::FloatSci; break;
        case CharType('E'): result.type = Presentation::FloatSciUpper; break;
        case CharType('g'): result.type = Presentation::FloatGeneral; break;
        case CharType('G'): result.type = Presentation::FloatGeneralUpper; break;
        case CharType('s'): result.type = Presentation::String; break;
        case CharType('c'): result.type = Presentation::Char; break;
        case CharType('p'): result.type = Presentation::Pointer; break;
        default           : break;
        }
    }

    return result;
}

template <Concepts::IsCharacter CharType>
template <typename T>
TFormatArg<CharType> TFormatArg<CharType>::Make(const T& value) noexcept
{
    using CleanT = std::decay_t<T>;

    TFormatArg<CharType> arg;
    arg.m_value = &value;
    arg.m_formatFn = [](Container::TBasicString<CharType>& out, const void* val, const TFormatSpec<CharType>& spec)
    {
        if constexpr (std::is_array_v<T>)
        {
            using ElemT = std::add_const_t<std::remove_extent_t<T>>;
            TFormatter<CharType, ElemT*>::Format(out, static_cast<ElemT*>(val), spec);
        }
        else { TFormatter<CharType, CleanT>::Format(out, *static_cast<const CleanT*>(val), spec); }
    };
    return arg;
}

template <Concepts::IsCharacter CharType>
void TFormatArg<CharType>::Format(Container::TBasicString<CharType>& out, const TFormatSpec<CharType>& spec) const
{
    GP_ASSERT(m_formatFn != nullptr, "TFormatArg: null format function (was Make() called?)");
    m_formatFn(out, m_value, spec);
}

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, bool>
{
    static void Format(Container::TBasicString<CharType>& out, bool value, const TFormatSpec<CharType>& spec)
    {
        using Presentation = typename TFormatSpec<CharType>::Presentation;

        if (spec.type == Presentation::Decimal || spec.type == Presentation::Binary ||
            spec.type == Presentation::Octal || spec.type == Presentation::HexLower ||
            spec.type == Presentation::HexUpper)
        {
            // Numeric presentation: format as 0 or 1
            TFormatter<CharType, UInt64>::Format(out, static_cast<UInt64>(value ? 1 : 0), spec);
        }
        else
        {
            static constexpr char TrueStr[] = "true";
            static constexpr char FalseStr[] = "false";
            const char* src = value ? TrueStr : FalseStr;
            Container::TBasicString<CharType> token;
            for (const char* p = src; *p; ++p) { token.PushBack(static_cast<CharType>(*p)); }
            Internal::AppendString(out, Container::TBasicStringView<CharType>(token), spec);
        }
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, char>
{
    static void Format(Container::TBasicString<CharType>& out, char value, const TFormatSpec<CharType>& spec)
    {
        using Presentation = typename TFormatSpec<CharType>::Presentation;
        using Alignment = typename TFormatSpec<CharType>::Alignment;

        if (spec.type == Presentation::Decimal || spec.type == Presentation::HexLower ||
            spec.type == Presentation::HexUpper || spec.type == Presentation::Binary ||
            spec.type == Presentation::Octal)
        {
            // Numeric presentation of the code point
            TFormatter<CharType, UInt64>::Format(out, static_cast<UInt64>(static_cast<unsigned char>(value)), spec);
        }
        else
        {
            CharType ch = static_cast<CharType>(value);
            Container::TBasicStringView<CharType> sv(&ch, 1);
            Internal::ApplyAlignment(out, sv, spec, Alignment::Left);
        }
    }
};

template <Concepts::IsCharacter CharType>
requires(!std::is_same_v<CharType, char>) struct TFormatter<CharType, CharType>
{
    static void Format(Container::TBasicString<CharType>& out, CharType value, const TFormatSpec<CharType>& spec)
    {
        using Alignment = typename TFormatSpec<CharType>::Alignment;

        Container::TBasicStringView<CharType> sv(&value, 1);
        Internal::ApplyAlignment(out, sv, spec, Alignment::Left);
    }
};

template <Concepts::IsCharacter CharType, typename IntT>
requires(std::is_integral_v<IntT> && std::is_signed_v<IntT> && !std::is_same_v<IntT, bool>)
struct TFormatter<CharType, IntT>
{
    static void Format(Container::TBasicString<CharType>& out, IntT value, const TFormatSpec<CharType>& spec)
    {
        using Presentation = typename TFormatSpec<CharType>::Presentation;

        if (spec.type == Presentation::Char)
        {
            TFormatter<CharType, char>::Format(out, static_cast<char>(value), spec);
            return;
        }

        bool isNeg = (value < 0);
        UInt64 abs = isNeg ? (value == std::numeric_limits<IntT>::min()
                                  ? static_cast<UInt64>(std::numeric_limits<IntT>::max()) + 1ULL
                                  : static_cast<UInt64>(-value))
                           : static_cast<UInt64>(value);
        Internal::AppendInteger(out, abs, isNeg, spec);
    }
};

template <Concepts::IsCharacter CharType, typename UIntT>
requires(std::is_integral_v<UIntT> && std::is_unsigned_v<UIntT> && !std::is_same_v<UIntT, bool>)
struct TFormatter<CharType, UIntT>
{
    static void Format(Container::TBasicString<CharType>& out, UIntT value, const TFormatSpec<CharType>& spec)
    {
        using Presentation = typename TFormatSpec<CharType>::Presentation;

        if (spec.type == Presentation::Char)
        {
            TFormatter<CharType, char>::Format(out, static_cast<char>(value), spec);
            return;
        }

        Internal::AppendInteger(out, static_cast<UInt64>(value), false, spec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, Float32>
{
    static void Format(Container::TBasicString<CharType>& out, Float32 value, const TFormatSpec<CharType>& spec)
    {
        Internal::AppendFloat(out, static_cast<Float64>(value), spec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, Float64>
{
    static void Format(Container::TBasicString<CharType>& out, Float64 value, const TFormatSpec<CharType>& spec)
    {
        Internal::AppendFloat(out, value, spec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, const CharType*>
{
    static void Format(Container::TBasicString<CharType>& out, const CharType* value, const TFormatSpec<CharType>& spec)
    {
        if (value == nullptr)
        {
            static constexpr char NullStr[] = "(null)";
            Container::TBasicString<CharType> token;
            for (const char* p = NullStr; *p; ++p) { token.PushBack(static_cast<CharType>(*p)); }
            Internal::AppendString(out, Container::TBasicStringView<CharType>(token), spec);
        }
        else { Internal::AppendString(out, Container::TBasicStringView<CharType>(value), spec); }
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, CharType*>
{
    static void Format(Container::TBasicString<CharType>& out, CharType* value, const TFormatSpec<CharType>& spec)
    {
        TFormatter<CharType, const CharType*>::Format(out, value, spec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, Container::TBasicStringView<CharType>>
{
    static void Format(
        Container::TBasicString<CharType>& out,
        Container::TBasicStringView<CharType> value,
        const TFormatSpec<CharType>& spec
    )
    {
        Internal::AppendString(out, value, spec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, Container::TBasicString<CharType>>
{
    static void Format(
        Container::TBasicString<CharType>& out,
        const Container::TBasicString<CharType>& value,
        const TFormatSpec<CharType>& spec
    )
    {
        Internal::AppendString(out, Container::TBasicStringView<CharType>(value), spec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, void*>
{
    static void Format(Container::TBasicString<CharType>& out, const void* value, const TFormatSpec<CharType>& spec)
    {
        TFormatSpec<CharType> ptrSpec = spec;
        ptrSpec.type = TFormatSpec<CharType>::Presentation::HexLower;
        ptrSpec.altForm = true;
        // Zero-pad to native pointer width (16 hex digits on 64-bit, 8 on 32-bit).
        if (ptrSpec.width < 0) { ptrSpec.width = static_cast<Int32>(sizeof(void*) * 2 + 2); }
        if (!ptrSpec.zeroPad)
        {
            ptrSpec.zeroPad = true;
            ptrSpec.fillChar = CharType('0');
        }
        Internal::AppendInteger(out, static_cast<UInt64>(reinterpret_cast<UIntPtr>(value)), false, ptrSpec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, const void*> : TFormatter<CharType, void*>
{
    static void Format(Container::TBasicString<CharType>& out, const void* value, const TFormatSpec<CharType>& spec)
    {
        TFormatter<CharType, void*>::Format(out, value, spec);
    }
};

template <Concepts::IsCharacter CharType>
struct TFormatter<CharType, NullPtr>
{
    static void Format(Container::TBasicString<CharType>& out, NullPtr, const TFormatSpec<CharType>& spec)
    {
        TFormatter<CharType, void*>::Format(out, nullptr, spec);
    }
};

namespace Internal
{

/// \brief Core implementation: iterates fmt, emits literals, and dispatches argument placeholders.
/// \param fmt The format string.
/// \param args Array of type-erased arguments.
/// \param argCount Number of elements in args.
template <Concepts::IsCharacter CharType>
Container::TBasicString<CharType>
    FormatImpl(Container::TBasicStringView<CharType> fmt, const TFormatArg<CharType>* args, SizeT argCount)
{
    Container::TBasicString<CharType> result;
    result.Reserve(fmt.Size() + argCount * 8);   // Heuristic pre-size

    SizeT seqArgIndex = 0;                       // Next sequential argument index
    SizeT n = fmt.Size();
    SizeT literalStart = 0;

    for (SizeT i = 0; i < n;)
    {
        CharType ch = fmt[i];

        if (ch == CharType('{') && i + 1 < n && fmt[i + 1] == CharType('{'))
        {
            result.Append(fmt.Substr(literalStart, i - literalStart));
            result.PushBack(CharType('{'));
            i += 2;
            literalStart = i;
            continue;
        }
        if (ch == CharType('}') && i + 1 < n && fmt[i + 1] == CharType('}'))
        {
            result.Append(fmt.Substr(literalStart, i - literalStart));
            result.PushBack(CharType('}'));
            i += 2;
            literalStart = i;
            continue;
        }

        if (ch == CharType('{'))
        {
            // Flush preceding literal content.
            result.Append(fmt.Substr(literalStart, i - literalStart));
            ++i;

            // Scan to the matching '}'
            SizeT placeholderStart = i;
            while (i < n && fmt[i] != CharType('}')) { ++i; }
            GP_ASSERT(i < n, "StringFormat::Format: unterminated '{' in format string");

            Container::TBasicStringView<CharType> placeholder = fmt.Substr(placeholderStart, i - placeholderStart);
            ++i;   // Consume '}'
            literalStart = i;

            SizeT colonPos = placeholder.Find(CharType(':'));
            Container::TBasicStringView<CharType> argRef;
            Container::TBasicStringView<CharType> specStr;

            if (colonPos != Container::TBasicStringView<CharType>::NPOS)
            {
                argRef = placeholder.Substr(0, colonPos);
                specStr = placeholder.Substr(colonPos + 1);
            }
            else
            {
                argRef = placeholder;
                specStr = Container::TBasicStringView<CharType>();
            }

            SizeT argIdx = seqArgIndex;
            bool isPositional = false;

            if (!argRef.IsEmpty())
            {
                argIdx = 0;
                isPositional = true;
                for (SizeT k = 0; k < argRef.Size(); ++k)
                {
                    GP_ASSERT(
                        argRef[k] >= CharType('0') && argRef[k] <= CharType('9'),
                        "StringFormat::Format: non-numeric argument index"
                    );
                    argIdx = argIdx * 10 + static_cast<SizeT>(argRef[k] - CharType('0'));
                }
            }

            if (!isPositional) { ++seqArgIndex; }

            GP_ASSERT(argIdx < argCount, "StringFormat::Format: argument index out of range");

            TFormatSpec<CharType> spec = ParseFormatSpec(specStr);
            args[argIdx].Format(result, spec);

            continue;
        }

        ++i;
    }

    // Append any remaining literal tail.
    if (literalStart < n) { result.Append(fmt.Substr(literalStart)); }

    return result;
}

}   // namespace Internal

template <Concepts::IsCharacter CharType, typename... Args>
Container::TBasicString<CharType> Format(Container::TBasicStringView<CharType> fmt, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0) { return Internal::FormatImpl<CharType>(fmt, nullptr, 0); }
    else
    {
        const TFormatArg<CharType> argArray[] = {
            TFormatArg<CharType>::Make(static_cast<const std::remove_reference_t<Args>&>(args))...
        };
        return Internal::FormatImpl<CharType>(fmt, argArray, sizeof...(Args));
    }
}

template <Concepts::IsCharacter CharType, typename... Args>
Container::TBasicString<CharType> Format(const CharType* fmt, Args&&... args)
{
    return Format<CharType>(Container::TBasicStringView<CharType>(fmt), std::forward<Args>(args)...);
}

}   // namespace GP::Utilities
