// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Concepts.hpp"
#include <compare>
#include <cstddef>
#include <iterator>
#include <string_view>

namespace GP::Container
{

/// \brief A non-owning view of a contiguous sequence of characters, similar to std::string_view but with additional
/// functionality.
/// \tparam CharType The character type of the string view (e.g., char, wchar_t).
template <Concepts::IsCharacter CharType>
class TBasicStringView
{
public:
    using ValueType = CharType;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using SizeType = Size;
    using DifferenceType = Offset;
    using Iterator = const CharType*;
    using ConstIterator = const CharType*;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
    static constexpr SizeType NPOS = static_cast<SizeType>(-1);   //<! A constant representing a non-position.

private:
    ConstPointer m_data;   //<! Pointer to the first character of the string view
    SizeType m_size;       //<! Number of characters in the string view

public:
    /// \brief Constructs an empty string view.
    constexpr TBasicStringView() noexcept;

    /// \brief Construct a string view from another string view (copy constructor).
    constexpr TBasicStringView(const TBasicStringView&) noexcept = default;

    /// \brief Constructs a string view from a pointer and length.
    /// \param str Pointer to the first character of the string view.
    /// \param length Number of characters in the string view.
    constexpr TBasicStringView(ConstPointer str, SizeType length) noexcept;

    /// \brief Constructs a string view from a null-terminated string.
    /// \param str Pointer to the null-terminated string.
    constexpr TBasicStringView(ConstPointer str) noexcept;

    /// \brief Deleted constructor to prevent construction from nullptr.
    constexpr TBasicStringView(NullPtr) = delete;

public:
    /// \brief Assignment operator (copy assignment).
    constexpr TBasicStringView& operator=(const TBasicStringView&) noexcept = default;

    /// \brief Implicit conversion operator to std::basic_string_view.
    constexpr operator std::basic_string_view<CharType>() const noexcept;

    /// \brief Subscript operator for element access without bounds checking.
    /// \param index The index of the character to access.
    /// \return A reference to the character at the specified index.
    constexpr ConstReference operator[](SizeType index) const noexcept;

public:
    /// \brief Accesses the character at the specified index with bounds checking.
    /// \param index The index of the character to access.
    /// \return A reference to the character at the specified index.
    constexpr ConstReference At(SizeType index) const;

    /// \brief Returns a reference to the first character in the string view.
    /// \return A reference to the first character.
    constexpr ConstReference Front() const noexcept;

    /// \brief Returns a reference to the last character in the string view.
    /// \return A reference to the last character.
    constexpr ConstReference Back() const noexcept;

    /// \brief Returns a pointer to the underlying character array.
    /// \return A pointer to the first character in the string view.
    constexpr ConstPointer Data() const noexcept;

    /// \brief Returns the number of characters in the string view.
    /// \return The size of the string view.
    constexpr SizeType Size() const noexcept;

    /// \brief Returns the length of the string view (same as Size).
    /// \return The length of the string view.
    constexpr SizeType Length() const noexcept;

    /// \brief Checks if the string view is empty (i.e., has a size of 0).
    /// \return true if the string view is empty, false otherwise.
    constexpr bool IsEmpty() const noexcept;

    /// \brief Removes the first n characters from the string view.
    /// \param n The number of characters to remove from the beginning of the string view.
    constexpr void RemovePrefix(SizeType n) noexcept;

    /// \brief Removes the last n characters from the string view.
    /// \param n The number of characters to remove from the end of the string view.
    constexpr void RemoveSuffix(SizeType n) noexcept;

    /// \brief Swaps the contents of this string view with another string view.
    /// \param other The other string view to swap with.
    constexpr void Swap(TBasicStringView& other) noexcept;

    /// \brief Copies a substring of the string view into a provided buffer.
    /// \param dest Pointer to the destination buffer where the substring will be copied.
    /// \param count The number of characters to copy.
    /// \param pos The starting position in the string view from which to copy (default is 0).
    /// \return The number of characters actually copied.
    constexpr SizeType Copy(CharType* dest, SizeType count, SizeType pos = 0) const;

    /// \brief Returns a substring of the string view starting at the specified position and of the specified length.
    /// \param pos The starting position of the substring (default is 0).
    /// \param count The length of the substring (default is NPOS, which means until the end of the string view).
    /// \return A new string view representing the specified substring.
    constexpr TBasicStringView Substr(SizeType pos = 0, SizeType count = NPOS) const;

    /// \brief Compares this string view with another string view.
    /// \param other The other string view to compare with.
    /// \return An integer less than, equal to, or greater than zero if this string view is found, respectively, to be
    /// less than, to match, or be greater than the other string view.
    constexpr int Compare(TBasicStringView other) const noexcept;

    /// \brief Checks if the string view starts with the specified prefix.
    /// \param prefix The prefix to check for.
    /// \return true if the string view starts with the prefix, false otherwise.
    constexpr bool StartsWith(TBasicStringView prefix) const noexcept;

