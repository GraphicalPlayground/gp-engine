// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include <compare>
#include <ostream>

namespace gp
{

/// @brief Non-owning, read-only view into a contiguous character sequence.
/// @details
/// Provides a lightweight, non-allocating interface for string inspection.
/// Aggressively constexpr to enable compile-time string processing. All search
/// methods use brute-force O(n*m) algorithms, sufficient for typical engine
/// string lengths without pulling in <algorithm>.
/// @tparam CharT The character type.
/// @tparam Traits The character traits type (defaults to std::char_traits<CharT>).
template <typename CharT, typename Traits>
class BasicStringView
{
public:
    using TraitsType = Traits;
    using ValueType = CharT;
    using Pointer = const CharT*;
    using ConstPointer = const CharT*;
    using Reference = const CharT&;
    using ConstReference = const CharT&;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Iterator = ConstPointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
    static constexpr SizeType npos = static_cast<SizeType>(-1);

private:
    ConstPointer m_data{ nullptr };
    SizeType m_size{ 0ull };

public:
    /// @brief Default constructs an empty string view.
    constexpr BasicStringView() noexcept = default;

    /// @brief Constructs a view from a null-terminated C string.
    /// @param[in] str Pointer to a null-terminated character array, or nullptr.
    constexpr BasicStringView(const CharT* str) noexcept
        : m_data(str)
        , m_size(str ? TraitsType::length(str) : 0)
    {}

    /// @brief Constructs a view from a pointer and explicit length.
    /// @param[in] str Pointer to the character array.
    /// @param[in] count Number of characters in the view.
    constexpr BasicStringView(const CharT* str, SizeType count) noexcept
        : m_data(str)
        , m_size(count)
    {}

    /// @brief Constructs a view from a gp::basic_string.
    /// @tparam Allocator The string's allocator type.
    /// @param[in] str The string to view.
    template <typename Allocator = gp::memory::PolymorphicAllocator<CharT>>
    constexpr explicit BasicStringView(const gp::BasicString<CharT, Traits, Allocator>& str) noexcept
        : m_data(str.data())
        , m_size(str.size())
    {}

    constexpr BasicStringView(const BasicStringView&) noexcept = default;
    constexpr BasicStringView& operator=(const BasicStringView&) noexcept = default;

public:
    /// @brief Unchecked element access.
    /// @param[in] pos Index of the character.
    /// @return Reference to the character at pos.
    GP_NODISCARD constexpr ConstReference operator[](SizeType pos) const noexcept { return m_data[pos]; }

    /// @brief Equality comparison.
    /// @param[in] other The view to compare against.
    /// @return True if the views are equal, false otherwise.
    GP_NODISCARD constexpr bool operator==(const BasicStringView& other) const noexcept
    {
        return m_size == other.m_size && TraitsType::compare(m_data, other.m_data, m_size) == 0;
    }

    /// @brief Inequality comparison.
    /// @param[in] other The view to compare against.
    /// @return True if the views are not equal, false otherwise.
    GP_NODISCARD constexpr bool operator!=(const BasicStringView& other) const noexcept { return !(*this == other); }

    /// @brief Three-way comparison.
    /// @param[in] other The view to compare against.
    /// @return Strong ordering indicating the relative order of the views.
    GP_NODISCARD constexpr auto operator<=>(const BasicStringView& other) const noexcept
    {
        const SizeType len = m_size < other.m_size ? m_size : other.m_size;
        int cmp = TraitsType::compare(m_data, other.m_data, len);
        if (cmp != 0) { return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater; }
        return m_size <=> other.m_size;
    }

public:
    /// @brief Returns a pointer to the underlying character array.
    /// @return Pointer to the first character in the view, or nullptr if empty.
    GP_NODISCARD constexpr ConstPointer data() const noexcept { return m_data; }

    /// @brief Returns the number of characters in the view.
    /// @return The length of the view (not counting any null terminator).
    GP_NODISCARD constexpr SizeType size() const noexcept { return m_size; }

