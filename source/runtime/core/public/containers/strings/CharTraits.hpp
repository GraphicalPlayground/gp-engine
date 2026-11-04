// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/strings/StringEnums.hpp"
#include "CoreMinimal.hpp"
#include <type_traits>

namespace gp::container
{

/// @brief Engine-native, `constexpr`, locale-independent character traits and buffer primitives (ADR-0045).
/// @details Replaces `std::char_traits<CharT>` (`<string>`) and `<cctype>` across all `gp-engine` string and view
///          containers. All character classification, case-folding, comparison, search, and 64-bit FNV-1a hashing
///          operations are deterministic, lock-free, and immune to OS locale mutations (such as the Turkish-I problem).
/// @tparam InCharT The character code-unit type (`char`, `wchar_t`, `char8_t`, `char16_t`, or `char32_t`).
template <concepts::IsCharacter InCharT>
struct CharTraits
{
public:
    using CharType = InCharT;
    using UnsignedType = std::make_unsigned_t<InCharT>;
    using SizeType = gp::USize;

    /// @brief Sentinel index returned when a search operation finds no match.
    static constexpr SizeType kNpos = static_cast<SizeType>(-1);

    /// @brief Alias for `kNpos` matching `BasicStringView::npos` and `BasicString::npos`.
    static constexpr SizeType npos = kNpos;

    /// @brief 64-bit FNV-1a offset basis constant (`0xcbf29ce484222325`).
    static constexpr gp::UInt64 kFnv1a64OffsetBasis = 14695981039346656037ULL;

    /// @brief 64-bit FNV-1a prime multiplier (`0x100000001b3`).
    static constexpr gp::UInt64 kFnv1a64Prime = 1099511628211ULL;

public:
    /// @section ASCII Classification & Case Folding (`constexpr`)

    /// @brief Checks whether `ch` lies within the 7-bit ASCII range (`0x00`..`0x7F`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is a 7-bit ASCII character, false otherwise.
    [[nodiscard]] static constexpr bool isAscii(const CharType ch) noexcept
    {
        return static_cast<UnsignedType>(ch) <= static_cast<UnsignedType>(0x7Fu);
    }

    /// @brief Checks whether `ch` is an uppercase ASCII letter (`'A'`..`'Z'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is `'A'`..`'Z'`, false otherwise.
    [[nodiscard]] static constexpr bool isUpper(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return u >= static_cast<UnsignedType>('A') && u <= static_cast<UnsignedType>('Z');
    }

    /// @brief Checks whether `ch` is a lowercase ASCII letter (`'a'`..`'z'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is `'a'`..`'z'`, false otherwise.
    [[nodiscard]] static constexpr bool isLower(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return u >= static_cast<UnsignedType>('a') && u <= static_cast<UnsignedType>('z');
    }

    /// @brief Checks whether `ch` is an ASCII alphabetic character (`'A'`..`'Z'` or `'a'`..`'z'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is an ASCII letter, false otherwise.
    [[nodiscard]] static constexpr bool isAlpha(const CharType ch) noexcept
    {
        return isUpper(ch) || isLower(ch);
    }

    /// @brief Checks whether `ch` is an ASCII decimal digit (`'0'`..`'9'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is `'0'`..`'9'`, false otherwise.
    [[nodiscard]] static constexpr bool isDigit(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return u >= static_cast<UnsignedType>('0') && u <= static_cast<UnsignedType>('9');
    }

    /// @brief Checks whether `ch` is an ASCII hexadecimal digit (`'0'`..`'9'`, `'a'`..`'f'`, or `'A'`..`'F'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is a hexadecimal digit, false otherwise.
    [[nodiscard]] static constexpr bool isHexDigit(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return isDigit(ch) || (u >= static_cast<UnsignedType>('a') && u <= static_cast<UnsignedType>('f')) ||
               (u >= static_cast<UnsignedType>('A') && u <= static_cast<UnsignedType>('F'));
    }

    /// @brief Checks whether `ch` is an ASCII alphanumeric character (`'A'`..`'Z'`, `'a'`..`'z'`, or `'0'`..`'9'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is ASCII alphanumeric, false otherwise.
    [[nodiscard]] static constexpr bool isAlphaNumeric(const CharType ch) noexcept
    {
        return isAlpha(ch) || isDigit(ch);
    }

