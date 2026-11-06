// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/StringEnums.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Scalar.hpp"
#include "templates/Enums.hpp"
#include "templates/Optional.hpp"
#include <charconv>
#include <compare>
#include <format>
#include <iterator>
#include <ostream>
#include <system_error>
#include <type_traits>

namespace gp::container
{

/// @brief Non-owning, read-only view into a contiguous character sequence (ADR-0045).
/// @details Provides a lightweight, non-allocating `16-byte` `{ const InCharT* m_data, USize m_size }` interface for
///          string inspection, slicing, case-aware searching, wildcard matching, tokenization, and numeric parsing.
///          Uses `gp::container::CharTraits<InCharT>` instead of `<string>` (`std::char_traits`).
/// @tparam InCharT The character code-unit type of the string view. Must satisfy `gp::concepts::IsCharacter`.
template <concepts::IsCharacter InCharT>
class BasicStringView
{
public:
    using TraitsType = CharTraits<InCharT>;
    using ValueType = InCharT;
    using Pointer = const InCharT*;
    using ConstPointer = const InCharT*;
    using Reference = const InCharT&;
    using ConstReference = const InCharT&;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Iterator = ConstPointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
    /// @brief Sentinel index indicating "not found" or "all remaining characters".
    static constexpr SizeType npos = TraitsType::kNpos;

public:
    /// @brief Default constructs an empty string view (`data() == nullptr`, `size() == 0`).
    [[nodiscard]] constexpr BasicStringView() noexcept = default;

    /// @brief Constructs a view from a null-terminated C string (`nullptr`-safe).
    /// @param[in] str Pointer to a null-terminated character array, or `nullptr`.
    [[nodiscard]] constexpr BasicStringView(const InCharT* const str) noexcept
        : m_data(str)
        , m_size(str != nullptr ? TraitsType::length(str) : 0u)
    {}

    /// @brief Constructs a view from a character pointer and explicit length.
    /// @param[in] str Pointer to the character array.
    /// @param[in] count Number of characters in the view.
    [[nodiscard]] constexpr BasicStringView(const InCharT* const str, const SizeType count) noexcept
        : m_data(str)
        , m_size(count)
    {}

    /// @brief Constructs a view from a contiguous iterator range `[first, last)`.
    /// @param[in] first Pointer to the first character.
    /// @param[in] last Pointer one past the last character.
    [[nodiscard]] constexpr BasicStringView(const InCharT* const first, const InCharT* const last) noexcept
        : m_data(first)
        , m_size((first != nullptr && last != nullptr && last >= first) ? static_cast<SizeType>(last - first) : 0u)
    {}

    /// @brief Copy constructor and copy assignment operator.
    [[nodiscard]] constexpr BasicStringView(const BasicStringView&) noexcept = default;
    constexpr BasicStringView& operator=(const BasicStringView&) noexcept = default;

public:
    /// @brief Unchecked element access.
    /// @param[in] pos Zero-based index of the character.
    /// @return Const reference to the character at `pos`.
    [[nodiscard]] constexpr ConstReference operator[](const SizeType pos) const noexcept
    {
        GP_ASSERT(pos < m_size, "BasicStringView::operator[] index out of bounds.");
        return m_data[pos];
    }

    /// @brief Case-sensitive equality comparison.
    /// @param[in] other The view to compare against.
    /// @return True if both views have the same length and identical code units, false otherwise.
    [[nodiscard]] constexpr bool operator==(const BasicStringView& other) const noexcept
    {
        return m_size == other.m_size && TraitsType::equals(m_data, other.m_data, m_size);
    }

    /// @brief Case-sensitive lexicographical three-way comparison.
    /// @param[in] other The view to compare against.
    /// @return `std::strong_ordering` indicating the relative order of the two views.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const BasicStringView& other) const noexcept
    {
        const SizeType len = m_size < other.m_size ? m_size : other.m_size;
        const gp::Int32 cmp = TraitsType::compare(m_data, other.m_data, len);
        if (cmp != 0)
        {
            return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
        }
        return m_size <=> other.m_size;
    }

public:
    /// @brief Returns a pointer to the underlying character array.
    /// @return Pointer to the first character in the view, or `nullptr` if default-constructed.
    [[nodiscard]] constexpr ConstPointer data() const noexcept
    {
        return m_data;
    }

    /// @brief Returns the number of character code units in the view.
    /// @return The length of the view (excluding any null terminator).
    [[nodiscard]] constexpr SizeType size() const noexcept
    {
        return m_size;
    }

    /// @brief Returns the number of character code units in the view (alias for `size()`).
    /// @return The length of the view (excluding any null terminator).
    [[nodiscard]] constexpr SizeType length() const noexcept
    {
        return m_size;
    }

    /// @brief Returns the size of the viewed sequence in bytes (`size() * sizeof(InCharT)`).
    /// @return Number of bytes spanned by the view.
    [[nodiscard]] constexpr SizeType sizeInBytes() const noexcept
    {
        return m_size * sizeof(InCharT);
    }

    /// @brief Returns the maximum number of characters a view can represent.
    /// @return Maximum representable character count.
    [[nodiscard]] constexpr SizeType maxSize() const noexcept
    {
        return (npos - 1u) / sizeof(InCharT);
    }

    /// @brief Checks whether the view is empty (`size() == 0`).
    /// @return True if empty, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_size == 0u;
    }

    /// @brief Bounds-checked element access.
    /// @param[in] pos Zero-based index of the character.
    /// @return Const reference to the character at `pos`.
    [[nodiscard]] constexpr ConstReference at(const SizeType pos) const noexcept
    {
        GP_ASSERT(pos < m_size, "BasicStringView::at index out of bounds.");
        return m_data[pos];
    }

    /// @brief Returns a const reference to the first character in the view.
    /// @return Const reference to `m_data[0]`.
    [[nodiscard]] constexpr ConstReference front() const noexcept
    {
        GP_ASSERT(!isEmpty(), "BasicStringView::front called on an empty view.");
        return m_data[0];
    }

    /// @brief Returns a const reference to the last character in the view.
    /// @return Const reference to `m_data[m_size - 1]`.
    [[nodiscard]] constexpr ConstReference back() const noexcept
    {
        GP_ASSERT(!isEmpty(), "BasicStringView::back called on an empty view.");
        return m_data[m_size - 1u];
    }

    /// @brief Returns an iterator to the first character.
    /// @return Iterator pointing to the start of the view.
    [[nodiscard]] constexpr Iterator begin() const noexcept
    {
        return m_data;
    }