    /// @brief Returns the number of characters in the view (alias for size).
    /// @return The length of the view (not counting any null terminator).
    GP_NODISCARD constexpr SizeType length() const noexcept { return m_size; }

    /// @brief Returns the maximum number of characters the view can refer to.
    /// @return The maximum size of the view, typically limited by SizeType.
    GP_NODISCARD constexpr SizeType maxSize() const noexcept { return npos / sizeof(CharT); }

    /// @brief Checks if the view is empty.
    /// @return True if the view is empty, false otherwise.
    GP_NODISCARD constexpr bool empty() const noexcept { return m_size == 0; }

    /// @brief Bounds-checked element access.
    /// @param[in] pos Index of the character.
    /// @return Reference to the character at pos.
    GP_NODISCARD constexpr ConstReference at(SizeType pos) const
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Returns a reference to the first character.
    /// @return Reference to the first character in the view.
    GP_NODISCARD constexpr ConstReference front() const noexcept { return m_data[0]; }

    /// @brief Returns a reference to the last character.
    /// @return Reference to the last character in the view.
    GP_NODISCARD constexpr ConstReference back() const noexcept { return m_data[m_size - 1]; }

    /// @brief Returns an iterator to the first character.
    /// @return Iterator pointing to the first character in the view.
    GP_NODISCARD constexpr Iterator begin() const noexcept { return m_data; }

    /// @brief Returns an iterator to one past the last character.
    /// @return Iterator pointing to one past the last character in the view.
    GP_NODISCARD constexpr Iterator end() const noexcept { return m_data + m_size; }

    /// @brief Returns a const iterator to the first character.
    /// @return Const iterator pointing to the first character in the view.
    GP_NODISCARD constexpr ConstIterator cbegin() const noexcept { return m_data; }

    /// @brief Returns a const iterator to one past the last character.
    /// @return Const iterator pointing to one past the last character in the view.
    GP_NODISCARD constexpr ConstIterator cend() const noexcept { return m_data + m_size; }

    /// @brief Returns a reverse iterator to the last character.
    /// @return Reverse iterator pointing to the last character in the view.
    GP_NODISCARD constexpr ReverseIterator rbegin() const noexcept { return ReverseIterator(end()); }

    /// @brief Returns a reverse iterator to one before the first character.
    /// @return Reverse iterator pointing to one before the first character in the view.
    GP_NODISCARD constexpr ReverseIterator rend() const noexcept { return ReverseIterator(begin()); }

    /// @brief Returns a const reverse iterator to the last character.
    /// @return Const reverse iterator pointing to the last character in the view.
    GP_NODISCARD constexpr ConstReverseIterator crbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// @brief Returns a const reverse iterator to one before the first character.
    /// @return Const reverse iterator pointing to one before the first character in the view.
    GP_NODISCARD constexpr ConstReverseIterator crend() const noexcept { return ConstReverseIterator(cbegin()); }

    /// @brief Shrinks the view by moving the start forward.
    /// @param[in] n Number of characters to remove from the front.
    constexpr void removePrefix(SizeType n) noexcept
    {
        GP_ASSERT(n <= m_size);
        m_data += n;
        m_size -= n;
    }

    /// @brief Shrinks the view by moving the end backward.
    /// @param[in] n Number of characters to remove from the back.
    constexpr void removeSuffix(SizeType n) noexcept
    {
        GP_ASSERT(n <= m_size);
        m_size -= n;
    }

    /// @brief Swaps the contents of two string views.
    /// @param[in,out] other The view to swap with.
    constexpr void swap(BasicStringView& other) noexcept
    {
        const ConstPointer tmp_data = m_data;
        const SizeType tmp_size = m_size;
        m_data = other.m_data;
        m_size = other.m_size;
        other.m_data = tmp_data;
        other.m_size = tmp_size;
    }