    /// @brief Alias for `isAlphaNumeric(ch)`.
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is ASCII alphanumeric, false otherwise.
    [[nodiscard]] static constexpr bool isAlnum(const CharType ch) noexcept
    {
        return isAlphaNumeric(ch);
    }

    /// @brief Checks whether `ch` is an ASCII whitespace character (`' '`, `'\t'`, `'\n'`, `'\r'`, `'\v'`, or `'\f'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is ASCII whitespace, false otherwise.
    [[nodiscard]] static constexpr bool isWhitespace(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return u == static_cast<UnsignedType>(' ') ||
               (u >= static_cast<UnsignedType>('\t') && u <= static_cast<UnsignedType>('\r'));
    }

    /// @brief Checks whether `ch` is a horizontal blank character (`' '` or `'\t'`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is `' '` or `'\t'`, false otherwise.
    [[nodiscard]] static constexpr bool isBlank(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return u == static_cast<UnsignedType>(' ') || u == static_cast<UnsignedType>('\t');
    }

    /// @brief Checks whether `ch` is an ASCII control character (`0x00`..`0x1F` or `0x7F`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is an ASCII control character, false otherwise.
    [[nodiscard]] static constexpr bool isControl(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return u <= static_cast<UnsignedType>(0x1Fu) || u == static_cast<UnsignedType>(0x7Fu);
    }

    /// @brief Checks whether `ch` is a printable ASCII character (`0x20`..`0x7E`).
    /// @param[in] ch The character code unit to inspect.
    /// @return True if `ch` is printable ASCII, false otherwise.
    [[nodiscard]] static constexpr bool isPrintable(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        return u >= static_cast<UnsignedType>(0x20u) && u <= static_cast<UnsignedType>(0x7Eu);
    }

    /// @brief Converts an uppercase ASCII character to lowercase; leaves all other characters unchanged.
    /// @param[in] ch The character code unit to fold.
    /// @return Lowercase ASCII equivalent if `ch` is `'A'`..`'Z'`, otherwise `ch`.
    [[nodiscard]] static constexpr CharType toLower(const CharType ch) noexcept
    {
        if (isUpper(ch))
        {
            return static_cast<CharType>(static_cast<UnsignedType>(ch) + static_cast<UnsignedType>('a' - 'A'));
        }
        return ch;
    }

    /// @brief Converts a lowercase ASCII character to uppercase; leaves all other characters unchanged.
    /// @param[in] ch The character code unit to fold.
    /// @return Uppercase ASCII equivalent if `ch` is `'a'`..`'z'`, otherwise `ch`.
    [[nodiscard]] static constexpr CharType toUpper(const CharType ch) noexcept
    {
        if (isLower(ch))
        {
            return static_cast<CharType>(static_cast<UnsignedType>(ch) - static_cast<UnsignedType>('a' - 'A'));
        }
        return ch;
    }

    /// @brief Converts an ASCII hexadecimal digit character to its integer value (`0`..`15`).
    /// @param[in] ch The hexadecimal character (`'0'`..`'9'`, `'a'`..`'f'`, or `'A'`..`'F'`).
    /// @return Integer value in `[0, 15]`, or `-1` if `ch` is not a valid hexadecimal digit.
    [[nodiscard]] static constexpr gp::Int32 hexDigitToValue(const CharType ch) noexcept
    {
        const UnsignedType u = static_cast<UnsignedType>(ch);
        if (u >= static_cast<UnsignedType>('0') && u <= static_cast<UnsignedType>('9'))
        {
            return static_cast<gp::Int32>(u - static_cast<UnsignedType>('0'));
        }
        if (u >= static_cast<UnsignedType>('a') && u <= static_cast<UnsignedType>('f'))
        {
            return static_cast<gp::Int32>(10u + (u - static_cast<UnsignedType>('a')));
        }
        if (u >= static_cast<UnsignedType>('A') && u <= static_cast<UnsignedType>('F'))
        {
            return static_cast<gp::Int32>(10u + (u - static_cast<UnsignedType>('A')));
        }
        return -1;
    }

