// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/StringEnums.hpp"
#include "containers/strings/Unicode.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/Platform.hpp"
#include <compare>
#include <format>
#include <iterator>
#include <limits>
#include <ostream>
#include <string_view>
#include <type_traits>

namespace gp::container
{

/// @brief A fixed-size string that is allocated inline on the stack or within a struct.
/// @tparam InCharT The character type of the string.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
/// @note This class is designed to be a lightweight, stack-allocated string with a fixed capacity.
///       It never performs dynamic memory allocation and is suitable for scenarios where the maximum string size is
///       known at compile time.
template <concepts::IsCharacter InCharT, gp::USize N>
class BasicFixedString
{
public:
    using SmallSizeType = std::conditional_t<N <= 0xFFu, gp::UInt8, gp::UInt16>;
    static_assert(N < std::numeric_limits<gp::UInt16>::max(), "BasicFixedString: N must be less than 65535.");

    using TraitsType = CharTraits<InCharT>;
    using ValueType = InCharT;
    using SizeType = SmallSizeType;
    using DifferenceType = gp::ISize;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Iterator = Pointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    /// @brief Sentinel value indicating "not found".
    static constexpr gp::USize npos = BasicStringView<InCharT>::npos;

public:
    /// @brief Default constructor. Initializes an empty null-terminated fixed string.
    constexpr BasicFixedString() noexcept = default;

    /// @brief Constructs a fixed string from a string view.
    /// @param[in] view The string view to construct from.
    constexpr BasicFixedString(const BasicStringView<InCharT> view) noexcept
    {
        assign(view);
    }

    /// @brief Constructs a fixed string from a null-terminated C-string.
    /// @param[in] str The null-terminated string to construct from.
    constexpr BasicFixedString(const InCharT* const str) noexcept
    {
        assign(BasicStringView<InCharT>(str));
    }

    /// @brief Constructs a fixed string from a character pointer and explicit length.
    /// @param[in] str Pointer to the character array.
    /// @param[in] count Number of characters to copy.
    constexpr BasicFixedString(const InCharT* const str, const gp::USize count) noexcept
    {
        assign(BasicStringView<InCharT>(str, count));
    }

    /// @brief Constructs a fixed string filled with `count` copies of `ch`.
    /// @param[in] count Number of characters to write (must be `<= N`).
    /// @param[in] ch Fill character.
    constexpr BasicFixedString(const gp::USize count, const InCharT ch) noexcept
    {
        GP_ASSERT(count <= N && "FixedString capacity exceeded");
        m_size = static_cast<SizeType>(count);
        if (m_size > 0u)
        {
            TraitsType::assign(m_data, count, ch);
        }
        m_data[m_size] = InCharT{ 0 };
    }

public:
    /// @brief Gets a reference to the character at the specified position.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A reference to the character at the specified position.
    [[nodiscard]] constexpr Reference operator[](const SizeType pos) noexcept
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Gets a const reference to the character at the specified position.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A const reference to the character at the specified position.
    [[nodiscard]] constexpr ConstReference operator[](const SizeType pos) const noexcept
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Gets a pointer to the underlying null-terminated character array.
    /// @return A pointer to the underlying character array.
    [[nodiscard]] constexpr Pointer operator*() noexcept
    {
        return m_data;
    }

    /// @brief Gets a const pointer to the underlying null-terminated character array.
    /// @return A const pointer to the underlying character array.
    [[nodiscard]] constexpr ConstPointer operator*() const noexcept
    {
        return m_data;
    }

    /// @brief Compares this string with a string view for equality.
    /// @param[in] other The string view to compare with.
    /// @return True if the strings are equal, false otherwise.
    [[nodiscard]] constexpr bool operator==(const BasicStringView<InCharT>& other) const noexcept
    {
        return view() == other;
    }

    /// @brief Compares this string with a null-terminated C-string for equality.
    /// @param[in] rhs The null-terminated C-string to compare with.
    /// @return True if the strings are equal, false otherwise.
    [[nodiscard]] constexpr bool operator==(const InCharT* const rhs) const noexcept
    {
        return view() == BasicStringView<InCharT>(rhs);
    }

    /// @brief Compares this string with a string view for three-way lexicographical ordering.
    /// @param[in] other The string view to compare with.
    /// @return A `std::strong_ordering` indicating the relative ordering of the strings.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const BasicStringView<InCharT>& other) const noexcept
    {
        return view() <=> other;
    }

