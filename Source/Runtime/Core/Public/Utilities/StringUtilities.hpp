// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicString.hpp"
#include "Container/BasicStringView.hpp"
#include "Container/ContainerForward.hpp"
#include "CoreBuild.hpp"
#include <cctype>
#include <iterator>

namespace GP::Utilities
{

/// \brief Returns true if the character is an ASCII alphabetic letter (A-Z, a-z).
/// \note Uses unsigned cast to avoid undefined behavior with signed char on MSVC/GCC.
/// \param ch The character to test.
/// \return true if the character is alphabetic.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsAlpha(CharType ch) noexcept;

/// \brief Returns true if the character is an ASCII decimal digit (0-9).
/// \param ch The character to test.
/// \return true if the character is a decimal digit.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsDigit(CharType ch) noexcept;

/// \brief Returns true if the character is alphanumeric (A-Z, a-z, 0-9).
/// \param ch The character to test.
/// \return true if the character is alphanumeric.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsAlNum(CharType ch) noexcept;

/// \brief Returns true if the character is an ASCII whitespace character (space, tab, CR, LF, VT, FF).
/// \param ch The character to test.
/// \return true if the character is whitespace.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsSpace(CharType ch) noexcept;

/// \brief Returns true if the character is an ASCII uppercase letter (A-Z).
/// \param ch The character to test.
/// \return true if the character is uppercase.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsUpper(CharType ch) noexcept;

/// \brief Returns true if the character is an ASCII lowercase letter (a-z).
/// \param ch The character to test.
/// \return true if the character is lowercase.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsLower(CharType ch) noexcept;

/// \brief Returns true if the character is a hexadecimal digit (0-9, a-f, A-F).
/// \param ch The character to test.
/// \return true if the character is a hex digit.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsHexDigit(CharType ch) noexcept;

/// \brief Returns true if the character is a printable ASCII character (0x20-0x7E).
/// \param ch The character to test.
/// \return true if the character is printable.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE bool IsPrint(CharType ch) noexcept;

/// \brief Converts an ASCII character to its uppercase equivalent. Non-alpha characters are returned unchanged.
/// \param ch The character to convert.
/// \return The uppercase character, or ch if it has no uppercase form.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE CharType ToUpper(CharType ch) noexcept;

/// \brief Converts an ASCII character to its lowercase equivalent. Non-alpha characters are returned unchanged.
/// \param ch The character to convert.
/// \return The lowercase character, or ch if it has no lowercase form.
template <Concepts::IsCharacter CharType>
GP_NODISCARD GP_FORCEINLINE CharType ToLower(CharType ch) noexcept;

/// \brief Returns a new string with every character converted to uppercase.
/// \param str The input string view.
/// \return A new owning string in all-uppercase.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToUpperStr(Container::TBasicStringView<CharType> str);

/// \brief Returns a new string with every character converted to lowercase.
/// \param str The input string view.
/// \return A new owning string in all-lowercase.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToLowerStr(Container::TBasicStringView<CharType> str);

/// \brief Returns a new string in Title Case: first letter of each whitespace-separated word is
/// uppercased, the rest lowercased.
/// \param str The input string view.
/// \return A new owning string in Title Case.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToTitleCase(Container::TBasicStringView<CharType> str);

/// \brief Converts an identifier to lowerCamelCase (e.g., "my_variable" -> "myVariable").
/// \param str The input identifier.
/// \return The identifier in lowerCamelCase.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToCamelCase(Container::TBasicStringView<CharType> str);

/// \brief Converts an identifier to UpperCamelCase / PascalCase (e.g., "my_variable" -> "MyVariable").
/// \param str The input identifier.
/// \return The identifier in PascalCase.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToPascalCase(Container::TBasicStringView<CharType> str);

/// \brief Converts an identifier to snake_case (e.g., "myVariable" -> "my_variable").
/// \param str The input identifier.
/// \return The identifier in snake_case.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToSnakeCase(Container::TBasicStringView<CharType> str);

/// \brief Converts an identifier to kebab-case (e.g., "myVariable" -> "my-variable").
/// \param str The input identifier.
/// \return The identifier in kebab-case.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToKebabCase(Container::TBasicStringView<CharType> str);

/// \brief Converts an identifier to SCREAMING_SNAKE_CASE (e.g., "myVariable" -> "MY_VARIABLE").
/// \param str The input identifier.
/// \return The identifier in SCREAMING_SNAKE_CASE.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ToScreamingSnakeCase(Container::TBasicStringView<CharType> str);

/// \brief Returns a zero-copy view with all leading whitespace removed.
/// \param str The input string view.
/// \return A subview starting at the first non-whitespace character.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicStringView<CharType> TrimLeftView(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns a zero-copy view with all trailing whitespace removed.
/// \param str The input string view.
/// \return A subview ending at the last non-whitespace character.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicStringView<CharType> TrimRightView(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns a zero-copy view with all leading and trailing whitespace removed.
/// \param str The input string view.
/// \return A subview of the trimmed content.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicStringView<CharType> TrimView(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns a new owning string with all leading whitespace removed.
/// \param str The input string view.
/// \return A new trimmed string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> TrimLeft(Container::TBasicStringView<CharType> str);

/// \brief Returns a new owning string with all trailing whitespace removed.
/// \param str The input string view.
/// \return A new trimmed string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> TrimRight(Container::TBasicStringView<CharType> str);

/// \brief Returns a new owning string with all leading and trailing whitespace removed.
/// \param str The input string view.
/// \return A new trimmed string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> Trim(Container::TBasicStringView<CharType> str);

/// \brief Returns a new string left-padded (content is right-aligned) to totalWidth using fillChar.
/// \note If the string is already >= totalWidth characters, it is returned unchanged.
/// \param str The input string view.
/// \param totalWidth The desired total character width.
/// \param fillChar The character used for padding (default: space).
/// \return The padded string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType>
    PadLeft(Container::TBasicStringView<CharType> str, SizeT totalWidth, CharType fillChar = CharType(' '));

/// \brief Returns a new string right-padded (content is left-aligned) to totalWidth using fillChar.
/// \note If the string is already >= totalWidth characters, it is returned unchanged.
/// \param str The input string view.
/// \param totalWidth The desired total character width.
/// \param fillChar The character used for padding (default: space).
/// \return The padded string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType>
    PadRight(Container::TBasicStringView<CharType> str, SizeT totalWidth, CharType fillChar = CharType(' '));

/// \brief Returns a new string center-padded to totalWidth using fillChar.
/// \note When padding is odd, the extra character is appended to the right side.
/// \note If the string is already >= totalWidth characters, it is returned unchanged.
/// \param str The input string view.
/// \param totalWidth The desired total character width.
/// \param fillChar The character used for padding (default: space).
/// \return The padded string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType>
    PadCenter(Container::TBasicStringView<CharType> str, SizeT totalWidth, CharType fillChar = CharType(' '));

/// \brief Returns a new string formed by concatenating str with itself count times.
/// \note Returns an empty string when count is 0.
/// \param str The string to repeat.
/// \param count The number of repetitions.
/// \return The repeated string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> Repeat(Container::TBasicStringView<CharType> str, SizeT count);

/// \brief Returns a new string with every non-overlapping occurrence of `from` replaced by `to`.
/// \note Replacement proceeds left-to-right; newly inserted text is not re-scanned.
/// \param str The source string.
/// \param from The substring to search for. If empty, the original string is returned unchanged.
/// \param to The replacement string.
/// \return A new string with all replacements applied.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> ReplaceAll(
    Container::TBasicStringView<CharType> str,
    Container::TBasicStringView<CharType> from,
    Container::TBasicStringView<CharType> to
);

