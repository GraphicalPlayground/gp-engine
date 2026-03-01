// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicString.hpp"
#include "Container/BasicStringView.hpp"
#include "CoreBuild.hpp"
#include <concepts>
#include <cstdio>
#include <type_traits>

namespace GP::Utilities
{

/// \brief Parsed representation of a format spec drawn from a `{:spec}` placeholder.
///
/// Grammar (identical to Python / fmtlib):
/// \code
///   spec      := [[fill]align][sign]['#']['0'][width]['.' precision][type]
///   fill      := any character (only meaningful when align is also present)
///   align     := '<' | '>' | '^'
///   sign      := '+' | '-' | ' '
///   width     := decimal-integer
///   precision := '.' decimal-integer
///   type      := 'b'|'B'|'o'|'d'|'x'|'X'|'e'|'E'|'f'|'F'|'g'|'G'|'s'|'c'|'p'
/// \endcode
///
/// \tparam CharType The character type of the format string.
template <Concepts::IsCharacter CharType>
struct TFormatSpec
{
    /// \brief Horizontal alignment for the formatted value within the padded field.
    enum class Alignment : UInt8
    {
        Default,   //<! Left for strings, right for numbers.
        Left,      //<! '<' — content flush-left, fill on right.
        Right,     //<! '>' — content flush-right, fill on left.
        Center,    //<! '^' — content centred, fill split left and right.
    };

    /// \brief Sign convention for numeric values.
    enum class Sign : UInt8
    {
        Default,   //<! Same as Minus: only show '-' for negative values.
        Plus,      //<! '+' — always show a sign character.
        Minus,     //<! '-' — only show '-' for negative values (default).
        Space,     //<! ' ' — space for positive, '-' for negative.
    };

    /// \brief Specifies how the value should be rendered.
    enum class Presentation : UInt8
    {
        Default,             //<! Type-appropriate default (decimal for ints, 'g' for floats, etc.).
        Binary,              //<! 'b' — binary integer, e.g. "1010".
        BinaryUpper,         //<! 'B' — binary integer, uppercase prefix "0B" when altForm=true.
        Octal,               //<! 'o' — octal integer, e.g. "17".
        Decimal,             //<! 'd' — decimal integer.
        HexLower,            //<! 'x' — lowercase hexadecimal, e.g. "ff".
        HexUpper,            //<! 'X' — uppercase hexadecimal, e.g. "FF".
        FloatFixed,          //<! 'f' — fixed-point float, e.g. "3.140000".
        FloatFixedUpper,     //<! 'F' — same as 'f', uppercase special values (NAN, INF).
        FloatSci,            //<! 'e' — scientific notation, e.g. "3.140000e+00".
        FloatSciUpper,       //<! 'E' — scientific notation, uppercase exponent, e.g. "3.14E+00".
        FloatGeneral,        //<! 'g' — shortest of 'e' or 'f'.
        FloatGeneralUpper,   //<! 'G' — uppercase variant of 'g'.
        String,              //<! 's' — explicit string presentation.
        Char,                //<! 'c' — single-character presentation.
        Pointer,             //<! 'p' — pointer address as "0x…" hex.
    };

    CharType fillChar = CharType(' ');   //<! The fill character (default: space).
    Alignment align = Alignment::Default;
    Sign sign = Sign::Default;
    bool altForm = false;   //<! '#' flag: adds 0x/0b/0 prefix for integers.
    bool zeroPad = false;   //<! '0' flag: zero-pads within the field width after sign/prefix.
    Int32 width = -1;       //<! Minimum field width (-1 = no minimum).
    Int32 precision = -1;   //<! Precision for floats or max chars for strings (-1 = default).
    Presentation type = Presentation::Default;
};

/// \brief Primary formatter template. Specialize this to add formatting support for custom types.
///
/// Each specialization must provide:
/// \code
///   static void Format(TBasicString<CharType>& out, const T& value, const TFormatSpec<CharType>& spec);
/// \endcode
///
/// The primary template body contains a static_assert that fires for unrecognized types,
/// producing a clear error message at compile time.
///
/// \tparam CharType The character type of the output string.
/// \tparam T        The value type to format.
template <Concepts::IsCharacter CharType, typename T>
struct TFormatter
{
    static void Format(Container::TBasicString<CharType>&, const T&, const TFormatSpec<CharType>&)
    {
        static_assert(
            sizeof(T) == 0,
            "No TFormatter<CharType, T> specialisation found. " "Provide one by specialising GP::Utilities::TFormatter."
        );
    }
};

/// \brief Type-erased wrapper around a single format argument.
///
/// Stores a const pointer to the original value and a statically-generated dispatch function.
/// Zero dynamic allocation; the dispatch function pointer is embedded per-type at compile time.
///
/// \tparam CharType The character type of the output string.
template <Concepts::IsCharacter CharType>
struct TFormatArg
{
    using FormatFn = void (*)(Container::TBasicString<CharType>&, const void*, const TFormatSpec<CharType>&);

    const void* m_value = nullptr;
    FormatFn m_formatFn = nullptr;

    /// \brief Constructs a TFormatArg bound to value, using TFormatter<CharType, T> for dispatch.
    /// \tparam T The concrete type of the argument.
    /// \param value The argument value. Must outlive this TFormatArg (i.e., the Format() call).
    /// \return A new TFormatArg wrapping value.
    template <typename T>
    GP_NODISCARD static TFormatArg Make(const T& value) noexcept;

    /// \brief Formats the stored value into out using spec.
    void Format(Container::TBasicString<CharType>& out, const TFormatSpec<CharType>& spec) const;
};

/// \brief Formats a string according to fmt, substituting each `{}` placeholder with the
/// corresponding argument.
///
/// Placeholder syntax:
/// \code
///   {}        — next argument, default format
///   {:spec}   — next argument with format spec
///   {N}       — argument at index N (0-based), default format
///   {N:spec}  — argument at index N with format spec
///   {{        — literal '{'
///   }}        — literal '}'
/// \endcode
///
/// \tparam Args Variadic argument types. Each must have a TFormatter specialization.
/// \param fmt The format string.
/// \param args The arguments to substitute.
/// \return A new formatted string.
/// \warning Mixing positional ({N}) and sequential ({}) placeholders in the same format string produces undefined
/// ordering and should be avoided.
template <Concepts::IsCharacter CharType, typename... Args>
GP_NODISCARD Container::TBasicString<CharType> Format(Container::TBasicStringView<CharType> fmt, Args&&... args);

/// \brief Convenience overload accepting a null-terminated C-string as the format string.
/// \tparam Args Variadic argument types.
/// \param fmt  Null-terminated format C-string.
/// \param args The arguments to substitute.
/// \return A new formatted string.
template <Concepts::IsCharacter CharType, typename... Args>
GP_NODISCARD Container::TBasicString<CharType> Format(const CharType* fmt, Args&&... args);

/// \brief Parses a format spec string (the content between ':' and '}' in a placeholder).
/// \param spec The format spec string view, may be empty.
/// \return The parsed TFormatSpec<CharType>.
template <Concepts::IsCharacter CharType>
GP_NODISCARD TFormatSpec<CharType> ParseFormatSpec(Container::TBasicStringView<CharType> spec) noexcept;

}   // namespace GP::Utilities

// Include the implementation
#include "Utilities/StringFormat.inl"