    /// @brief Copies characters to an external buffer.
    /// @param[out] dest Destination buffer.
    /// @param[in] count Maximum number of characters to copy.
    /// @param[in] pos Starting position.
    /// @return Number of characters actually copied.
    constexpr SizeType copy(CharT* dest, SizeType count, SizeType pos = 0) const
    {
        GP_ASSERT(pos <= m_size);
        const SizeType rcount = _min(count, m_size - pos);
        TraitsType::copy(dest, m_data + pos, rcount);
        return rcount;
    }

    /// @brief Returns a substring view.
    /// @param[in] pos Starting position.
    /// @param[in] count Number of characters (clamped to remaining length).
    /// @return A new view over the requested range.
    GP_NODISCARD constexpr BasicStringView substr(SizeType pos = 0, SizeType count = npos) const
    {
        GP_ASSERT(pos <= m_size);
        return BasicStringView(m_data + pos, _min(count, m_size - pos));
    }

    /// @brief Compares this view with another.
    /// @param[in] other The view to compare against.
    /// @return Negative if less, zero if equal, positive if greater.
    GP_NODISCARD constexpr int compare(BasicStringView other) const noexcept
    {
        const SizeType len = _min(m_size, other.m_size);
        int result = TraitsType::compare(m_data, other.m_data, len);
        if (result != 0) { return result; }
        if (m_size < other.m_size) { return -1; }
        if (m_size > other.m_size) { return 1; }
        return 0;
    }

    /// @brief Compares a substring of this view with another view.
    /// @param[in] pos Starting position of the substring in this view.
    /// @param[in] count Number of characters in the substring (clamped to remaining).
    /// @param[in] other The view to compare against.
    /// @return Negative if less, zero if equal, positive if greater.
    GP_NODISCARD constexpr int compare(SizeType pos, SizeType count, BasicStringView other) const
    {
        return substr(pos, count).compare(other);
    }

    /// @brief Compares a substring of this view with a substring of another view.
    /// @param[in] pos1 Starting position of the substring in this view.
    /// @param[in] count1 Number of characters in the substring of this view (clamped to remaining).
    /// @param[in] other The view to compare against.
    /// @param[in] pos2 Starting position of the substring in the other view.
    /// @param[in] count2 Number of characters in the substring of the other view (clamped to remaining).
    /// @return Negative if less, zero if equal, positive if greater.
    GP_NODISCARD constexpr int
        compare(SizeType pos1, SizeType count1, BasicStringView other, SizeType pos2, SizeType count2) const
    {
        return substr(pos1, count1).compare(other.substr(pos2, count2));
    }

    /// @brief Compares this view with a null-terminated C string.
    /// @param[in] str The C string to compare against.
    /// @return Negative if less, zero if equal, positive if greater.
    GP_NODISCARD constexpr int compare(const CharT* str) const { return compare(BasicStringView(str)); }

    /// @brief Compares a substring of this view with a null-terminated C string.
    /// @param[in] pos Starting position of the substring in this view.
    /// @param[in] count Number of characters in the substring (clamped to remaining).
    /// @param[in] str The C string to compare against.
    /// @return Negative if less, zero if equal, positive if greater.
    GP_NODISCARD constexpr int compare(SizeType pos, SizeType count, const CharT* str) const
    {
        return substr(pos, count).compare(BasicStringView(str));
    }

    /// @brief Checks if the view starts with the given prefix.
    /// @param[in] prefix The prefix to check for.
    /// @return True if the view starts with prefix, false otherwise.
    GP_NODISCARD constexpr bool startsWith(BasicStringView prefix) const noexcept
    {
        return m_size >= prefix.m_size && TraitsType::compare(m_data, prefix.m_data, prefix.m_size) == 0;
    }

    /// @brief Checks if the view starts with the given character.
    /// @param[in] ch The character to check for.
    /// @return True if the view starts with ch, false otherwise.
    GP_NODISCARD constexpr bool startsWith(CharT ch) const noexcept
    {
        return m_size > 0 && TraitsType::eq(m_data[0], ch);
    }