/// \brief Counts the number of non-overlapping occurrences of a substring.
/// \param str The string to search within.
/// \param sub The substring to count. If empty, returns 0.
/// \return The number of non-overlapping occurrences.
template <Concepts::IsCharacter CharType>
GP_NODISCARD SizeT
    CountOccurrences(Container::TBasicStringView<CharType> str, Container::TBasicStringView<CharType> sub) noexcept;

/// \brief Counts the number of occurrences of a single character.
/// \param str The string to search within.
/// \param ch The character to count.
/// \return The number of occurrences.
template <Concepts::IsCharacter CharType>
GP_NODISCARD SizeT CountOccurrences(Container::TBasicStringView<CharType> str, CharType ch) noexcept;

/// \brief If str begins with prefix, returns a zero-copy view with that prefix removed.
/// \note If the prefix is not found, the original view is returned unchanged.
/// \param str The source string.
/// \param prefix The prefix to strip.
/// \return A view with the prefix removed, or the original view.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicStringView<CharType>
    StripPrefix(Container::TBasicStringView<CharType> str, Container::TBasicStringView<CharType> prefix) noexcept;

/// \brief If str ends with suffix, returns a zero-copy view with that suffix removed.
/// \note If the suffix is not found, the original view is returned unchanged.
/// \param str The source string.
/// \param suffix The suffix to strip.
/// \return A view with the suffix removed, or the original view.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicStringView<CharType>
    StripSuffix(Container::TBasicStringView<CharType> str, Container::TBasicStringView<CharType> suffix) noexcept;