    /// @brief Compares this string with a null-terminated C-string for three-way lexicographical ordering.
    /// @param[in] rhs The null-terminated C-string to compare with.
    /// @return A `std::strong_ordering` indicating the relative ordering of the strings.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const InCharT* const rhs) const noexcept
    {
        return view() <=> BasicStringView<InCharT>(rhs);
    }

    /// @brief Compares this string with another fixed string for equality.
    /// @tparam M Capacity of the other fixed string.
    /// @param[in] other The fixed string to compare with.
    /// @return True if the strings are equal, false otherwise.
    template <gp::USize M>
    [[nodiscard]] constexpr bool operator==(const BasicFixedString<InCharT, M>& other) const noexcept
    {
        return view() == other.view();
    }

    /// @brief Compares this string with another fixed string for ordering.
    /// @tparam M Capacity of the other fixed string.
    /// @param[in] other The fixed string to compare with.
    /// @return A `std::strong_ordering` indicating the relative ordering of the strings.
    template <gp::USize M>
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const BasicFixedString<InCharT, M>& other) const noexcept
    {
        return view() <=> other.view();
    }

    /// @brief Implicitly converts this fixed string to a non-owning string view.
    /// @return A string view representing the contents of this fixed string.
    [[nodiscard]] constexpr operator BasicStringView<InCharT>() const noexcept
    {
        return BasicStringView<InCharT>(m_data, m_size);
    }

    /// @brief Appends a string view to this fixed string.
    /// @param[in] strView The string view to append.
    /// @return A reference to this fixed string.
    constexpr BasicFixedString& operator+=(const BasicStringView<InCharT> strView) noexcept
    {
        append(strView);
        return *this;
    }

    /// @brief Appends a single character to this fixed string.
    /// @param[in] ch The character to append.
    /// @return A reference to this fixed string.
    constexpr BasicFixedString& operator+=(const InCharT ch) noexcept
    {
        pushBack(ch);
        return *this;
    }

public:
    /// @brief Gets a reference to the character at the specified position with bounds checking.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A reference to the character at the specified position.
    [[nodiscard]] constexpr Reference at(const SizeType pos) noexcept
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Gets a const reference to the character at the specified position with bounds checking.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A const reference to the character at the specified position.
    [[nodiscard]] constexpr ConstReference at(const SizeType pos) const noexcept
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Gets a reference to the first character in the string.
    /// @return A reference to the first character in the string.
    [[nodiscard]] constexpr Reference front() noexcept
    {
        GP_ASSERT(!isEmpty());
        return m_data[0];
    }

    /// @brief Gets a const reference to the first character in the string.
    /// @return A const reference to the first character in the string.
    [[nodiscard]] constexpr ConstReference front() const noexcept
    {
        GP_ASSERT(!isEmpty());
        return m_data[0];
    }

    /// @brief Gets a reference to the last character in the string.
    /// @return A reference to the last character in the string.
    [[nodiscard]] constexpr Reference back() noexcept
    {
        GP_ASSERT(!isEmpty());
        return m_data[m_size - 1u];
    }

    /// @brief Gets a const reference to the last character in the string.
    /// @return A const reference to the last character in the string.
    [[nodiscard]] constexpr ConstReference back() const noexcept
    {
        GP_ASSERT(!isEmpty());
        return m_data[m_size - 1u];
    }