    /// @brief Checks if the view starts with the given C string.
    /// @param[in] str The C string to check for.
    /// @return True if the view starts with str, false otherwise.
    GP_NODISCARD constexpr bool startsWith(const CharT* str) const { return startsWith(BasicStringView(str)); }

    /// @brief Checks if the view ends with the given suffix.
    /// @param[in] suffix The suffix to check for.
    /// @return True if the view ends with suffix, false otherwise.
    GP_NODISCARD constexpr bool endsWith(BasicStringView suffix) const noexcept
    {
        return m_size >= suffix.m_size &&
               TraitsType::compare(m_data + (m_size - suffix.m_size), suffix.m_data, suffix.m_size) == 0;
    }

    /// @brief Checks if the view ends with the given character.
    /// @param[in] ch The character to check for.
    /// @return True if the view ends with ch, false otherwise.
    GP_NODISCARD constexpr bool endsWith(CharT ch) const noexcept
    {
        return m_size > 0 && TraitsType::eq(m_data[m_size - 1], ch);
    }

    /// @brief Checks if the view ends with the given C string.
    /// @param[in] str The C string to check for.
    /// @return True if the view ends with str, false otherwise.
    GP_NODISCARD constexpr bool endsWith(const CharT* str) const { return endsWith(BasicStringView(str)); }

    /// @brief Checks if the view contains the given substring.
    /// @param[in] sv The substring to check for.
    /// @return True if the view contains sv, false otherwise.
    GP_NODISCARD constexpr bool contains(BasicStringView sv) const noexcept { return find(sv) != npos; }

    /// @brief Checks if the view contains the given character.
    /// @param[in] ch The character to check for.
    /// @return True if the view contains ch, false otherwise.
    GP_NODISCARD constexpr bool contains(CharT ch) const noexcept { return find(ch) != npos; }

    /// @brief Checks if the view contains the given C string.
    /// @param[in] str The C string to check for.
    /// @return True if the view contains str, false otherwise.
    GP_NODISCARD constexpr bool contains(const CharT* str) const { return find(str) != npos; }

    /// @brief Finds the first occurrence of a substring.
    /// @param[in] sv The substring to search for.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType find(BasicStringView sv, SizeType pos = 0) const noexcept
    {
        if (sv.m_size == 0) { return pos <= m_size ? pos : npos; }
        if (pos + sv.m_size > m_size) { return npos; }
        const SizeType last = m_size - sv.m_size;
        for (SizeType i = pos; i <= last; ++i)
        {
            if (TraitsType::compare(m_data + i, sv.m_data, sv.m_size) == 0) { return i; }
        }
        return npos;
    }

    /// @brief Finds the first occurrence of a character.
    /// @param[in] ch The character to search for.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType find(CharT ch, SizeType pos = 0) const noexcept
    {
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (TraitsType::eq(m_data[i], ch)) { return i; }
        }
        return npos;
    }

    /// @brief Finds the first occurrence of a C string.
    /// @param[in] str The C string to search for.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType find(const CharT* str, SizeType pos = 0) const
    {
        return find(BasicStringView(str), pos);
    }

    /// @brief Finds the first occurrence of a buffer with explicit length.
    /// @param[in] str Pointer to the character array to search for.
    /// @param[in] pos Position to start searching from.
    /// @param[in] count Number of characters in the buffer.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType find(const CharT* str, SizeType pos, SizeType count) const
    {
        return find(BasicStringView(str, count), pos);
    }

    /// @brief Finds the last occurrence of a substring.
    /// @param[in] sv The substring to search for.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType rfind(BasicStringView sv, SizeType pos = npos) const noexcept
    {
        if (sv.m_size == 0) { return _min(pos, m_size); }
        if (sv.m_size > m_size) { return npos; }
        SizeType i = _min(pos, m_size - sv.m_size);
        for (;; --i)
        {
            if (TraitsType::compare(m_data + i, sv.m_data, sv.m_size) == 0) { return i; }
            if (i == 0) { break; }
        }
        return npos;
    }