    /// @brief Converts a 4-bit nibble (`0`..`15`) to an uppercase ASCII hexadecimal character (`'0'`..`'9'`,
    /// `'A'`..`'F'`).
    /// @param[in] nibble Low 4-bit value (masked with `0x0F`).
    /// @return Uppercase hexadecimal character.
    [[nodiscard]] static constexpr CharType valueToHexDigitUpper(const gp::UInt8 nibble) noexcept
    {
        const gp::UInt8 v = static_cast<gp::UInt8>(nibble & 0x0Fu);
        return static_cast<CharType>(v < 10u ? ('0' + v) : ('A' + (v - 10u)));
    }

    /// @brief Converts a 4-bit nibble (`0`..`15`) to a lowercase ASCII hexadecimal character (`'0'`..`'9'`,
    /// `'a'`..`'f'`).
    /// @param[in] nibble Low 4-bit value (masked with `0x0F`).
    /// @return Lowercase hexadecimal character.
    [[nodiscard]] static constexpr CharType valueToHexDigitLower(const gp::UInt8 nibble) noexcept
    {
        const gp::UInt8 v = static_cast<gp::UInt8>(nibble & 0x0Fu);
        return static_cast<CharType>(v < 10u ? ('0' + v) : ('a' + (v - 10u)));
    }

public:
    /// @section Single-Character Comparison (`constexpr`)

    /// @brief Checks whether two characters are equal using exact code-unit comparison.
    /// @param[in] a Left-hand character.
    /// @param[in] b Right-hand character.
    /// @return True if `a == b`, false otherwise.
    [[nodiscard]] static constexpr bool eq(const CharType a, const CharType b) noexcept
    {
        return static_cast<UnsignedType>(a) == static_cast<UnsignedType>(b);
    }

    /// @brief Checks whether two characters are equal under ASCII case-insensitive folding.
    /// @param[in] a Left-hand character.
    /// @param[in] b Right-hand character.
    /// @return True if `toLower(a) == toLower(b)`, false otherwise.
    [[nodiscard]] static constexpr bool eqIgnoreCase(const CharType a, const CharType b) noexcept
    {
        return static_cast<UnsignedType>(toLower(a)) == static_cast<UnsignedType>(toLower(b));
    }

    /// @brief Checks whether two characters are equal under the specified `SearchCase` mode.
    /// @param[in] a Left-hand character.
    /// @param[in] b Right-hand character.
    /// @param[in] searchCase Case sensitivity mode.
    /// @return True if `a` and `b` match under `searchCase`, false otherwise.
    [[nodiscard]] static constexpr bool eq(const CharType a, const CharType b, const SearchCase searchCase) noexcept
    {
        return (searchCase == SearchCase::CaseSensitive) ? eq(a, b) : eqIgnoreCase(a, b);
    }

    /// @brief Checks whether `a` is strictly less than `b` using unsigned code-unit ordering.
    /// @param[in] a Left-hand character.
    /// @param[in] b Right-hand character.
    /// @return True if `UnsignedType(a) < UnsignedType(b)`, false otherwise.
    [[nodiscard]] static constexpr bool lt(const CharType a, const CharType b) noexcept
    {
        return static_cast<UnsignedType>(a) < static_cast<UnsignedType>(b);
    }

    /// @brief Checks whether `a` is strictly less than `b` under ASCII case-insensitive folding.
    /// @param[in] a Left-hand character.
    /// @param[in] b Right-hand character.
    /// @return True if `UnsignedType(toLower(a)) < UnsignedType(toLower(b))`, false otherwise.
    [[nodiscard]] static constexpr bool ltIgnoreCase(const CharType a, const CharType b) noexcept
    {
        return static_cast<UnsignedType>(toLower(a)) < static_cast<UnsignedType>(toLower(b));
    }

public:
    /// @section Contiguous Character Buffer Operations (`constexpr`)

    /// @brief Assigns character `src` to `dst`.
    /// @param[out] dst Destination character reference.
    /// @param[in] src Source character value.
    static constexpr void assign(CharType& dst, const CharType src) noexcept
    {
        dst = src;
    }

    /// @brief Fills `count` characters starting at `dst` with `ch` (`std::char_traits::assign` overload).
    /// @param[out] dst Destination character buffer.
    /// @param[in] count Number of characters to write.
    /// @param[in] ch Fill character value.
    /// @return `dst`.
    static constexpr CharType* assign(CharType* const dst, const SizeType count, const CharType ch) noexcept
    {
        return fill(dst, count, ch);
    }