    /// \brief Checks if the string view starts with the specified character.
    /// \param c The character to check for.
    /// \return true if the string view starts with the character, false otherwise.
    constexpr bool StartsWith(CharType c) const noexcept;

    /// \brief Checks if the string view ends with the specified suffix.
    /// \param suffix The suffix to check for.
    /// \return true if the string view ends with the suffix, false otherwise.
    constexpr bool EndsWith(TBasicStringView suffix) const noexcept;

    /// \brief Checks if the string view ends with the specified character.
    /// \param c The character to check for.
    /// \return true if the string view ends with the character, false otherwise.
    constexpr bool EndsWith(CharType c) const noexcept;

    /// \brief Checks if the string view contains the specified substring.
    /// \param sv The substring to check for.
    /// \return true if the string view contains the substring, false otherwise.
    constexpr bool Contains(TBasicStringView sv) const noexcept;

    /// \brief Checks if the string view contains the specified character.
    /// \param c The character to check for.
    /// \return true if the string view contains the character, false otherwise.
    constexpr bool Contains(CharType c) const noexcept;

    /// \brief Finds the first occurrence of the specified substring starting from the given position.
    /// \param sv The substring to find.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of the substring, or NPOS if not found.
    constexpr SizeType Find(TBasicStringView sv, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of the specified character starting from the given position.
    /// \param c The character to find.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of the character, or NPOS if not found.
    constexpr SizeType Find(CharType c, SizeType pos = 0) const noexcept;

    /// \brief Finds the last occurrence of the specified substring starting from the given position and searching
    /// backwards.
    /// \param sv The substring to find.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of the substring, or NPOS if not found.
    constexpr SizeType RFind(TBasicStringView sv, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last occurrence of the specified character starting from the given position and searching
    /// backwards.
    /// \param c The character to find.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of the character, or NPOS if not found.
    constexpr SizeType RFind(CharType c, SizeType pos = NPOS) const noexcept;

    /// \brief Calculates a hash value for the string view.
    /// \return A hash value representing the contents of the string view.
    constexpr SizeType GetHash() const noexcept;

    /// \brief Returns an iterator to the beginning of the string view.
    /// \return An iterator pointing to the first character in the string view.
    constexpr Iterator begin() const noexcept;

    /// \brief Returns an iterator to the end of the string view.
    /// \return An iterator pointing to one past the last character in the string view.
    constexpr Iterator end() const noexcept;

    /// \brief Returns a const iterator to the beginning of the string view.
    /// \return A const iterator pointing to the first character in the string view.
    constexpr ConstIterator cbegin() const noexcept;

    /// \brief Returns a const iterator to the end of the string view.
    /// \return A const iterator pointing to one past the last character in the string view.
    constexpr ConstIterator cend() const noexcept;

    /// \brief Returns a reverse iterator to the beginning of the reversed string view (i.e., the end of the original
    /// string view).
    /// \return A reverse iterator pointing to the last character in the string view.
    constexpr ReverseIterator rbegin() const noexcept;

    /// \brief Returns a reverse iterator to the end of the reversed string view (i.e., the beginning of the original
    /// string view).
    /// \return A reverse iterator pointing to one before the first character in the string view.
    constexpr ReverseIterator rend() const noexcept;

    /// \brief Returns a const reverse iterator to the beginning of the reversed string view (i.e., the end of the
    /// original string view).
    /// \return A const reverse iterator pointing to the last character in the string view.
    constexpr ConstReverseIterator crbegin() const noexcept;

    /// \brief Returns a const reverse iterator to the end of the reversed string view (i.e., the beginning of the
    /// original string view).
    /// \return A const reverse iterator pointing to one before the first character in the string view.
    constexpr ConstReverseIterator crend() const noexcept;

    /// \brief Finds the first occurrence of any character from the specified substring starting from the given
    /// position.
    /// \param sv The substring containing the characters to find.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of any character from the substring, or NPOS if not found.
    constexpr SizeType FindFirstOf(TBasicStringView sv, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of the specified character starting from the given position.
    /// \param c The character to find.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of the character, or NPOS if not found.
    constexpr SizeType FindFirstOf(CharType c, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of any character from the specified substring starting from the given
    /// position.
    /// \param str Pointer to a character array containing the characters to find.
    /// \param pos The position to start the search from (default is 0).
    /// \param count The number of characters in the character array.
    /// \return The index of the first occurrence of any character from the character array, or NPOS if not found.
    constexpr SizeType FindFirstOf(ConstPointer str, SizeType pos, SizeType count) const noexcept;

    /// \brief Finds the first occurrence of any character from the specified null-terminated string starting from the
    /// given position.
    /// \param str Pointer to a null-terminated string containing the characters to find.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of any character from the null-terminated string, or NPOS if not
    /// found.
    constexpr SizeType FindFirstOf(ConstPointer str, SizeType pos = 0) const noexcept;

    /// \brief Finds the last occurrence of any character from the specified substring starting from the given position
    /// and searching backwards.
    /// \param sv The substring containing the characters to find.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of any character from the substring, or NPOS if not found.
    constexpr SizeType FindLastOf(TBasicStringView sv, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last occurrence of the specified character starting from the given position and searching
    /// backwards.
    /// \param c The character to find.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of the character, or NPOS if not found.
    constexpr SizeType FindLastOf(CharType c, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last occurrence of any character from the specified character array starting from the given
    /// position and searching backwards.
    /// \param str Pointer to a character array containing the characters to find.
    /// \param pos The position to start the search from (default is NPOS, which
    /// means starting from the end of the string view).
    /// \param count The number of characters in the character array.
    constexpr SizeType FindLastOf(ConstPointer str, SizeType pos, SizeType count) const noexcept;

    /// \brief Finds the last occurrence of any character from the specified null-terminated string starting from the
    /// given position and searching backwards.
    /// \param str Pointer to a null-terminated string containing the characters to find.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of any character from the null-terminated string, or NPOS if not found.
    constexpr SizeType FindLastOf(ConstPointer str, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the first occurrence of any character not in the specified substring starting from the given
    /// position.
    /// \param sv The substring containing the characters to avoid.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of any character not in the substring, or NPOS if not found.
    constexpr SizeType FindFirstNotOf(TBasicStringView sv, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of any character not equal to the specified character starting from the given
    /// position.
    /// \param c The character to avoid.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of any character not equal to the specified character, or NPOS if not
    /// found.
    constexpr SizeType FindFirstNotOf(CharType c, SizeType pos = 0) const noexcept;

    /// \brief Finds the first occurrence of any character not in the specified character array starting from the given
    /// position.
    /// \param str Pointer to a character array containing the characters to avoid.
    /// \param pos The position to start the search from (default is 0).
    /// \param count The number of characters in the character array.
    /// \return The index of the first occurrence of any character not in the character array, or NPOS if not found.
    constexpr SizeType FindFirstNotOf(ConstPointer str, SizeType pos, SizeType count) const noexcept;

    /// \brief Finds the first occurrence of any character not in the specified null-terminated string starting from the
    /// given position.
    /// \param str Pointer to a null-terminated string containing the characters to avoid.
    /// \param pos The position to start the search from (default is 0).
    /// \return The index of the first occurrence of any character not in the null-terminated string, or NPOS if not
    /// found.
    constexpr SizeType FindFirstNotOf(ConstPointer str, SizeType pos = 0) const noexcept;

    /// \brief Finds the last occurrence of any character not in the specified substring starting from the given
    /// position and searching backwards.
    /// \param sv The substring containing the characters to avoid.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of any character not in the substring, or NPOS if not found.
    constexpr SizeType FindLastNotOf(TBasicStringView sv, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last occurrence of any character not equal to the specified character starting from the given
    /// position and searching backwards.
    /// \param c The character to avoid.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of any character not equal to the specified character, or NPOS if not
    /// found.
    constexpr SizeType FindLastNotOf(CharType c, SizeType pos = NPOS) const noexcept;

    /// \brief Finds the last occurrence of any character not in the specified character array starting from the given
    /// position and searching backwards.
    /// \param str Pointer to a character array containing the characters to avoid.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \param count The number of characters in the character array.
    /// \return The index of the last occurrence of any character not in the character array, or NPOS if not found.
    constexpr SizeType FindLastNotOf(ConstPointer str, SizeType pos, SizeType count) const noexcept;

    /// \brief Finds the last occurrence of any character not in the specified null-terminated string starting from the
    /// given position and searching backwards.
    /// \param str Pointer to a null-terminated string containing the characters to avoid.
    /// \param pos The position to start the search from (default is NPOS, which means starting from the end of the
    /// string view).
    /// \return The index of the last occurrence of any character not in the null-terminated string, or NPOS if not
    /// found.
    constexpr SizeType FindLastNotOf(ConstPointer str, SizeType pos = NPOS) const noexcept;
};

}   // namespace GP::Container

/// \brief Equality operator for TBasicStringView, compares two string views for equality.
/// \tparam CharType The character type of the string views being compared.
/// \param lhs The left-hand side string view to compare.
/// \param rhs The right-hand side string view to compare.
/// \return true if the string views are equal, false otherwise.
template <GP::Concepts::IsCharacter CharType>
constexpr bool
    operator==(GP::Container::TBasicStringView<CharType> lhs, GP::Container::TBasicStringView<CharType> rhs) noexcept;

/// \brief Three-way comparison operator for TBasicStringView, compares two string views and returns a strong ordering.
/// \tparam CharType The character type of the string views being compared.
/// \param lhs The left-hand side string view to compare.
/// \param rhs The right-hand side string view to compare.
/// \return A std::strong_ordering value indicating the relative order of the string views.
template <GP::Concepts::IsCharacter CharType>
constexpr std::strong_ordering
    operator<=>(GP::Container::TBasicStringView<CharType> lhs, GP::Container::TBasicStringView<CharType> rhs) noexcept;

// Include the implementation
#include "Container/BasicStringView.inl"