    /// @brief Returns an iterator one past the last character.
    /// @return Iterator pointing to the end of the view.
    [[nodiscard]] constexpr Iterator end() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Returns a const iterator to the first character.
    /// @return Const iterator pointing to the start of the view.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return m_data;
    }

    /// @brief Returns a const iterator one past the last character.
    /// @return Const iterator pointing to the end of the view.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Returns a reverse iterator to the last character.
    /// @return Reverse iterator pointing to the reverse beginning of the view.
    [[nodiscard]] constexpr ReverseIterator rbegin() const noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a reverse iterator one before the first character.
    /// @return Reverse iterator pointing to the reverse end of the view.
    [[nodiscard]] constexpr ReverseIterator rend() const noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator to the last character.
    /// @return Const reverse iterator pointing to the reverse beginning of the view.
    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(cend());
    }

    /// @brief Returns a const reverse iterator one before the first character.
    /// @return Const reverse iterator pointing to the reverse end of the view.
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(cbegin());
    }

public:
    /// @section Modifiers & Zero-Copy Slicing (`constexpr`)

    /// @brief Shrinks the view by advancing the start pointer forward by `n` characters.
    /// @param[in] n Number of characters to remove from the front (`n <= size()`).
    constexpr void removePrefix(const SizeType n) noexcept
    {
        GP_ASSERT(n <= m_size, "BasicStringView::removePrefix count exceeds size.");
        m_data += n;
        m_size -= n;
    }

    /// @brief Shrinks the view by moving the end backward by `n` characters.
    /// @param[in] n Number of characters to remove from the back (`n <= size()`).
    constexpr void removeSuffix(const SizeType n) noexcept
    {
        GP_ASSERT(n <= m_size, "BasicStringView::removeSuffix count exceeds size.");
        m_size -= n;
    }

    /// @brief Swaps the pointer and length of this view with `other`.
    /// @param[in,out] other The view to swap with.
    constexpr void swap(BasicStringView& other) noexcept
    {
        const ConstPointer tmpData = m_data;
        const SizeType tmpSize = m_size;
        m_data = other.m_data;
        m_size = other.m_size;
        other.m_data = tmpData;
        other.m_size = tmpSize;
    }

    /// @brief Copies a substring of characters to an external buffer (without appending a null terminator).
    /// @param[out] dest Destination buffer.
    /// @param[in] count Maximum number of characters to copy.
    /// @param[in] pos Starting position (`pos <= size()`).
    /// @return Number of characters actually copied.
    constexpr SizeType copy(InCharT* const dest, const SizeType count, const SizeType pos = 0u) const noexcept
    {
        GP_ASSERT(pos <= m_size, "BasicStringView::copy pos out of bounds.");
        const SizeType rcount = gp::math::min(count, m_size - pos);
        TraitsType::copy(dest, m_data + pos, rcount);
        return rcount;
    }

    /// @brief Returns a substring view starting at `pos` of length `min(count, size() - pos)`.
    /// @param[in] pos Starting index (`pos <= size()`).
    /// @param[in] count Maximum number of characters to include.
    /// @return Sub-view over the requested range.
    [[nodiscard]] constexpr BasicStringView substr(const SizeType pos = 0u, const SizeType count = npos) const noexcept
    {
        GP_ASSERT(pos <= m_size, "BasicStringView::substr pos out of bounds.");
        return BasicStringView(m_data + pos, gp::math::min(count, m_size - pos));
    }

    /// @brief Alias for `substr(pos, count)` matching `gp-engine` view terminology.
    /// @param[in] pos Starting index (`pos <= size()`).
    /// @param[in] count Maximum number of characters to include.
    /// @return Sub-view over the requested range.
    [[nodiscard]] constexpr BasicStringView subview(const SizeType pos = 0u, const SizeType count = npos) const noexcept
    {
        return substr(pos, count);
    }

    /// @brief Returns the leftmost `min(count, size())` characters as a view (Unreal `FStringView::Left` parity).
    /// @param[in] count Number of characters from the start.
    /// @return Prefix view of length `min(count, size())`.
    [[nodiscard]] constexpr BasicStringView left(const SizeType count) const noexcept
    {
        const SizeType clamped = gp::math::min(count, m_size);
        return BasicStringView(m_data, clamped);
    }

    /// @brief Returns the rightmost `min(count, size())` characters as a view (Unreal `FStringView::Right` parity).
    /// @param[in] count Number of characters from the end.
    /// @return Suffix view of length `min(count, size())`.
    [[nodiscard]] constexpr BasicStringView right(const SizeType count) const noexcept
    {
        const SizeType clamped = gp::math::min(count, m_size);
        return BasicStringView(m_data + (m_size - clamped), clamped);
    }

    /// @brief Clamped, non-asserting substring view starting at `pos` (Unreal `FStringView::Mid` parity).
    /// @param[in] pos Starting index (returns an empty view if `pos >= size()`).
    /// @param[in] count Maximum number of characters to include.
    /// @return Clamped sub-view.
    [[nodiscard]] constexpr BasicStringView mid(const SizeType pos, const SizeType count = npos) const noexcept
    {
        if (pos >= m_size)
        {
            return BasicStringView{};
        }
        return BasicStringView(m_data + pos, gp::math::min(count, m_size - pos));
    }

    /// @brief Returns a view with `min(count, size())` characters removed from the right (end).
    /// @param[in] count Number of characters to chop from the end.
    /// @return View of the remaining prefix.
    [[nodiscard]] constexpr BasicStringView leftChop(const SizeType count) const noexcept
    {
        const SizeType clamped = gp::math::min(count, m_size);
        return BasicStringView(m_data, m_size - clamped);
    }

    /// @brief Returns a view with `min(count, size())` characters removed from the left (start).
    /// @param[in] count Number of characters to chop from the start.
    /// @return View of the remaining suffix.
    [[nodiscard]] constexpr BasicStringView rightChop(const SizeType count) const noexcept
    {
        const SizeType clamped = gp::math::min(count, m_size);
        return BasicStringView(m_data + clamped, m_size - clamped);
    }

    /// @brief Returns a sub-view with leading ASCII whitespace characters stripped.
    /// @return Trimmed view.
    [[nodiscard]] constexpr BasicStringView trimStart() const noexcept
    {
        SizeType idx = 0u;
        while (idx < m_size && TraitsType::isWhitespace(m_data[idx]))
        {
            ++idx;
        }
        return BasicStringView(m_data + idx, m_size - idx);
    }

    /// @brief Alias for `trimStart()`.
    /// @return Trimmed view.
    [[nodiscard]] constexpr BasicStringView trimStartView() const noexcept
    {
        return trimStart();
    }

    /// @brief Returns a sub-view with trailing ASCII whitespace characters stripped.
    /// @return Trimmed view.
    [[nodiscard]] constexpr BasicStringView trimEnd() const noexcept
    {
        SizeType len = m_size;
        while (len > 0u && TraitsType::isWhitespace(m_data[len - 1u]))
        {
            --len;
        }
        return BasicStringView(m_data, len);
    }

    /// @brief Alias for `trimEnd()`.
    /// @return Trimmed view.
    [[nodiscard]] constexpr BasicStringView trimEndView() const noexcept
    {
        return trimEnd();
    }

    /// @brief Returns a sub-view with both leading and trailing ASCII whitespace characters stripped.
    /// @return Trimmed view.
    [[nodiscard]] constexpr BasicStringView trim() const noexcept
    {
        return trimStart().trimEnd();
    }

    /// @brief Alias for `trim()`.
    /// @return Trimmed view.
    [[nodiscard]] constexpr BasicStringView trimView() const noexcept
    {
        return trim();
    }

