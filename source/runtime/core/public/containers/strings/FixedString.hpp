// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/Platform.hpp"
#include <limits>

namespace gp::container
{

/// @brief A fixed-size string that is allocated on the stack.
/// @tparam CharT The character type of the string.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
/// @note This class is designed to be a lightweight, stack-allocated string with a fixed capacity.
/// It does not perform any dynamic memory allocation and is suitable for scenarios where the maximum string size is
/// known at compile time.
template <concepts::IsCharacter CharT, gp::USize N>
class BasicFixedString
{
public:
    using SmallSizeType = std::conditional_t<N <= 0xFF, gp::UInt8, gp::UInt16>;
    static_assert(N < std::numeric_limits<gp::UInt16>::max(), "BasicFixedString: N must be less than 65535.");

    using ValueType = CharT;
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

private:
    ValueType m_data[N + 1]{};
    SizeType m_size{ 0u };

public:
    /// @brief Default constructor. Initializes an empty fixed string.
    constexpr BasicFixedString() noexcept = default;

    /// @brief Constructs a fixed string from a string view.
    /// @param[in] view The string view to construct from.
    constexpr BasicFixedString(BasicStringView<CharT> view) noexcept
    {
        assign(view);
    }

    /// @brief Constructs a fixed string from a null-terminated string.
    /// @param[in] str The null-terminated string to construct from.
    constexpr BasicFixedString(const CharT* str) noexcept
    {
        assign(BasicStringView<CharT>(str));
    }

public:
    /// @brief Gets a reference to the character at the specified position.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A reference to the character at the specified position.
    [[nodiscard]] constexpr Reference operator[](SizeType pos) noexcept
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Gets a const reference to the character at the specified position.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A const reference to the character at the specified position.
    [[nodiscard]] constexpr ConstReference operator[](SizeType pos) const noexcept
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Gets a pointer to the underlying character array.
    /// @return A pointer to the underlying character array.
    [[nodiscard]] constexpr Pointer operator*() noexcept
    {
        return m_data;
    }

    /// @brief Gets a const pointer to the underlying character array.
    /// @return A const pointer to the underlying character array.
    [[nodiscard]] constexpr ConstPointer operator*() const noexcept
    {
        return m_data;
    }

    /// @brief Compares this string with another string view for equality.
    /// @param[in] other The string view to compare with.
    /// @return true if the strings are equal, false otherwise.
    [[nodiscard]] constexpr bool operator==(const BasicStringView<CharT>& other) const noexcept
    {
        return static_cast<BasicStringView<CharT>>(*this) == other;
    }

    /// @brief Compares this string with another string view for ordering.
    /// @param[in] other The string view to compare with.
    /// @return A value indicating the relative ordering of the strings.
    [[nodiscard]] constexpr auto operator<=>(const BasicStringView<CharT>& other) const noexcept
    {
        return static_cast<BasicStringView<CharT>>(*this) <=> other;
    }

    /// @brief Compares this string with another fixed string for equality.
    /// @param[in] other The fixed string to compare with.
    /// @return true if the strings are equal, false otherwise.
    template <gp::USize M>
    [[nodiscard]] constexpr bool operator==(const BasicFixedString<CharT, M>& other) const noexcept
    {
        return static_cast<BasicStringView<CharT>>(*this) == static_cast<BasicStringView<CharT>>(other);
    }

    /// @brief Compares this string with another fixed string for ordering.
    /// @param[in] other The fixed string to compare with.
    /// @return A value indicating the relative ordering of the strings.
    template <gp::USize M>
    [[nodiscard]] constexpr auto operator<=>(const BasicFixedString<CharT, M>& other) const noexcept
    {
        return static_cast<BasicStringView<CharT>>(*this) <=> static_cast<BasicStringView<CharT>>(other);
    }

    /// @brief Converts this fixed string to a string view.
    /// @return A string view representing the contents of this fixed string.
    [[nodiscard]] constexpr operator BasicStringView<CharT>() const noexcept
    {
        return BasicStringView<CharT>(m_data, m_size);
    }

    /// @brief Appends a string view to this fixed string.
    /// @param[in] view The string view to append.
    /// @return A reference to this fixed string.
    constexpr BasicFixedString& operator+=(BasicStringView<CharT> view) noexcept
    {
        append(view);
        return *this;
    }

    /// @brief Appends a character to this fixed string.
    /// @param[in] ch The character to append.
    /// @return A reference to this fixed string.
    BasicFixedString& operator+=(CharT ch) noexcept
    {
        pushBack(ch);
        return *this;
    }

public:
    /// @brief Gets a reference to the character at the specified position with bounds checking.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A reference to the character at the specified position.
    [[nodiscard]] constexpr Reference at(SizeType pos)
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Gets a const reference to the character at the specified position with bounds checking.
    /// @param[in] pos The position of the character to retrieve.
    /// @return A const reference to the character at the specified position.
    [[nodiscard]] constexpr ConstReference at(SizeType pos) const
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
        return m_data[m_size - 1];
    }