    /// @brief Finds the last occurrence of a character.
    /// @param[in] ch The character to search for.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType rfind(CharT ch, SizeType pos = npos) const noexcept
    {
        if (m_size == 0) { return npos; }
        SizeType i = _min(pos, m_size - 1);
        for (;; --i)
        {
            if (TraitsType::eq(m_data[i], ch)) { return i; }
            if (i == 0) { break; }
        }
        return npos;
    }

    /// @brief Finds the last occurrence of a C string.
    /// @param[in] str The C string to search for.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType rfind(const CharT* str, SizeType pos = npos) const
    {
        return rfind(BasicStringView(str), pos);
    }

    /// @brief Finds the last occurrence of a buffer with explicit length.
    /// @param[in] str Pointer to the character array to search for.
    /// @param[in] pos Position to start searching backward from.
    /// @param[in] count Number of characters in the buffer.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType rfind(const CharT* str, SizeType pos, SizeType count) const
    {
        return rfind(BasicStringView(str, count), pos);
    }

    /// @brief Finds the first character that matches any character in the given view.
    /// @param[in] sv The set of characters to match against.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstOf(BasicStringView sv, SizeType pos = 0) const noexcept
    {
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (_charInSet(m_data[i], sv)) { return i; }
        }
        return npos;
    }

    /// @brief Finds the first occurrence of the given character.
    /// @param[in] ch The character to match.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstOf(CharT ch, SizeType pos = 0) const noexcept { return find(ch, pos); }

    /// @brief Finds the first character that matches any character in the given C string.
    /// @param[in] str The C string containing characters to match.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstOf(const CharT* str, SizeType pos = 0) const
    {
        return findFirstOf(BasicStringView(str), pos);
    }

    /// @brief Finds the first character that matches any character in the given buffer.
    /// @param[in] str Pointer to the character array containing characters to match.
    /// @param[in] pos Position to start searching from.
    /// @param[in] count Number of characters in the buffer.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstOf(const CharT* str, SizeType pos, SizeType count) const
    {
        return findFirstOf(BasicStringView(str, count), pos);
    }

    /// @brief Finds the last character that matches any character in the given view.
    /// @param[in] sv The set of characters to match against.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastOf(BasicStringView sv, SizeType pos = npos) const noexcept
    {
        if (m_size == 0) { return npos; }
        SizeType i = _min(pos, m_size - 1);
        for (;; --i)
        {
            if (_charInSet(m_data[i], sv)) { return i; }
            if (i == 0) { break; }
        }
        return npos;
    }

    /// @brief Finds the last occurrence of the given character.
    /// @param[in] ch The character to match.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastOf(CharT ch, SizeType pos = npos) const noexcept { return rfind(ch, pos); }

    /// @brief Finds the last character that matches any character in the given C string.
    /// @param[in] str The C string containing characters to match.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastOf(const CharT* str, SizeType pos = npos) const
    {
        return findLastOf(BasicStringView(str), pos);
    }

    /// @brief Finds the last character that matches any character in the given buffer.
    /// @param[in] str Pointer to the character array containing characters to match.
    /// @param[in] pos Position to start searching backward from.
    /// @param[in] count Number of characters in the buffer.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastOf(const CharT* str, SizeType pos, SizeType count) const
    {
        return findLastOf(BasicStringView(str, count), pos);
    }

    /// @brief Finds the first character that does not match any character in the given view.
    /// @param[in] sv The set of characters to not match against.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstNotOf(BasicStringView sv, SizeType pos = 0) const noexcept
    {
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (!_charInSet(m_data[i], sv)) { return i; }
        }
        return npos;
    }

    /// @brief Finds the first character that is not equal to the given character.
    /// @param[in] ch The character to not match.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstNotOf(CharT ch, SizeType pos = 0) const noexcept
    {
        for (SizeType i = pos; i < m_size; ++i)
        {
            if (!TraitsType::eq(m_data[i], ch)) { return i; }
        }
        return npos;
    }

    /// @brief Finds the first character that does not match any character in the given C string.
    /// @param[in] str The C string containing characters to not match.
    /// @param[in] pos Position to start searching from.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstNotOf(const CharT* str, SizeType pos = 0) const
    {
        return findFirstNotOf(BasicStringView(str), pos);
    }

    /// @brief Finds the first character that does not match any character in the given buffer.
    /// @param[in] str Pointer to the character array containing characters to not match.
    /// @param[in] pos Position to start searching from.
    /// @param[in] count Number of characters in the buffer.
    /// @return Index of the first occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findFirstNotOf(const CharT* str, SizeType pos, SizeType count) const
    {
        return findFirstNotOf(BasicStringView(str, count), pos);
    }

    /// @brief Finds the last character that does not match any character in the given view.
    /// @param[in] sv The set of characters to not match against.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastNotOf(BasicStringView sv, SizeType pos = npos) const noexcept
    {
        if (m_size == 0) { return npos; }
        SizeType i = _min(pos, m_size - 1);
        for (;; --i)
        {
            if (!_charInSet(m_data[i], sv)) { return i; }
            if (i == 0) { break; }
        }
        return npos;
    }

    /// @brief Finds the last character that is not equal to the given character.
    /// @param[in] ch The character to not match.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastNotOf(CharT ch, SizeType pos = npos) const noexcept
    {
        if (m_size == 0) { return npos; }
        SizeType i = _min(pos, m_size - 1);
        for (;; --i)
        {
            if (!TraitsType::eq(m_data[i], ch)) { return i; }
            if (i == 0) { break; }
        }
        return npos;
    }

    /// @brief Finds the last character that does not match any character in the given C string.
    /// @param[in] str The C string containing characters to not match.
    /// @param[in] pos Position to start searching backward from.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastNotOf(const CharT* str, SizeType pos = npos) const
    {
        return findLastNotOf(BasicStringView(str), pos);
    }

    /// @brief Finds the last character that does not match any character in the given buffer.
    /// @param[in] str Pointer to the character array containing characters to not match.
    /// @param[in] pos Position to start searching backward from.
    /// @param[in] count Number of characters in the buffer.
    /// @return Index of the last occurrence, or npos if not found.
    GP_NODISCARD constexpr SizeType findLastNotOf(const CharT* str, SizeType pos, SizeType count) const
    {
        return findLastNotOf(BasicStringView(str, count), pos);
    }

private:
    /// @brief Returns the minimum of two size values without <algorithm>.
    /// @param[in] a First size value.
    /// @param[in] b Second size value.
    /// @return The smaller of a and b.
    GP_NODISCARD static constexpr SizeType _min(SizeType a, SizeType b) noexcept { return a < b ? a : b; }

    /// @brief Checks if a character exists in the given character set view.
    /// @param[in] ch The character to check.
    /// @param[in] set The view representing the set of characters.
    /// @return True if ch is found in set, false otherwise.
    GP_NODISCARD static constexpr bool _charInSet(CharT ch, BasicStringView set) noexcept
    {
        for (SizeType i = 0; i < set.m_size; ++i)
        {
            if (TraitsType::eq(ch, set.m_data[i])) { return true; }
        }
        return false;
    }
};

}   // namespace gp

/// @brief Stream insertion operator for BasicStringView, outputs the view's characters to the stream.
/// @param os The output stream to write to.
/// @param sv The BasicStringView to output.
/// @return Reference to the output stream after writing the view's characters.
template <typename CharT, typename Traits>
std::ostream& operator<<(std::ostream& os, const gp::BasicStringView<CharT, Traits>& sv)
{
    return os << sv.data();
}
