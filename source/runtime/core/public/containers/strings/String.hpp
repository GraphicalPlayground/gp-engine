// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"
#include "CoreMinimal.hpp"

namespace gp::container
{

template <concepts::IsCharacter CharT>
class BasicString
{
public:
    using ValueType = CharT;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Iterator = Pointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
    [[nodiscard]] constexpr Reference operator[](SizeType pos) noexcept;
    [[nodiscard]] constexpr ConstReference operator[](SizeType pos) const noexcept;

    [[nodiscard]] constexpr bool operator==(const BasicString& other) const noexcept;
    [[nodiscard]] constexpr auto operator<=>(const BasicString& other) const noexcept;

    [[nodiscard]] constexpr bool operator==(const BasicStringView<CharT>& other) const noexcept;
    [[nodiscard]] constexpr auto operator<=>(const BasicStringView<CharT>& other) const noexcept;

    [[nodiscard]] constexpr bool operator==(const CharT* other) const noexcept;
    [[nodiscard]] constexpr auto operator<=>(const CharT* other) const noexcept;

    [[nodiscard]] constexpr Pointer operator*() noexcept;
    [[nodiscard]] constexpr ConstPointer operator*() const noexcept;

public:
    [[nodiscard]] constexpr Reference at(SizeType pos);
    [[nodiscard]] constexpr ConstReference at(SizeType pos) const;

    [[nodiscard]] constexpr bool isEmpty() const noexcept;
    [[nodiscard]] constexpr SizeType size() const noexcept;
    [[nodiscard]] constexpr SizeType length() const noexcept;
    [[nodiscard]] constexpr SizeType capacity() const noexcept;
    [[nodiscard]] constexpr SizeType maxSize() const noexcept;

    [[nodiscard]] constexpr ConstPointer data() const noexcept;
    [[nodiscard]] constexpr Pointer data() noexcept;

    [[nodiscard]] constexpr ConstIterator begin() const noexcept;
    [[nodiscard]] constexpr Iterator begin() noexcept;
    [[nodiscard]] constexpr ConstIterator end() const noexcept;
    [[nodiscard]] constexpr Iterator end() noexcept;

    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept;
    [[nodiscard]] constexpr ConstIterator cend() const noexcept;

    [[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept;
    [[nodiscard]] constexpr ReverseIterator rbegin() noexcept;
    [[nodiscard]] constexpr ConstReverseIterator rend() const noexcept;
    [[nodiscard]] constexpr ReverseIterator rend() noexcept;

    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept;
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept;
};

}   // namespace gp::container

namespace gp
{

/// @brief Owning, read/write `char` string.
using String = container::BasicString<char>;

/// @brief Owning, read/write `wchar_t` string.
using WString = container::BasicString<wchar_t>;

/// @brief Owning, read/write `char8_t` string.
using U8String = container::BasicString<char8_t>;

/// @brief Owning, read/write `char16_t` string.
using U16String = container::BasicString<char16_t>;

/// @brief Owning, read/write `char32_t` string.
using U32String = container::BasicString<char32_t>;

}   // namespace gp