public:
    /// @section Comparisons & Case-Aware Predicates (`constexpr`)

    /// @brief Checks whether this view equals `other` under the specified `SearchCase` mode.
    /// @param[in] other View to compare against.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if equal under `searchCase`, false otherwise.
    [[nodiscard]] constexpr bool
        equals(const BasicStringView other, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return m_size == other.m_size && TraitsType::equals(m_data, other.m_data, m_size, searchCase);
    }

    /// @brief Checks whether this view equals `other` using ASCII case-insensitive comparison.
    /// @param[in] other View to compare against.
    /// @return True if equal case-insensitively, false otherwise.
    [[nodiscard]] constexpr bool equalsIgnoreCase(const BasicStringView other) const noexcept
    {
        return equals(other, SearchCase::IgnoreCase);
    }

    /// @brief Compares this view with `other` under the specified `SearchCase` mode.
    /// @param[in] other The view to compare against.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Negative if less, `0` if equal, positive if greater.
    [[nodiscard]] constexpr gp::Int32
        compare(const BasicStringView other, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        const SizeType len = gp::math::min(m_size, other.m_size);
        const gp::Int32 result = TraitsType::compare(m_data, other.m_data, len, searchCase);
        if (result != 0)
        {
            return result;
        }
        if (m_size < other.m_size)
        {
            return -1;
        }
        if (m_size > other.m_size)
        {
            return 1;
        }
        return 0;
    }

    /// @brief Compares this view with `other` using ASCII case-insensitive comparison.
    /// @param[in] other The view to compare against.
    /// @return Negative if less, `0` if equal, positive if greater.
    [[nodiscard]] constexpr gp::Int32 compareIgnoreCase(const BasicStringView other) const noexcept
    {
        return compare(other, SearchCase::IgnoreCase);
    }

    /// @brief Compares a substring of this view with `other`.
    /// @param[in] pos Starting position in this view.
    /// @param[in] count Number of characters in this view's substring.
    /// @param[in] other The view to compare against.
    /// @return Negative if less, `0` if equal, positive if greater.
    [[nodiscard]] constexpr gp::Int32
        compare(const SizeType pos, const SizeType count, const BasicStringView other) const noexcept
    {
        return substr(pos, count).compare(other);
    }

    /// @brief Compares a substring of this view with a substring of `other`.
    /// @param[in] pos1 Starting position in this view.
    /// @param[in] count1 Number of characters in this view's substring.
    /// @param[in] other The other view.
    /// @param[in] pos2 Starting position in `other`.
    /// @param[in] count2 Number of characters in `other`'s substring.
    /// @return Negative if less, `0` if equal, positive if greater.
    [[nodiscard]] constexpr gp::Int32 compare(
        const SizeType pos1,
        const SizeType count1,
        const BasicStringView other,
        const SizeType pos2,
        const SizeType count2
    ) const noexcept
    {
        return substr(pos1, count1).compare(other.substr(pos2, count2));
    }

    /// @brief Compares this view with a null-terminated C string.
    /// @param[in] str Null-terminated C string.
    /// @return Negative if less, `0` if equal, positive if greater.
    [[nodiscard]] constexpr gp::Int32 compare(const InCharT* const str) const noexcept
    {
        return compare(BasicStringView(str));
    }

    /// @brief Compares a substring of this view with a null-terminated C string.
    /// @param[in] pos Starting position in this view.
    /// @param[in] count Number of characters in this view's substring.
    /// @param[in] str Null-terminated C string.
    /// @return Negative if less, `0` if equal, positive if greater.
    [[nodiscard]] constexpr gp::Int32
        compare(const SizeType pos, const SizeType count, const InCharT* const str) const noexcept
    {
        return substr(pos, count).compare(BasicStringView(str));
    }

    /// @brief Checks whether this view begins with `prefix`.
    /// @param[in] prefix Prefix view to test.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if this view starts with `prefix`, false otherwise.
    [[nodiscard]] constexpr bool
        startsWith(const BasicStringView prefix, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return m_size >= prefix.m_size && TraitsType::equals(m_data, prefix.m_data, prefix.m_size, searchCase);
    }

    /// @brief Checks whether this view begins with character `ch`.
    /// @param[in] ch Character to test.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if non-empty and `front()` matches `ch`, false otherwise.
    [[nodiscard]] constexpr bool
        startsWith(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return !isEmpty() && TraitsType::eq(front(), ch, searchCase);
    }

    /// @brief Checks whether this view begins with null-terminated C string `str`.
    /// @param[in] str Null-terminated C string prefix.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if this view starts with `str`, false otherwise.
    [[nodiscard]] constexpr bool
        startsWith(const InCharT* const str, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return startsWith(BasicStringView(str), searchCase);
    }

    /// @brief Checks whether this view ends with `suffix`.
    /// @param[in] suffix Suffix view to test.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if this view ends with `suffix`, false otherwise.
    [[nodiscard]] constexpr bool
        endsWith(const BasicStringView suffix, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return m_size >= suffix.m_size &&
               TraitsType::equals(m_data + (m_size - suffix.m_size), suffix.m_data, suffix.m_size, searchCase);
    }

    /// @brief Checks whether this view ends with character `ch`.
    /// @param[in] ch Character to test.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if non-empty and `back()` matches `ch`, false otherwise.
    [[nodiscard]] constexpr bool
        endsWith(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return !isEmpty() && TraitsType::eq(back(), ch, searchCase);
    }

    /// @brief Checks whether this view ends with null-terminated C string `str`.
    /// @param[in] str Null-terminated C string suffix.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if this view ends with `str`, false otherwise.
    [[nodiscard]] constexpr bool
        endsWith(const InCharT* const str, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return endsWith(BasicStringView(str), searchCase);
    }

    /// @brief Checks whether this view contains substring `sv`.
    /// @param[in] sv Substring to search for.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if `sv` occurs within this view, false otherwise.
    [[nodiscard]] constexpr bool
        contains(const BasicStringView sv, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return find(sv, 0u, searchCase) != npos;
    }

    /// @brief Checks whether this view contains character `ch`.
    /// @param[in] ch Character to search for.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if `ch` occurs within this view, false otherwise.
    [[nodiscard]] constexpr bool
        contains(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return find(ch, 0u, searchCase) != npos;
    }

    /// @brief Checks whether this view contains null-terminated C string `str`.
    /// @param[in] str Null-terminated C string to search for.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if `str` occurs within this view, false otherwise.
    [[nodiscard]] constexpr bool
        contains(const InCharT* const str, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return contains(BasicStringView(str), searchCase);
    }