    /// @brief Checks if the string is empty.
    /// @return True if the string is empty, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_size == 0u;
    }

    /// @brief Gets the number of characters in the string.
    /// @return The number of characters in the string.
    [[nodiscard]] constexpr SizeType size() const noexcept
    {
        return m_size;
    }

    /// @brief Gets the number of characters in the string.
    /// @return The number of characters in the string.
    [[nodiscard]] constexpr SizeType length() const noexcept
    {
        return m_size;
    }

    /// @brief Gets the size of the active string characters in bytes (excluding the null terminator).
    /// @return Number of bytes occupied by the active string characters.
    [[nodiscard]] constexpr gp::USize sizeInBytes() const noexcept
    {
        return static_cast<gp::USize>(m_size) * sizeof(InCharT);
    }

    /// @brief Gets the maximum number of characters the string can hold, excluding the null terminator.
    /// @return The maximum number of characters the string can hold, excluding the null terminator.
    [[nodiscard]] constexpr SizeType capacity() const noexcept
    {
        return static_cast<SizeType>(N);
    }

    /// @brief Gets the maximum number of characters the string can hold, excluding the null terminator.
    /// @return The maximum number of characters the string can hold, excluding the null terminator.
    [[nodiscard]] constexpr SizeType maxSize() const noexcept
    {
        return static_cast<SizeType>(N);
    }

    /// @brief Gets a const pointer to the underlying character array.
    /// @return A const pointer to the underlying character array.
    [[nodiscard]] constexpr ConstPointer data() const noexcept
    {
        return m_data;
    }

    /// @brief Gets a mutable pointer to the underlying character array.
    /// @return A mutable pointer to the underlying character array.
    [[nodiscard]] constexpr Pointer data() noexcept
    {
        return m_data;
    }

    /// @brief Converts this fixed string to a non-owning string view.
    /// @return A string view representing the contents of this fixed string.
    [[nodiscard]] constexpr BasicStringView<InCharT> view() const noexcept
    {
        return BasicStringView<InCharT>(m_data, m_size);
    }

    /// @brief Gets a const pointer to the underlying null-terminated character array.
    /// @return A const pointer to the underlying null-terminated character array.
    [[nodiscard]] constexpr ConstPointer cString() const noexcept
    {
        return m_data;
    }

    /// @brief Gets a mutable pointer to the underlying null-terminated character array.
    /// @return A mutable pointer to the underlying null-terminated character array.
    [[nodiscard]] constexpr Pointer cString() noexcept
    {
        return m_data;
    }

    /// @brief Gets a const iterator to the beginning of the string.
    /// @return A const iterator to the beginning of the string.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return m_data;
    }

    /// @brief Gets an iterator to the beginning of the string.
    /// @return An iterator to the beginning of the string.
    [[nodiscard]] constexpr Iterator begin() noexcept
    {
        return m_data;
    }

    /// @brief Gets a const iterator to the end of the string.
    /// @return A const iterator to the end of the string.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Gets an iterator to the end of the string.
    /// @return An iterator to the end of the string.
    [[nodiscard]] constexpr Iterator end() noexcept
    {
        return m_data + m_size;
    }

    /// @brief Gets a const iterator to the beginning of the string.
    /// @return A const iterator to the beginning of the string.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return m_data;
    }

    /// @brief Gets a const iterator to the end of the string.
    /// @return A const iterator to the end of the string.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Gets a const reverse iterator to the beginning of the reversed string.
    /// @return A const reverse iterator to the beginning of the reversed string.
    [[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Gets a reverse iterator to the beginning of the reversed string.
    /// @return A reverse iterator to the beginning of the reversed string.
    [[nodiscard]] constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Gets a const reverse iterator to the end of the reversed string.
    /// @return A const reverse iterator to the end of the reversed string.
    [[nodiscard]] constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Gets a reverse iterator to the end of the reversed string.
    /// @return A reverse iterator to the end of the reversed string.
    [[nodiscard]] constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Gets a const reverse iterator to the beginning of the reversed string.
    /// @return A const reverse iterator to the beginning of the reversed string.
    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Gets a const reverse iterator to the end of the reversed string.
    /// @return A const reverse iterator to the end of the reversed string.
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Clears the string, setting its size to zero and null-terminating it.
    /// @note This does not deallocate any memory, as the string is stack-allocated and has a fixed capacity.
    constexpr void clear() noexcept
    {
        m_size = 0u;
        m_data[0] = InCharT{ 0 };
    }

    /// @brief Resizes the active length without initializing newly exposed elements, then writes the null terminator.
    /// @param[in] newSize New length in code units (must be `<= N`).
    constexpr void resizeUninitialized(const gp::USize newSize) noexcept
    {
        GP_ASSERT(newSize <= N && "FixedString capacity exceeded");
        m_size = static_cast<SizeType>(newSize);
        m_data[m_size] = InCharT{ 0 };
    }

    /// @brief Assigns the contents of a string view to this fixed string (safe for overlapping subviews).
    /// @param[in] strView The string view to assign from.
    constexpr void assign(const BasicStringView<InCharT> strView) noexcept
    {
        GP_ASSERT(strView.size() <= N && "FixedString capacity exceeded");
        const SizeType newSize = static_cast<SizeType>(strView.size());
        if (newSize > 0u)
        {
            TraitsType::move(m_data, strView.data(), newSize);
        }
        m_size = newSize;
        m_data[m_size] = InCharT{ 0 };
    }

    /// @brief Appends the contents of a string view to this fixed string (safe for overlapping subviews).
    /// @param[in] strView The string view to append.
    constexpr void append(const BasicStringView<InCharT> strView) noexcept
    {
        GP_ASSERT(static_cast<gp::USize>(m_size) + strView.size() <= N && "FixedString capacity exceeded");
        if (!strView.isEmpty())
        {
            TraitsType::move(m_data + m_size, strView.data(), strView.size());
            m_size = static_cast<SizeType>(static_cast<gp::USize>(m_size) + strView.size());
            m_data[m_size] = InCharT{ 0 };
        }
    }

    /// @brief Appends a single character to this fixed string.
    /// @param[in] ch The character to append.
    constexpr void pushBack(const InCharT ch) noexcept
    {
        GP_ASSERT(m_size < N && "FixedString capacity exceeded");
        m_data[m_size++] = ch;
        m_data[m_size] = InCharT{ 0 };
    }

    /// @brief Removes the last character from this fixed string.
    /// @note This does not deallocate any memory, as the string is stack-allocated and has a fixed capacity.
    constexpr void popBack() noexcept
    {
        GP_ASSERT(m_size > 0u && "Cannot pop from an empty string");
        --m_size;
        m_data[m_size] = InCharT{ 0 };
    }

    /// @brief Converts all ASCII uppercase characters in-place to lowercase.
    /// @return Reference to `*this`.
    constexpr BasicFixedString& toLowerInPlace() noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            m_data[i] = TraitsType::toLower(m_data[i]);
        }
        return *this;
    }

    /// @brief Converts all ASCII lowercase characters in-place to uppercase.
    /// @return Reference to `*this`.
    constexpr BasicFixedString& toUpperInPlace() noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            m_data[i] = TraitsType::toUpper(m_data[i]);
        }
        return *this;
    }

    /// @brief Trims leading and trailing ASCII whitespace in-place.
    /// @return Reference to `*this`.
    constexpr BasicFixedString& trimInPlace() noexcept
    {
        assign(view().trim());
        return *this;
    }

    /// @brief Compares this fixed string with a string view using the specified case-sensitivity mode.
    /// @param[in] other The string view to compare with.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if equal under `searchCase`, false otherwise.
    [[nodiscard]] constexpr bool equals(
        const BasicStringView<InCharT> other, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().equals(other, searchCase);
    }

    /// @brief Case-insensitive equality convenience check.
    /// @param[in] other The string view to compare with.
    /// @return True if equal ignoring ASCII case, false otherwise.
    [[nodiscard]] constexpr bool equalsIgnoreCase(const BasicStringView<InCharT> other) const noexcept
    {
        return view().equalsIgnoreCase(other);
    }

    /// @brief Three-way lexicographical comparison with configurable case sensitivity.
    /// @param[in] other The string view to compare with.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Negative if `*this < other`, 0 if equal, positive if `*this > other`.
    [[nodiscard]] constexpr gp::Int32 compare(
        const BasicStringView<InCharT> other, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().compare(other, searchCase);
    }

    /// @brief Checks whether this string begins with `prefix`.
    /// @param[in] prefix Prefix view to test.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if `*this` begins with `prefix`.
    [[nodiscard]] constexpr bool startsWith(
        const BasicStringView<InCharT> prefix, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().startsWith(prefix, searchCase);
    }

    /// @brief Checks whether this string ends with `suffix`.
    /// @param[in] suffix Suffix view to test.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if `*this` ends with `suffix`.
    [[nodiscard]] constexpr bool endsWith(
        const BasicStringView<InCharT> suffix, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().endsWith(suffix, searchCase);
    }

    /// @brief Checks whether this string contains `sub`.
    /// @param[in] sub Substring view to search for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if `sub` occurs within `*this`.
    [[nodiscard]] constexpr bool contains(
        const BasicStringView<InCharT> sub, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().contains(sub, searchCase);
    }

    /// @brief Checks whether this string contains character `ch`.
    /// @param[in] ch Character to search for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if `ch` occurs within `*this`.
    [[nodiscard]] constexpr bool
        contains(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().contains(ch, searchCase);
    }

    /// @brief Finds the first occurrence of `sub` starting at `pos`.
    /// @param[in] sub Substring view to search for.
    /// @param[in] pos Starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr gp::USize find(
        const BasicStringView<InCharT> sub,
        const gp::USize pos = 0u,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().find(sub, pos, searchCase);
    }

    /// @brief Finds the last occurrence of `sub` at or before `pos`.
    /// @param[in] sub Substring view to search for.
    /// @param[in] pos Maximum starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr gp::USize rfind(
        const BasicStringView<InCharT> sub,
        const gp::USize pos = npos,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().rfind(sub, pos, searchCase);
    }

    /// @brief Finds an occurrence of `sub` in the specified `searchDir` (`FromStart` or `FromEnd`).
    /// @param[in] sub Substring view to search for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @param[in] searchDir Search direction (`SearchDir::FromStart` or `SearchDir::FromEnd`).
    /// @param[in] startPos Starting position (`0` for `FromStart`, `npos` for `FromEnd` when omitted).
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr gp::USize indexOf(
        const BasicStringView<InCharT> sub,
        const SearchCase searchCase = SearchCase::CaseSensitive,
        const SearchDir searchDir = SearchDir::FromStart,
        const gp::USize startPos = npos
    ) const noexcept
    {
        return view().indexOf(sub, searchCase, searchDir, startPos);
    }

    /// @brief Returns a non-owning subview `[pos, pos + count)`.
    /// @param[in] pos Starting index.
    /// @param[in] count Maximum number of characters to include.
    /// @return Sliced `BasicStringView<InCharT>`.
    [[nodiscard]] constexpr BasicStringView<InCharT>
        subview(const gp::USize pos = 0u, const gp::USize count = npos) const noexcept
    {
        return view().subview(pos, count);
    }

    /// @brief Computes the deterministic 64-bit FNV-1a hash of this fixed string.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return 64-bit FNV-1a hash value.
    [[nodiscard]] constexpr gp::UInt64 hash(const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().hash(searchCase);
    }

    /// @brief Computes the case-insensitive 64-bit FNV-1a hash of this fixed string.
    /// @return 64-bit case-folded FNV-1a hash value.
    [[nodiscard]] constexpr gp::UInt64 hashIgnoreCase() const noexcept
    {
        return view().hashIgnoreCase();
    }

    /// @brief Returns a range adapter that decodes Unicode codepoints (`char32_t`) on the fly.
    /// @return `unicode::CodepointView<InCharT>` over this fixed string.
    [[nodiscard]] constexpr unicode::CodepointView<InCharT> codepoints() const noexcept
    {
        return unicode::CodepointView<InCharT>(view());
    }