/// \brief Splits str by a single-character delimiter and writes each part to an output iterator.
/// \tparam OutputIt Must be an output iterator whose value type accepts TBasicStringView<CharType>
///         or TBasicString<CharType>.
/// \param str The string to split.
/// \param delimiter The delimiter character.
/// \param out The output iterator receiving each part.
/// \param maxParts Maximum number of parts to emit. The final part contains the remainder of the
///        string if the limit is reached before the end. Pass NPOS for unlimited.
/// \note Empty parts between consecutive delimiters are emitted as empty views.
template <Concepts::IsCharacter CharType, typename OutputIt>
void Split(
    Container::TBasicStringView<CharType> str,
    CharType delimiter,
    OutputIt out,
    SizeT maxParts = Container::TBasicStringView<CharType>::NPOS
);

/// \brief Splits str by a multi-character delimiter substring and writes each part to an output iterator.
/// \tparam OutputIt Must be an output iterator whose value type accepts TBasicStringView<CharType>
///         or TBasicString<CharType>.
/// \param str The string to split.
/// \param delimiter The delimiter substring. If empty, the entire str is emitted as one part.
/// \param out The output iterator receiving each part.
/// \param maxParts Maximum number of parts to emit.
template <Concepts::IsCharacter CharType, typename OutputIt>
void Split(
    Container::TBasicStringView<CharType> str,
    Container::TBasicStringView<CharType> delimiter,
    OutputIt out,
    SizeT maxParts = Container::TBasicStringView<CharType>::NPOS
);

/// \brief Concatenates the range [first, last) into a single string, inserting separator between each pair.
/// \tparam InputIt An input iterator whose value type is implicitly convertible to TBasicStringView<CharType>.
/// \param first Iterator to the first element.
/// \param last  Iterator past the last element.
/// \param separator The string inserted between consecutive elements.
/// \return A new string containing all elements joined by separator.
template <Concepts::IsCharacter CharType, typename InputIt>
GP_NODISCARD Container::TBasicString<CharType>
    Join(InputIt first, InputIt last, Container::TBasicStringView<CharType> separator);

/// \brief Returns a new string with all characters in reverse order.
/// \param str The input string.
/// \return A new reversed string.
template <Concepts::IsCharacter CharType>
GP_NODISCARD Container::TBasicString<CharType> Reverse(Container::TBasicStringView<CharType> str);

/// \brief Returns true if the string is non-empty and every character satisfies IsAlpha.
template <Concepts::IsCharacter CharType>
GP_NODISCARD bool IsAllAlpha(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns true if the string is non-empty and every character satisfies IsDigit.
template <Concepts::IsCharacter CharType>
GP_NODISCARD bool IsAllDigits(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns true if the string is non-empty and every character satisfies IsAlNum.
template <Concepts::IsCharacter CharType>
GP_NODISCARD bool IsAllAlNum(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns true if the string is empty or every character satisfies IsSpace.
template <Concepts::IsCharacter CharType>
GP_NODISCARD bool IsAllWhitespace(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns true if the string is non-empty and every alphabetic character is uppercase.
/// \note Non-alphabetic characters (digits, punctuation) are ignored by this check.
template <Concepts::IsCharacter CharType>
GP_NODISCARD bool IsAllUpper(Container::TBasicStringView<CharType> str) noexcept;

/// \brief Returns true if the string is non-empty and every alphabetic character is lowercase.
/// \note Non-alphabetic characters (digits, punctuation) are ignored by this check.
template <Concepts::IsCharacter CharType>
GP_NODISCARD bool IsAllLower(Container::TBasicStringView<CharType> str) noexcept;

}   // namespace GP::Utilities

// Include the implementation
#include "Utilities/StringUtilities.inl"