public:
    /// @section Substring & Character Set Search (`constexpr`)

    /// @brief Finds the first occurrence of substring `sv` starting at or after `pos`.
    /// @param[in] sv Substring to search for.
    /// @param[in] pos Starting index (defaults to `0`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType find(
        const BasicStringView sv, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (pos > m_size)
        {
            return npos;
        }
        if (sv.isEmpty())
        {
            return pos;
        }

        const SizeType rel = TraitsType::findSubrange(m_data + pos, m_size - pos, sv.m_data, sv.m_size, searchCase);
        return rel == npos ? npos : (pos + rel);
    }

    /// @brief Finds the first occurrence of character `ch` starting at or after `pos`.
    /// @param[in] ch Character to search for.
    /// @param[in] pos Starting index (defaults to `0`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType find(
        const InCharT ch, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (pos >= m_size)
        {
            return npos;
        }
        const InCharT* const found = TraitsType::find(m_data + pos, m_size - pos, ch, searchCase);
        return found != nullptr ? static_cast<SizeType>(found - m_data) : npos;
    }

    /// @brief Finds the first occurrence of C string `str` starting at or after `pos`.
    /// @param[in] str Null-terminated C string to search for.
    /// @param[in] pos Starting index (defaults to `0`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType find(
        const InCharT* const str, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return find(BasicStringView(str), pos, searchCase);
    }

    /// @brief Finds the first occurrence of buffer `[str, str + count)` starting at or after `pos`.
    /// @param[in] str Pointer to the character array to search for.
    /// @param[in] pos Starting index.
    /// @param[in] count Number of characters in `str`.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType find(
        const InCharT* const str,
        const SizeType pos,
        const SizeType count,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return find(BasicStringView(str, count), pos, searchCase);
    }

    /// @brief Finds the last occurrence of substring `sv` starting at or before `pos`.
    /// @param[in] sv Substring to search for.
    /// @param[in] pos Maximum starting index for the match (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType rfind(
        const BasicStringView sv, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (sv.isEmpty())
        {
            return gp::math::min(pos, m_size);
        }
        if (sv.m_size > m_size)
        {
            return npos;
        }

        const SizeType clampedPos = gp::math::min(pos, m_size - sv.m_size);
        const SizeType searchLen = clampedPos + sv.m_size;
        return TraitsType::rfindSubrange(m_data, searchLen, sv.m_data, sv.m_size, searchCase);
    }

    /// @brief Finds the last occurrence of character `ch` at or before `pos`.
    /// @param[in] ch Character to search for.
    /// @param[in] pos Maximum index to inspect (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType rfind(
        const InCharT ch, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (m_size == 0u)
        {
            return npos;
        }
        const SizeType clampedPos = gp::math::min(pos, m_size - 1u);
        const InCharT* const found = TraitsType::rfind(m_data, clampedPos + 1u, ch, searchCase);
        return found != nullptr ? static_cast<SizeType>(found - m_data) : npos;
    }

    /// @brief Finds the last occurrence of C string `str` starting at or before `pos`.
    /// @param[in] str Null-terminated C string to search for.
    /// @param[in] pos Maximum starting index for the match (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType rfind(
        const InCharT* const str, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return rfind(BasicStringView(str), pos, searchCase);
    }

    /// @brief Finds the last occurrence of buffer `[str, str + count)` starting at or before `pos`.
    /// @param[in] str Pointer to the character array to search for.
    /// @param[in] pos Maximum starting index for the match.
    /// @param[in] count Number of characters in `str`.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType rfind(
        const InCharT* const str,
        const SizeType pos,
        const SizeType count,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return rfind(BasicStringView(str, count), pos, searchCase);
    }

    /// @brief Finds an occurrence of substring `sv` in the specified `searchDir` (`FromStart` or `FromEnd`).
    /// @param[in] sv Substring to search for.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @param[in] searchDir Search direction (`SearchDir::FromStart` or `SearchDir::FromEnd`).
    /// @param[in] startPos Starting position (`0` for `FromStart`, `npos` for `FromEnd` when omitted).
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType indexOf(
        const BasicStringView sv,
        const SearchCase searchCase = SearchCase::CaseSensitive,
        const SearchDir searchDir = SearchDir::FromStart,
        const SizeType startPos = npos
    ) const noexcept
    {
        if (searchDir == SearchDir::FromStart)
        {
            return find(sv, startPos == npos ? 0u : startPos, searchCase);
        }
        return rfind(sv, startPos, searchCase);
    }

    /// @brief Finds an occurrence of character `ch` in the specified `searchDir` (`FromStart` or `FromEnd`).
    /// @param[in] ch Character to search for.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @param[in] searchDir Search direction (`SearchDir::FromStart` or `SearchDir::FromEnd`).
    /// @param[in] startPos Starting position (`0` for `FromStart`, `npos` for `FromEnd` when omitted).
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType indexOf(
        const InCharT ch,
        const SearchCase searchCase = SearchCase::CaseSensitive,
        const SearchDir searchDir = SearchDir::FromStart,
        const SizeType startPos = npos
    ) const noexcept
    {
        if (searchDir == SearchDir::FromStart)
        {
            return find(ch, startPos == npos ? 0u : startPos, searchCase);
        }
        return rfind(ch, startPos, searchCase);
    }

    /// @brief Finds the first character in this view (from `pos`) that matches any character in `sv`.
    /// @param[in] sv Set of characters to match against.
    /// @param[in] pos Position to start searching from.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first matching character, or `npos` if none match.
    [[nodiscard]] constexpr SizeType findFirstOf(
        const BasicStringView sv, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (pos >= m_size || sv.isEmpty())
        {
            return npos;
        }
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (TraitsType::find(sv.m_data, sv.m_size, m_data[i], searchCase) != nullptr)
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the first occurrence of `ch` starting at `pos`.
    /// @param[in] ch Character to match.
    /// @param[in] pos Position to start searching from.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType findFirstOf(
        const InCharT ch, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return find(ch, pos, searchCase);
    }

    /// @brief Finds the first character in this view (from `pos`) that matches any character in C string `str`.
    /// @param[in] str Null-terminated C string of characters to match.
    /// @param[in] pos Position to start searching from.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first matching character, or `npos` if none match.
    [[nodiscard]] constexpr SizeType findFirstOf(
        const InCharT* const str, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findFirstOf(BasicStringView(str), pos, searchCase);
    }

    /// @brief Finds the first character in this view (from `pos`) that matches any character in `[str, str + count)`.
    /// @param[in] str Buffer of characters to match.
    /// @param[in] pos Position to start searching from.
    /// @param[in] count Number of characters in `str`.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first matching character, or `npos` if none match.
    [[nodiscard]] constexpr SizeType findFirstOf(
        const InCharT* const str,
        const SizeType pos,
        const SizeType count,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findFirstOf(BasicStringView(str, count), pos, searchCase);
    }

    /// @brief Finds the last character in this view (at or before `pos`) that matches any character in `sv`.
    /// @param[in] sv Set of characters to match against.
    /// @param[in] pos Maximum index to inspect (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last matching character, or `npos` if none match.
    [[nodiscard]] constexpr SizeType findLastOf(
        const BasicStringView sv, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (m_size == 0u || sv.isEmpty())
        {
            return npos;
        }
        const SizeType clampedPos = gp::math::min(pos, m_size - 1u);
        for (SizeType i = clampedPos + 1u; i > 0u; --i)
        {
            if (TraitsType::find(sv.m_data, sv.m_size, m_data[i - 1u], searchCase) != nullptr)
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Finds the last occurrence of `ch` at or before `pos`.
    /// @param[in] ch Character to match.
    /// @param[in] pos Maximum index to inspect (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType findLastOf(
        const InCharT ch, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return rfind(ch, pos, searchCase);
    }

    /// @brief Finds the last character in this view (at or before `pos`) that matches any character in C string `str`.
    /// @param[in] str Null-terminated C string of characters to match.
    /// @param[in] pos Maximum index to inspect (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last matching character, or `npos` if none match.
    [[nodiscard]] constexpr SizeType findLastOf(
        const InCharT* const str, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findLastOf(BasicStringView(str), pos, searchCase);
    }

    /// @brief Finds the last character in this view (at or before `pos`) that matches any character in `[str, str +
    /// count)`.
    /// @param[in] str Buffer of characters to match.
    /// @param[in] pos Maximum index to inspect.
    /// @param[in] count Number of characters in `str`.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last matching character, or `npos` if none match.
    [[nodiscard]] constexpr SizeType findLastOf(
        const InCharT* const str,
        const SizeType pos,
        const SizeType count,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findLastOf(BasicStringView(str, count), pos, searchCase);
    }

    /// @brief Finds the first character in this view (from `pos`) that does NOT match any character in `sv`.
    /// @param[in] sv Set of characters to exclude.
    /// @param[in] pos Position to start searching from.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findFirstNotOf(
        const BasicStringView sv, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (pos >= m_size)
        {
            return npos;
        }
        if (sv.isEmpty())
        {
            return pos;
        }
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (TraitsType::find(sv.m_data, sv.m_size, m_data[i], searchCase) == nullptr)
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the first character in this view (from `pos`) that is not equal to `ch`.
    /// @param[in] ch Character to exclude.
    /// @param[in] pos Position to start searching from.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findFirstNotOf(
        const InCharT ch, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (pos >= m_size)
        {
            return npos;
        }
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (!TraitsType::eq(m_data[i], ch, searchCase))
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the first character in this view (from `pos`) that does not match any character in C string `str`.
    /// @param[in] str Null-terminated C string of characters to exclude.
    /// @param[in] pos Position to start searching from.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findFirstNotOf(
        const InCharT* const str, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findFirstNotOf(BasicStringView(str), pos, searchCase);
    }

    /// @brief Finds the first character in this view (from `pos`) that does not match any character in `[str, str +
    /// count)`.
    /// @param[in] str Buffer of characters to exclude.
    /// @param[in] pos Position to start searching from.
    /// @param[in] count Number of characters in `str`.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the first non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findFirstNotOf(
        const InCharT* const str,
        const SizeType pos,
        const SizeType count,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findFirstNotOf(BasicStringView(str, count), pos, searchCase);
    }

    /// @brief Finds the last character in this view (at or before `pos`) that does NOT match any character in `sv`.
    /// @param[in] sv Set of characters to exclude.
    /// @param[in] pos Maximum index to inspect (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findLastNotOf(
        const BasicStringView sv, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (m_size == 0u)
        {
            return npos;
        }
        const SizeType clampedPos = gp::math::min(pos, m_size - 1u);
        if (sv.isEmpty())
        {
            return clampedPos;
        }
        for (SizeType i = clampedPos + 1u; i > 0u; --i)
        {
            if (TraitsType::find(sv.m_data, sv.m_size, m_data[i - 1u], searchCase) == nullptr)
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Finds the last character in this view (at or before `pos`) that is not equal to `ch`.
    /// @param[in] ch Character to exclude.
    /// @param[in] pos Maximum index to inspect (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findLastNotOf(
        const InCharT ch, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (m_size == 0u)
        {
            return npos;
        }
        const SizeType clampedPos = gp::math::min(pos, m_size - 1u);
        for (SizeType i = clampedPos + 1u; i > 0u; --i)
        {
            if (!TraitsType::eq(m_data[i - 1u], ch, searchCase))
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Finds the last character in this view (at or before `pos`) that does not match any character in `str`.
    /// @param[in] str Null-terminated C string of characters to exclude.
    /// @param[in] pos Maximum index to inspect (defaults to `npos`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findLastNotOf(
        const InCharT* const str, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findLastNotOf(BasicStringView(str), pos, searchCase);
    }

    /// @brief Finds the last character in this view (at or before `pos`) that does not match `[str, str + count)`.
    /// @param[in] str Buffer of characters to exclude.
    /// @param[in] pos Maximum index to inspect.
    /// @param[in] count Number of characters in `str`.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Index of the last non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findLastNotOf(
        const InCharT* const str,
        const SizeType pos,
        const SizeType count,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return findLastNotOf(BasicStringView(str, count), pos, searchCase);
    }