    /// @brief Fills `count` characters starting at `dst` with `ch`.
    /// @param[out] dst Destination character buffer.
    /// @param[in] count Number of characters to write.
    /// @param[in] ch Fill character value.
    /// @return `dst`.
    static constexpr CharType* fill(CharType* const dst, const SizeType count, const CharType ch) noexcept
    {
        if (dst == nullptr || count == 0u)
        {
            return dst;
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            dst[i] = ch;
        }
        return dst;
    }

    /// @brief Copies `count` non-overlapping characters from `src` to `dst`.
    /// @param[out] dst Destination character buffer.
    /// @param[in] src Source character buffer.
    /// @param[in] count Number of characters to copy.
    /// @return `dst`.
    static constexpr CharType* copy(CharType* const dst, const CharType* const src, const SizeType count) noexcept
    {
        if (dst == nullptr || src == nullptr || count == 0u)
        {
            return dst;
        }

        if (!std::is_constant_evaluated())
        {
            if (dst == src)
            {
                return dst;
            }
            __builtin_memcpy(dst, src, count * sizeof(CharType));
            return dst;
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            dst[i] = src[i];
        }
        return dst;
    }

    /// @brief Copies `count` potentially overlapping characters from `src` to `dst`.
    /// @param[out] dst Destination character buffer.
    /// @param[in] src Source character buffer.
    /// @param[in] count Number of characters to move.
    /// @return `dst`.
    static constexpr CharType* move(CharType* const dst, const CharType* const src, const SizeType count) noexcept
    {
        if (dst == nullptr || src == nullptr || count == 0u)
        {
            return dst;
        }

        if (!std::is_constant_evaluated())
        {
            if (dst == src)
            {
                return dst;
            }
            __builtin_memmove(dst, src, count * sizeof(CharType));
            return dst;
        }

        bool copyBackward = false;
        for (SizeType i = 0u; i < count; ++i)
        {
            if (dst == (src + i))
            {
                copyBackward = true;
                break;
            }
        }

        if (copyBackward)
        {
            for (SizeType i = count; i > 0u; --i)
            {
                dst[i - 1u] = src[i - 1u];
            }
        }
        else
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                dst[i] = src[i];
            }
        }
        return dst;
    }

    /// @brief Computes the number of code units preceding the null terminator in `str` (`0` if `str == nullptr`).
    /// @param[in] str Null-terminated character pointer, or `nullptr`.
    /// @return Length of `str` in `CharType` units.
    [[nodiscard]] static constexpr SizeType length(const CharType* const str) noexcept
    {
        if (str == nullptr)
        {
            return 0u;
        }

        if constexpr (sizeof(CharType) == 1u)
        {
            if (!std::is_constant_evaluated())
            {
                return static_cast<SizeType>(__builtin_strlen(reinterpret_cast<const char*>(str)));
            }
        }

        SizeType len = 0u;
        while (str[len] != CharType{ 0 })
        {
            ++len;
        }
        return len;
    }

    /// @brief Computes the bounded length of `str`, scanning at most `maxCount` code units (`strnlen` semantics).
    /// @param[in] str Character buffer pointer, or `nullptr`.
    /// @param[in] maxCount Maximum number of characters to inspect.
    /// @return Number of characters before the first null terminator, or `maxCount` if none is found.
    [[nodiscard]] static constexpr SizeType lengthBounded(const CharType* const str, const SizeType maxCount) noexcept
    {
        if (str == nullptr || maxCount == 0u)
        {
            return 0u;
        }

        SizeType len = 0u;
        while (len < maxCount && str[len] != CharType{ 0 })
        {
            ++len;
        }
        return len;
    }

    /// @brief Lexicographically compares `count` characters of `lhs` and `rhs` (case-sensitive).
    /// @param[in] lhs First character sequence.
    /// @param[in] rhs Second character sequence.
    /// @param[in] count Number of characters to compare.
    /// @return Negative if `lhs < rhs`, `0` if equal, positive if `lhs > rhs`.
    [[nodiscard]] static constexpr gp::Int32
        compare(const CharType* const lhs, const CharType* const rhs, const SizeType count) noexcept
    {
        if (count == 0u)
        {
            return 0;
        }
        if (!std::is_constant_evaluated())
        {
            if (lhs == rhs)
            {
                return 0;
            }
        }
        if (lhs == nullptr)
        {
            return (rhs == nullptr) ? 0 : -1;
        }
        if (rhs == nullptr)
        {
            return 1;
        }

        if constexpr (sizeof(CharType) == 1u)
        {
            if (!std::is_constant_evaluated())
            {
                const gp::Int32 cmp = __builtin_memcmp(lhs, rhs, count);
                return (cmp < 0) ? -1 : ((cmp > 0) ? 1 : 0);
            }
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            const UnsignedType a = static_cast<UnsignedType>(lhs[i]);
            const UnsignedType b = static_cast<UnsignedType>(rhs[i]);
            if (a != b)
            {
                return (a < b) ? -1 : 1;
            }
        }
        return 0;
    }

    /// @brief Lexicographically compares `count` characters of `lhs` and `rhs` using ASCII case-insensitive folding.
    /// @param[in] lhs First character sequence.
    /// @param[in] rhs Second character sequence.
    /// @param[in] count Number of characters to compare.
    /// @return Negative if `lhs < rhs`, `0` if equal, positive if `lhs > rhs`.
    [[nodiscard]] static constexpr gp::Int32
        compareIgnoreCase(const CharType* const lhs, const CharType* const rhs, const SizeType count) noexcept
    {
        if (count == 0u)
        {
            return 0;
        }
        if (!std::is_constant_evaluated())
        {
            if (lhs == rhs)
            {
                return 0;
            }
        }
        if (lhs == nullptr)
        {
            return (rhs == nullptr) ? 0 : -1;
        }
        if (rhs == nullptr)
        {
            return 1;
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            const UnsignedType a = static_cast<UnsignedType>(toLower(lhs[i]));
            const UnsignedType b = static_cast<UnsignedType>(toLower(rhs[i]));
            if (a != b)
            {
                return (a < b) ? -1 : 1;
            }
        }
        return 0;
    }

    /// @brief Lexicographically compares `count` characters of `lhs` and `rhs` under `searchCase`.
    /// @param[in] lhs First character sequence.
    /// @param[in] rhs Second character sequence.
    /// @param[in] count Number of characters to compare.
    /// @param[in] searchCase Case sensitivity mode.
    /// @return Negative if `lhs < rhs`, `0` if equal, positive if `lhs > rhs`.
    [[nodiscard]] static constexpr gp::Int32 compare(
        const CharType* const lhs, const CharType* const rhs, const SizeType count, const SearchCase searchCase
    ) noexcept
    {
        return (searchCase == SearchCase::CaseSensitive) ? compare(lhs, rhs, count)
                                                         : compareIgnoreCase(lhs, rhs, count);
    }

    /// @brief Checks whether `count` characters of `lhs` and `rhs` are equal (case-sensitive).
    /// @param[in] lhs First character sequence.
    /// @param[in] rhs Second character sequence.
    /// @param[in] count Number of characters to compare.
    /// @return True if all `count` characters match, false otherwise.
    [[nodiscard]] static constexpr bool
        equals(const CharType* const lhs, const CharType* const rhs, const SizeType count) noexcept
    {
        return compare(lhs, rhs, count) == 0;
    }

    /// @brief Checks whether `count` characters of `lhs` and `rhs` are equal under ASCII case-insensitive folding.
    /// @param[in] lhs First character sequence.
    /// @param[in] rhs Second character sequence.
    /// @param[in] count Number of characters to compare.
    /// @return True if all `count` characters match case-insensitively, false otherwise.
    [[nodiscard]] static constexpr bool
        equalsIgnoreCase(const CharType* const lhs, const CharType* const rhs, const SizeType count) noexcept
    {
        return compareIgnoreCase(lhs, rhs, count) == 0;
    }

    /// @brief Checks whether `count` characters of `lhs` and `rhs` are equal under `searchCase`.
    /// @param[in] lhs First character sequence.
    /// @param[in] rhs Second character sequence.
    /// @param[in] count Number of characters to compare.
    /// @param[in] searchCase Case sensitivity mode.
    /// @return True if all `count` characters match under `searchCase`, false otherwise.
    [[nodiscard]] static constexpr bool equals(
        const CharType* const lhs, const CharType* const rhs, const SizeType count, const SearchCase searchCase
    ) noexcept
    {
        return (searchCase == SearchCase::CaseSensitive) ? equals(lhs, rhs, count) : equalsIgnoreCase(lhs, rhs, count);
    }

    /// @brief Finds the first occurrence of `ch` within `[ptr, ptr + count)`.
    /// @param[in] ptr Start of the character range.
    /// @param[in] count Number of characters to scan.
    /// @param[in] ch Character to locate.
    /// @param[in] searchCase Case sensitivity mode.
    /// @return Pointer to the first matching character, or `nullptr` if not found.
    [[nodiscard]] static constexpr const CharType* find(
        const CharType* const ptr,
        const SizeType count,
        const CharType ch,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) noexcept
    {
        if (ptr == nullptr || count == 0u)
        {
            return nullptr;
        }

        if (searchCase == SearchCase::CaseSensitive)
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                if (eq(ptr[i], ch))
                {
                    return ptr + i;
                }
            }
        }
        else
        {
            const CharType foldedTarget = toLower(ch);
            for (SizeType i = 0u; i < count; ++i)
            {
                if (eq(toLower(ptr[i]), foldedTarget))
                {
                    return ptr + i;
                }
            }
        }
        return nullptr;
    }

    /// @brief Finds the last occurrence of `ch` within `[ptr, ptr + count)`.
    /// @param[in] ptr Start of the character range.
    /// @param[in] count Number of characters to scan.
    /// @param[in] ch Character to locate.
    /// @param[in] searchCase Case sensitivity mode.
    /// @return Pointer to the last matching character, or `nullptr` if not found.
    [[nodiscard]] static constexpr const CharType* rfind(
        const CharType* const ptr,
        const SizeType count,
        const CharType ch,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) noexcept
    {
        if (ptr == nullptr || count == 0u)
        {
            return nullptr;
        }

        if (searchCase == SearchCase::CaseSensitive)
        {
            for (SizeType i = count; i > 0u; --i)
            {
                if (eq(ptr[i - 1u], ch))
                {
                    return ptr + (i - 1u);
                }
            }
        }
        else
        {
            const CharType foldedTarget = toLower(ch);
            for (SizeType i = count; i > 0u; --i)
            {
                if (eq(toLower(ptr[i - 1u]), foldedTarget))
                {
                    return ptr + (i - 1u);
                }
            }
        }
        return nullptr;
    }

    /// @brief Finds the first occurrence of `[needle, needle + needleLen)` inside `[haystack, haystack + haystackLen)`.
    /// @param[in] haystack Haystack buffer pointer.
    /// @param[in] haystackLen Length of `haystack` in characters.
    /// @param[in] needle Needle buffer pointer.
    /// @param[in] needleLen Length of `needle` in characters.
    /// @param[in] searchCase Case sensitivity mode.
    /// @return Zero-based index of the first match, or `kNpos` if not found.
    [[nodiscard]] static constexpr SizeType findSubrange(
        const CharType* const haystack,
        const SizeType haystackLen,
        const CharType* const needle,
        const SizeType needleLen,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) noexcept
    {
        if (needleLen == 0u)
        {
            return 0u;
        }
        if (haystack == nullptr || needle == nullptr || needleLen > haystackLen)
        {
            return kNpos;
        }

        const SizeType lastStart = haystackLen - needleLen;
        const CharType firstNeedle = needle[0];

        for (SizeType i = 0u; i <= lastStart; ++i)
        {
            if (eq(haystack[i], firstNeedle, searchCase) &&
                equals(haystack + i + 1u, needle + 1u, needleLen - 1u, searchCase))
            {
                return i;
            }
        }
        return kNpos;
    }

    /// @brief Finds the last occurrence of `[needle, needle + needleLen)` inside `[haystack, haystack + haystackLen)`.
    /// @param[in] haystack Haystack buffer pointer.
    /// @param[in] haystackLen Length of `haystack` in characters.
    /// @param[in] needle Needle buffer pointer.
    /// @param[in] needleLen Length of `needle` in characters.
    /// @param[in] searchCase Case sensitivity mode.
    /// @return Zero-based index of the last match, or `kNpos` if not found.
    [[nodiscard]] static constexpr SizeType rfindSubrange(
        const CharType* const haystack,
        const SizeType haystackLen,
        const CharType* const needle,
        const SizeType needleLen,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) noexcept
    {
        if (needleLen == 0u)
        {
            return haystackLen;
        }
        if (haystack == nullptr || needle == nullptr || needleLen > haystackLen)
        {
            return kNpos;
        }

        const SizeType lastStart = haystackLen - needleLen;
        const CharType firstNeedle = needle[0];

        for (SizeType i = lastStart + 1u; i > 0u; --i)
        {
            const SizeType pos = i - 1u;
            if (eq(haystack[pos], firstNeedle, searchCase) &&
                equals(haystack + pos + 1u, needle + 1u, needleLen - 1u, searchCase))
            {
                return pos;
            }
        }
        return kNpos;
    }