private:
    ValueType m_data[N + 1u]{};
    SizeType m_size{ 0u };
};

/// @brief Stream insertion operator for `BasicFixedString` (supports GoogleTest diagnostic printing).
/// @tparam InCharT Character type.
/// @tparam N Capacity of the fixed string.
/// @param[in,out] os Output stream.
/// @param[in] str Fixed string to write.
/// @return Reference to `os`.
template <concepts::IsCharacter InCharT, gp::USize N>
inline std::basic_ostream<InCharT>& operator<<(std::basic_ostream<InCharT>& os, const BasicFixedString<InCharT, N>& str)
{
    return os.write(str.data(), static_cast<std::streamsize>(str.size()));
}

}   // namespace gp::container

namespace gp
{

/// @brief Owning, stack-allocated UTF-8 string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedString = container::BasicFixedString<char, N>;

/// @brief Owning, stack-allocated wide string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedWString = container::BasicFixedString<wchar_t, N>;

/// @brief Owning, stack-allocated `char8_t` UTF-8 string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedU8String = container::BasicFixedString<char8_t, N>;

/// @brief Owning, stack-allocated UTF-16 string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedU16String = container::BasicFixedString<char16_t, N>;

/// @brief Owning, stack-allocated UTF-32 string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedU32String = container::BasicFixedString<char32_t, N>;

}   // namespace gp

namespace std
{

/// @brief `std::formatter` specialization for `gp::container::BasicFixedString`.
template <gp::concepts::IsCharacter InCharT, gp::USize N>
struct formatter<gp::container::BasicFixedString<InCharT, N>, InCharT> : formatter<basic_string_view<InCharT>, InCharT>
{
    /// @brief Formats `str` by delegating to `std::basic_string_view<InCharT>`.
    /// @tparam InFormatContext Format context type.
    /// @param[in] str The fixed string to format.
    /// @param[in,out] ctx The format context.
    /// @return Iterator past the end of the formatted output.
    template <typename InFormatContext>
    auto format(const gp::container::BasicFixedString<InCharT, N>& str, InFormatContext& ctx) const
    {
        return formatter<basic_string_view<InCharT>, InCharT>::format(
            basic_string_view<InCharT>(str.data(), str.size()), ctx
        );
    }
};

}   // namespace std