public:
    /// @section AAA Engine Utilities: Occurrence Counting, Wildcards, Splitting, Hashing & Parsing

    /// @brief Counts the number of non-overlapping occurrences of `needle` inside this view.
    /// @param[in] needle Substring to count.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Number of non-overlapping occurrences (`0` if `needle` is empty).
    [[nodiscard]] constexpr SizeType countOccurrences(
        const BasicStringView needle, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (needle.isEmpty() || needle.m_size > m_size)
        {
            return 0u;
        }

        SizeType count = 0u;
        SizeType pos = 0u;
        while ((pos = find(needle, pos, searchCase)) != npos)
        {
            ++count;
            pos += needle.m_size;
        }
        return count;
    }

    /// @brief Counts the number of occurrences of character `ch` inside this view.
    /// @param[in] ch Character to count.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return Number of matching characters.
    [[nodiscard]] constexpr SizeType
        countOccurrences(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        SizeType count = 0u;
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (TraitsType::eq(m_data[i], ch, searchCase))
            {
                ++count;
            }
        }
        return count;
    }

    /// @brief Tests whether this view matches a wildcard glob pattern containing `'*'` (0+ chars) and `'?'` (1 char).
    /// @details Uses a non-recursive dual-cursor backtracking algorithm with $O(1)$ stack space and zero allocations.
    /// @param[in] pattern Wildcard pattern (e.g., `"*.uasset"`, `"Hero_??_LOD*"`).
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return True if the entire view matches `pattern`, false otherwise.
    [[nodiscard]] constexpr bool matchesWildcard(
        const BasicStringView pattern, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        SizeType strIdx = 0u;
        SizeType patIdx = 0u;
        SizeType starPatIdx = npos;
        SizeType starMatchStrIdx = 0u;

        while (strIdx < m_size)
        {
            if (patIdx < pattern.m_size && (pattern.m_data[patIdx] == static_cast<InCharT>('?') ||
                                            TraitsType::eq(m_data[strIdx], pattern.m_data[patIdx], searchCase)))
            {
                ++strIdx;
                ++patIdx;
            }
            else if (patIdx < pattern.m_size && pattern.m_data[patIdx] == static_cast<InCharT>('*'))
            {
                starPatIdx = patIdx;
                starMatchStrIdx = strIdx;
                ++patIdx;
            }
            else if (starPatIdx != npos)
            {
                patIdx = starPatIdx + 1u;
                ++starMatchStrIdx;
                strIdx = starMatchStrIdx;
            }
            else
            {
                return false;
            }
        }

        while (patIdx < pattern.m_size && pattern.m_data[patIdx] == static_cast<InCharT>('*'))
        {
            ++patIdx;
        }

        return patIdx == pattern.m_size;
    }

    /// @brief Splits this view by substring `delimiter` and invokes `callback(BasicStringView)` per token with zero
    ///        memory allocations.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Delimiter substring (if empty, emits the entire view as a single token).
    /// @param[in] callback Function or lambda invoked for each token.
    /// @param[in] options Bitmask of `StringSplitOptions` (`SkipEmpty`, `TrimTokens`).
    /// @param[in] searchCase Case sensitivity mode when matching `delimiter`.
    /// @return Number of tokens emitted to `callback`.
    template <typename InCallback>
    constexpr SizeType splitView(
        const BasicStringView delimiter,
        InCallback&& callback,
        const StringSplitOptions options = StringSplitOptions::None,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const
    {
        const bool shouldTrim = gp::enums::hasAnyFlags(options & StringSplitOptions::TrimTokens);
        const bool shouldSkipEmpty = gp::enums::hasAnyFlags(options & StringSplitOptions::SkipEmpty);

        SizeType emittedCount = 0u;
        const auto emitToken = [&callback, &emittedCount, shouldTrim, shouldSkipEmpty](BasicStringView token) -> void
        {
            if (shouldTrim)
            {
                token = token.trim();
            }
            if (shouldSkipEmpty && token.isEmpty())
            {
                return;
            }
            callback(token);
            ++emittedCount;
        };

        if (m_size == 0u)
        {
            emitToken(BasicStringView{});
            return emittedCount;
        }

        if (delimiter.isEmpty())
        {
            emitToken(*this);
            return emittedCount;
        }

        SizeType start = 0u;
        while (start <= m_size)
        {
            const SizeType delimPos = find(delimiter, start, searchCase);
            if (delimPos == npos)
            {
                emitToken(substr(start));
                break;
            }

            emitToken(substr(start, delimPos - start));
            start = delimPos + delimiter.m_size;
        }

        return emittedCount;
    }

    /// @brief Splits this view by character `delimiter` and invokes `callback(BasicStringView)` per token.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Single character delimiter.
    /// @param[in] callback Function or lambda invoked for each token.
    /// @param[in] options Bitmask of `StringSplitOptions` (`SkipEmpty`, `TrimTokens`).
    /// @param[in] searchCase Case sensitivity mode when matching `delimiter`.
    /// @return Number of tokens emitted to `callback`.
    template <typename InCallback>
    requires(!std::is_same_v<std::remove_cvref_t<InCallback>, StringSplitOptions>) constexpr SizeType splitView(
        const InCharT delimiter,
        InCallback&& callback,
        const StringSplitOptions options = StringSplitOptions::None,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const
    {
        return splitView(BasicStringView(&delimiter, 1u), static_cast<InCallback&&>(callback), options, searchCase);
    }

    /// @brief Splits this view by character `delimiter` with `options` preceding `callback`.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Single character delimiter.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @param[in] callback Function or lambda invoked for each token.
    /// @return Number of tokens emitted to `callback`.
    template <typename InCallback>
    constexpr SizeType splitView(const InCharT delimiter, const StringSplitOptions options, InCallback&& callback) const
    {
        return splitView(
            BasicStringView(&delimiter, 1u), static_cast<InCallback&&>(callback), options, SearchCase::CaseSensitive
        );
    }

    /// @brief Splits this view by substring `delimiter` with `options` and `searchCase` preceding `callback`.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Delimiter substring.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @param[in] searchCase Case sensitivity mode when matching `delimiter`.
    /// @param[in] callback Function or lambda invoked for each token.
    /// @return Number of tokens emitted to `callback`.
    template <typename InCallback>
    constexpr SizeType splitView(
        const BasicStringView delimiter,
        const StringSplitOptions options,
        const SearchCase searchCase,
        InCallback&& callback
    ) const
    {
        return splitView(delimiter, static_cast<InCallback&&>(callback), options, searchCase);
    }

    /// @brief Splits this view into lines (`\n`, `\r\n`, or `\r`) and invokes `callback(BasicStringView)` per line.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] callback Function or lambda invoked for each line.
    /// @param[in] options Bitmask of `StringSplitOptions` (`SkipEmpty`, `TrimTokens`).
    /// @return Number of lines emitted to `callback`.
    template <typename InCallback>
    requires(!std::is_same_v<std::remove_cvref_t<InCallback>, StringSplitOptions>) constexpr SizeType
        splitLines(InCallback&& callback, const StringSplitOptions options = StringSplitOptions::None) const
    {
        const bool shouldTrim = gp::enums::hasAnyFlags(options & StringSplitOptions::TrimTokens);
        const bool shouldSkipEmpty = gp::enums::hasAnyFlags(options & StringSplitOptions::SkipEmpty);

        SizeType emittedCount = 0u;
        const auto emitLine = [&callback, &emittedCount, shouldTrim, shouldSkipEmpty](BasicStringView line) -> void
        {
            if (shouldTrim)
            {
                line = line.trim();
            }
            if (shouldSkipEmpty && line.isEmpty())
            {
                return;
            }
            callback(line);
            ++emittedCount;
        };

        SizeType start = 0u;
        SizeType i = 0u;
        while (i < m_size)
        {
            if (m_data[i] == static_cast<InCharT>('\r') || m_data[i] == static_cast<InCharT>('\n'))
            {
                emitLine(substr(start, i - start));
                if (m_data[i] == static_cast<InCharT>('\r') && (i + 1u) < m_size &&
                    m_data[i + 1u] == static_cast<InCharT>('\n'))
                {
                    i += 2u;
                }
                else
                {
                    ++i;
                }
                start = i;
            }
            else
            {
                ++i;
            }
        }

        if (start < m_size || m_size == 0u)
        {
            emitLine(substr(start));
        }

        return emittedCount;
    }

    /// @brief Splits this view into lines (`\n`, `\r\n`, or `\r`) with `options` preceding `callback`.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @param[in] callback Function or lambda invoked for each line.
    /// @return Number of lines emitted to `callback`.
    template <typename InCallback>
    constexpr SizeType splitLines(const StringSplitOptions options, InCallback&& callback) const
    {
        return splitLines(static_cast<InCallback&&>(callback), options);
    }

    /// @brief Computes a 64-bit FNV-1a hash of this view.
    /// @param[in] searchCase Case sensitivity mode (defaults to `SearchCase::CaseSensitive`).
    /// @return 64-bit FNV-1a hash value.
    [[nodiscard]] constexpr gp::UInt64 hash(const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return TraitsType::hash(m_data, m_size, searchCase);
    }

    /// @brief Computes a 64-bit FNV-1a ASCII case-insensitive hash of this view.
    /// @return 64-bit FNV-1a case-insensitive hash value.
    [[nodiscard]] constexpr gp::UInt64 hashIgnoreCase() const noexcept
    {
        return TraitsType::hashIgnoreCase(m_data, m_size);
    }

    /// @brief Parses this view as a signed 32-bit integer (`gp::Int32`) in base 10 with zero heap allocations.
    /// @return `Optional<gp::Int32>` containing the parsed value, or `nullopt` if invalid or out of range.
    [[nodiscard]] Optional<gp::Int32> tryParseInt32() const noexcept
    {
        return tryParseIntegralInternal<gp::Int32>(10);
    }

    /// @brief Parses this view as a signed 32-bit integer into `outValue`.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric radix in `[2, 36]` (defaults to `10`).
    /// @return True if parsing succeeded, false otherwise.
    [[nodiscard]] bool tryParseInt32(gp::Int32& outValue, const gp::Int32 base = 10) const noexcept
    {
        const Optional<gp::Int32> res = tryParseIntegralInternal<gp::Int32>(base);
        if (res.hasValue())
        {
            outValue = *res;
            return true;
        }
        return false;
    }

    /// @brief Parses this view as a signed 64-bit integer (`gp::Int64`) in base 10 with zero heap allocations.
    /// @return `Optional<gp::Int64>` containing the parsed value, or `nullopt` if invalid or out of range.
    [[nodiscard]] Optional<gp::Int64> tryParseInt64() const noexcept
    {
        return tryParseIntegralInternal<gp::Int64>(10);
    }

    /// @brief Parses this view as a signed 64-bit integer into `outValue`.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric radix in `[2, 36]` (defaults to `10`).
    /// @return True if parsing succeeded, false otherwise.
    [[nodiscard]] bool tryParseInt64(gp::Int64& outValue, const gp::Int32 base = 10) const noexcept
    {
        const Optional<gp::Int64> res = tryParseIntegralInternal<gp::Int64>(base);
        if (res.hasValue())
        {
            outValue = *res;
            return true;
        }
        return false;
    }

    /// @brief Parses this view as an unsigned 32-bit integer (`gp::UInt32`) in base 10 with zero heap allocations.
    /// @return `Optional<gp::UInt32>` containing the parsed value, or `nullopt` if invalid or out of range.
    [[nodiscard]] Optional<gp::UInt32> tryParseUInt32() const noexcept
    {
        return tryParseIntegralInternal<gp::UInt32>(10);
    }

    /// @brief Parses this view as an unsigned 32-bit integer into `outValue`.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric radix in `[2, 36]` (defaults to `10`).
    /// @return True if parsing succeeded, false otherwise.
    [[nodiscard]] bool tryParseUInt32(gp::UInt32& outValue, const gp::Int32 base = 10) const noexcept
    {
        const Optional<gp::UInt32> res = tryParseIntegralInternal<gp::UInt32>(base);
        if (res.hasValue())
        {
            outValue = *res;
            return true;
        }
        return false;
    }

    /// @brief Parses this view as an unsigned 64-bit integer (`gp::UInt64`) in base 10 with zero heap allocations.
    /// @return `Optional<gp::UInt64>` containing the parsed value, or `nullopt` if invalid or out of range.
    [[nodiscard]] Optional<gp::UInt64> tryParseUInt64() const noexcept
    {
        return tryParseIntegralInternal<gp::UInt64>(10);
    }

    /// @brief Parses this view as an unsigned 64-bit integer into `outValue`.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric radix in `[2, 36]` (defaults to `10`).
    /// @return True if parsing succeeded, false otherwise.
    [[nodiscard]] bool tryParseUInt64(gp::UInt64& outValue, const gp::Int32 base = 10) const noexcept
    {
        const Optional<gp::UInt64> res = tryParseIntegralInternal<gp::UInt64>(base);
        if (res.hasValue())
        {
            outValue = *res;
            return true;
        }
        return false;
    }

    /// @brief Parses this view as a 32-bit IEEE-754 floating-point number (`gp::Float32`) with zero heap allocations.
    /// @return `Optional<gp::Float32>` containing the parsed value, or `nullopt` if invalid.
    [[nodiscard]] Optional<gp::Float32> tryParseFloat32() const noexcept
    {
        return tryParseFloatInternal<gp::Float32>();
    }

    /// @brief Parses this view as a 32-bit floating-point number into `outValue`.
    /// @param[out] outValue Receives the parsed value on success.
    /// @return True if parsing succeeded, false otherwise.
    [[nodiscard]] bool tryParseFloat32(gp::Float32& outValue) const noexcept
    {
        const Optional<gp::Float32> res = tryParseFloat32();
        if (res.hasValue())
        {
            outValue = *res;
            return true;
        }
        return false;
    }

    /// @brief Parses this view as a 64-bit IEEE-754 floating-point number (`gp::Float64`) with zero heap allocations.
    /// @return `Optional<gp::Float64>` containing the parsed value, or `nullopt` if invalid.
    [[nodiscard]] Optional<gp::Float64> tryParseFloat64() const noexcept
    {
        return tryParseFloatInternal<gp::Float64>();
    }

    /// @brief Parses this view as a 64-bit floating-point number into `outValue`.
    /// @param[out] outValue Receives the parsed value on success.
    /// @return True if parsing succeeded, false otherwise.
    [[nodiscard]] bool tryParseFloat64(gp::Float64& outValue) const noexcept
    {
        const Optional<gp::Float64> res = tryParseFloat64();
        if (res.hasValue())
        {
            outValue = *res;
            return true;
        }
        return false;
    }

    /// @brief Parses this view as a boolean (`"true"`, `"false"`, `"1"`, `"0"`, `"yes"`, `"no"`, `"on"`, `"off"`).
    /// @return `Optional<bool>` containing the parsed boolean, or `nullopt` if unrecognized.
    [[nodiscard]] constexpr Optional<bool> tryParseBool() const noexcept
    {
        const BasicStringView trimmed = trim();
        if (trimmed.isEmpty() || trimmed.m_size > 5u)
        {
            return nullopt;
        }

        char asciiBuf[6]{};
        for (SizeType i = 0u; i < trimmed.m_size; ++i)
        {
            if (!TraitsType::isAscii(trimmed.m_data[i]))
            {
                return nullopt;
            }
            asciiBuf[i] = static_cast<char>(TraitsType::toLower(trimmed.m_data[i]));
        }

        const BasicStringView<char> asciiView(asciiBuf, trimmed.m_size);
        if (asciiView == "true" || asciiView == "1" || asciiView == "yes" || asciiView == "on")
        {
            return Optional<bool>(true);
        }
        if (asciiView == "false" || asciiView == "0" || asciiView == "no" || asciiView == "off")
        {
            return Optional<bool>(false);
        }
        return nullopt;
    }

    /// @brief Parses this view as a boolean into `outValue`.
    /// @param[out] outValue Receives the parsed boolean on success.
    /// @return True if recognized, false otherwise.
    [[nodiscard]] constexpr bool tryParseBool(bool& outValue) const noexcept
    {
        const Optional<bool> res = tryParseBool();
        if (res.hasValue())
        {
            outValue = *res;
            return true;
        }
        return false;
    }

private:
    template <typename InIntType>
    [[nodiscard]] Optional<InIntType> tryParseIntegralInternal(const gp::Int32 base) const noexcept
    {
        if (base < 2 || base > 36)
        {
            return nullopt;
        }

        BasicStringView trimmed = trim();
        if (trimmed.isEmpty())
        {
            return nullopt;
        }

        // Strip optional leading '+' if not followed by '-'
        if (trimmed.front() == static_cast<InCharT>('+') && trimmed.m_size > 1u &&
            trimmed[1u] != static_cast<InCharT>('-'))
        {
            trimmed.removePrefix(1u);
        }

        // Strip optional "0x" / "0X" prefix for base 16
        if (base == 16 && trimmed.m_size > 2u && trimmed[0u] == static_cast<InCharT>('0') &&
            (trimmed[1u] == static_cast<InCharT>('x') || trimmed[1u] == static_cast<InCharT>('X')))
        {
            trimmed.removePrefix(2u);
        }

        if constexpr (std::is_same_v<InCharT, char>)
        {
            InIntType parsedValue{ 0 };
            const char* const first = trimmed.m_data;
            const char* const last = trimmed.m_data + trimmed.m_size;
            const std::from_chars_result res = std::from_chars(first, last, parsedValue, static_cast<int>(base));
            if (res.ec == std::errc{} && res.ptr == last)
            {
                return Optional<InIntType>(parsedValue);
            }
            return nullopt;
        }
        else
        {
            constexpr SizeType kMaxAsciiDigits = 72u;
            if (trimmed.m_size >= kMaxAsciiDigits)
            {
                return nullopt;
            }
            char asciiBuf[kMaxAsciiDigits]{};
            for (SizeType i = 0u; i < trimmed.m_size; ++i)
            {
                if (!TraitsType::isAscii(trimmed.m_data[i]))
                {
                    return nullopt;
                }
                asciiBuf[i] = static_cast<char>(trimmed.m_data[i]);
            }
            InIntType parsedValue{ 0 };
            const char* const first = asciiBuf;
            const char* const last = asciiBuf + trimmed.m_size;
            const std::from_chars_result res = std::from_chars(first, last, parsedValue, static_cast<int>(base));
            if (res.ec == std::errc{} && res.ptr == last)
            {
                return Optional<InIntType>(parsedValue);
            }
            return nullopt;
        }
    }

    template <typename InFloatType>
    [[nodiscard]] Optional<InFloatType> tryParseFloatInternal() const noexcept
    {
        BasicStringView trimmed = trim();
        if (trimmed.isEmpty())
        {
            return nullopt;
        }

        if (trimmed.front() == static_cast<InCharT>('+') && trimmed.m_size > 1u &&
            trimmed[1u] != static_cast<InCharT>('-'))
        {
            trimmed.removePrefix(1u);
        }

        if constexpr (std::is_same_v<InCharT, char>)
        {
            InFloatType parsedValue{ 0 };
            const char* const first = trimmed.m_data;
            const char* const last = trimmed.m_data + trimmed.m_size;
            const std::from_chars_result res = std::from_chars(first, last, parsedValue);
            if (res.ec == std::errc{} && res.ptr == last)
            {
                return Optional<InFloatType>(parsedValue);
            }
            return nullopt;
        }
        else
        {
            constexpr SizeType kMaxAsciiChars = 128u;
            if (trimmed.m_size >= kMaxAsciiChars)
            {
                return nullopt;
            }
            char asciiBuf[kMaxAsciiChars]{};
            for (SizeType i = 0u; i < trimmed.m_size; ++i)
            {
                if (!TraitsType::isAscii(trimmed.m_data[i]))
                {
                    return nullopt;
                }
                asciiBuf[i] = static_cast<char>(trimmed.m_data[i]);
            }
            InFloatType parsedValue{ 0 };
            const char* const first = asciiBuf;
            const char* const last = asciiBuf + trimmed.m_size;
            const std::from_chars_result res = std::from_chars(first, last, parsedValue);
            if (res.ec == std::errc{} && res.ptr == last)
            {
                return Optional<InFloatType>(parsedValue);
            }
            return nullopt;
        }
    }

private:
    ConstPointer m_data{ nullptr };
    SizeType m_size{ 0u };
};

}   // namespace gp::container

/// @brief Stream insertion operator for `BasicStringView`; writes exactly `sv.size()` characters to `os`.
/// @param[in,out] os Output stream to write to.
/// @param[in] sv String view to output.
/// @return Reference to `os`.
template <gp::concepts::IsCharacter InCharT>
std::basic_ostream<InCharT>&
    operator<<(std::basic_ostream<InCharT>& os, const gp::container::BasicStringView<InCharT>& sv)
{
    if (sv.data() != nullptr && sv.size() > 0u)
    {
        os.write(sv.data(), static_cast<std::streamsize>(sv.size()));
    }
    return os;
}

/// @brief `std::formatter` specialization for `gp::container::BasicStringView`.
template <gp::concepts::IsCharacter InCharT>
struct std::formatter<gp::container::BasicStringView<InCharT>, InCharT>
    : std::formatter<std::basic_string_view<InCharT>, InCharT>
{
    template <typename InFormatContext>
    auto format(const gp::container::BasicStringView<InCharT> sv, InFormatContext& ctx) const
    {
        const std::basic_string_view<InCharT> stdSv(sv.data(), sv.size());
        return std::formatter<std::basic_string_view<InCharT>, InCharT>::format(stdSv, ctx);
    }
};