public:
    /// @section 64-Bit FNV-1a Deterministic Hashing (`constexpr`)

    /// @brief Computes a 64-bit FNV-1a case-sensitive hash over `[data, data + count)`.
    /// @param[in] data Character sequence pointer.
    /// @param[in] count Number of characters to hash.
    /// @param[in] seed Initial 64-bit hash state (defaults to `kFnv1a64OffsetBasis`).
    /// @return 64-bit FNV-1a hash value.
    [[nodiscard]] static constexpr gp::UInt64
        hash(const CharType* const data, const SizeType count, const gp::UInt64 seed = kFnv1a64OffsetBasis) noexcept
    {
        gp::UInt64 h = seed;
        if (data == nullptr || count == 0u)
        {
            return h;
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            const UnsignedType u = static_cast<UnsignedType>(data[i]);
            if constexpr (sizeof(CharType) == 1u)
            {
                h ^= static_cast<gp::UInt64>(u);
                h *= kFnv1a64Prime;
            }
            else
            {
                for (SizeType b = 0u; b < sizeof(CharType); ++b)
                {
                    const gp::UInt8 byteVal =
                        static_cast<gp::UInt8>((u >> (b * 8u)) & static_cast<UnsignedType>(0xFFu));
                    h ^= static_cast<gp::UInt64>(byteVal);
                    h *= kFnv1a64Prime;
                }
            }
        }
        return h;
    }

    /// @brief Computes a 64-bit FNV-1a ASCII case-insensitive hash over `[data, data + count)`.
    /// @param[in] data Character sequence pointer.
    /// @param[in] count Number of characters to hash.
    /// @param[in] seed Initial 64-bit hash state (defaults to `kFnv1a64OffsetBasis`).
    /// @return 64-bit FNV-1a case-insensitive hash value.
    [[nodiscard]] static constexpr gp::UInt64 hashIgnoreCase(
        const CharType* const data, const SizeType count, const gp::UInt64 seed = kFnv1a64OffsetBasis
    ) noexcept
    {
        gp::UInt64 h = seed;
        if (data == nullptr || count == 0u)
        {
            return h;
        }

        for (SizeType i = 0u; i < count; ++i)
        {
            const UnsignedType u = static_cast<UnsignedType>(toLower(data[i]));
            if constexpr (sizeof(CharType) == 1u)
            {
                h ^= static_cast<gp::UInt64>(u);
                h *= kFnv1a64Prime;
            }
            else
            {
                for (SizeType b = 0u; b < sizeof(CharType); ++b)
                {
                    const gp::UInt8 byteVal =
                        static_cast<gp::UInt8>((u >> (b * 8u)) & static_cast<UnsignedType>(0xFFu));
                    h ^= static_cast<gp::UInt64>(byteVal);
                    h *= kFnv1a64Prime;
                }
            }
        }
        return h;
    }

    /// @brief Computes a 64-bit FNV-1a hash over `[data, data + count)` under `searchCase`.
    /// @param[in] data Character sequence pointer.
    /// @param[in] count Number of characters to hash.
    /// @param[in] searchCase Case sensitivity mode.
    /// @param[in] seed Initial 64-bit hash state (defaults to `kFnv1a64OffsetBasis`).
    /// @return 64-bit FNV-1a hash value.
    [[nodiscard]] static constexpr gp::UInt64 hash(
        const CharType* const data,
        const SizeType count,
        const SearchCase searchCase,
        const gp::UInt64 seed = kFnv1a64OffsetBasis
    ) noexcept
    {
        return (searchCase == SearchCase::CaseSensitive) ? hash(data, count, seed) : hashIgnoreCase(data, count, seed);
    }
};

}   // namespace gp::container

namespace gp
{

/// @brief Convenience alias for `gp::container::CharTraits` in the root `gp` namespace.
template <concepts::IsCharacter InCharT>
using CharTraits = container::CharTraits<InCharT>;

}   // namespace gp