    /// @brief Gets a const reference to the last character in the string.
    /// @return A const reference to the last character in the string.
    [[nodiscard]] constexpr ConstReference back() const noexcept
    {
        GP_ASSERT(!isEmpty());
        return m_data[m_size - 1];
    }

    /// @brief Checks if the string is empty.
    /// @return true if the string is empty, false otherwise.
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

    /// @brief Gets the maximum number of characters the string can hold, excluding the null terminator.
    /// @return The maximum number of characters the string can hold, excluding the null terminator.
    [[nodiscard]] constexpr SizeType capacity() const noexcept
    {
        return N;
    }

    /// @brief Gets the maximum number of characters the string can hold, excluding the null terminator.
    /// @return The maximum number of characters the string can hold, excluding the null terminator.
    [[nodiscard]] constexpr SizeType maxSize() const noexcept
    {
        return N;
    }

    /// @brief Gets a pointer to the underlying character array.
    /// @return A pointer to the underlying character array.
    [[nodiscard]] constexpr ConstPointer data() const noexcept
    {
        return m_data;
    }

    /// @brief Gets a pointer to the underlying character array.
    /// @return A pointer to the underlying character array.
    [[nodiscard]] constexpr Pointer data() noexcept
    {
        return m_data;
    }

    /// @brief Converts this fixed string to a string view.
    /// @return A string view representing the contents of this fixed string.
    [[nodiscard]] constexpr BasicStringView<CharT> view() const noexcept
    {
        return BasicStringView<CharT>(m_data, m_size);
    }

    /// @brief Gets a const pointer to the underlying null-terminated character array.
    /// @return A const pointer to the underlying null-terminated character array.
    [[nodiscard]] constexpr ConstPointer cString() const noexcept
    {
        return m_data;
    }

    /// @brief Gets a pointer to the underlying null-terminated character array.
    /// @return A pointer to the underlying null-terminated character array.
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

    /// @brief Gets a const reverse iterator to the beginning of the string.
    /// @return A const reverse iterator to the beginning of the string.
    [[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Gets a reverse iterator to the beginning of the string.
    /// @return A reverse iterator to the beginning of the string.
    [[nodiscard]] constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Gets a const reverse iterator to the end of the string.
    /// @return A const reverse iterator to the end of the string.
    [[nodiscard]] constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Gets a reverse iterator to the end of the string.
    /// @return A reverse iterator to the end of the string.
    [[nodiscard]] constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Gets a const reverse iterator to the beginning of the string.
    /// @return A const reverse iterator to the beginning of the string.
    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Gets a const reverse iterator to the end of the string.
    /// @return A const reverse iterator to the end of the string.
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Clears the string, setting its size to zero and null-terminating it.
    /// @note This does not deallocate any memory, as the string is stack-allocated and has a fixed capacity.
    constexpr void clear() noexcept
    {
        m_size = 0u;
        m_data[0] = CharT{ 0 };
    }

    /// @brief Assigns the contents of a string view to this fixed string.
    /// @param[in] view The string view to assign from.
    constexpr void assign(BasicStringView<CharT> view) noexcept
    {
        GP_ASSERT(view.size() <= N && "FixedString capacity exceeded");
        m_size = static_cast<SizeType>(view.size());

        if (m_size > 0)
        {
            std::copy_n(view.data(), m_size, m_data);
        }
        m_data[m_size] = CharT{ 0 };
    }

    /// @brief Appends the contents of a string view to this fixed string.
    /// @param[in] view The string view to append.
    constexpr void append(BasicStringView<CharT> view) noexcept
    {
        GP_ASSERT(m_size + view.size() <= N && "FixedString capacity exceeded");

        if (!view.isEmpty())
        {
            std::copy_n(view.data(), view.size(), m_data + m_size);
            m_size += static_cast<SizeType>(view.size());
            m_data[m_size] = CharT{ 0 };
        }
    }

    /// @brief Appends a character to this fixed string.
    /// @param[in] ch The character to append.
    constexpr void pushBack(CharT ch) noexcept
    {
        GP_ASSERT(m_size < N && "FixedString capacity exceeded");
        m_data[m_size++] = ch;
        m_data[m_size] = CharT{ 0 };
    }

    /// @brief Removes the last character from this fixed string.
    /// @note This does not deallocate any memory, as the string is stack-allocated and has a fixed capacity.
    constexpr void popBack() noexcept
    {
        GP_ASSERT(m_size > 0 && "Cannot pop from an empty string");
        --m_size;
        m_data[m_size] = CharT{ 0 };
    }
};

}   // namespace gp::container

namespace gp
{

/// @brief Owning, stack-allocated string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedString = container::BasicFixedString<char, N>;

/// @brief Owning, stack-allocated string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedWString = container::BasicFixedString<wchar_t, N>;

/// @brief Owning, stack-allocated string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedU8String = container::BasicFixedString<char8_t, N>;

/// @brief Owning, stack-allocated string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedU16String = container::BasicFixedString<char16_t, N>;

/// @brief Owning, stack-allocated string with a fixed capacity.
/// @tparam N The maximum number of characters the string can hold, excluding the null terminator.
template <gp::USize N>
using FixedU32String = container::BasicFixedString<char32_t, N>;

}   // namespace gp
