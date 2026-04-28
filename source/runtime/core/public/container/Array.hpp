// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "macros/MacroUtilities.hpp"
#include <algorithm>
#include <compare>   // IWYU pragma: keep
#include <cstring>
#include <iterator>
#include <ranges>
#include <type_traits>

namespace gp
{

/// @brief Fixed-size array with stack allocation and constexpr support.
/// @details
/// Provides a simple wrapper around a C-style array with added functionality and safety checks. The size of the array
/// is fixed at compile time, and all elements are stored contiguously on the stack. The class supports aggregate
/// initialization, range-based for loops, and standard container interfaces for interoperability with <algorithm>.
/// Boundary checks are performed in debug builds, but not in release builds. Accessing out-of-bounds indices results in
/// undefined behavior.
template <typename T, gp::USize N>
class Array
{
    static_assert(!std::is_reference_v<T>, "Array elements cannot be references");
    static_assert(std::is_destructible_v<T>, "Array elements must be destructible");
    static_assert(
        std::is_trivially_copyable_v<T> || std::is_nothrow_constructible_v<T, T&&>,
        "Array elements must be trivially copyable or nothrow move constructible"
    );

public:
    using ValueType = T;
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
    static constexpr SizeType arraySize = N;
    static constexpr SizeType byteSize = N * sizeof(T);
    static constexpr SizeType npos = static_cast<SizeType>(-1);

private:
    // clang-format off
    GP_ALIGN(T) T m_data[N > 0 ? N : 1]{};   // Ensure at least one element for zero-sized arrays
    // clang-format on

public:
    /// @brief Constructs an empty Array, where all elements are value-initialized.
    constexpr Array() noexcept = default;

    /// @brief Copy constructor and copy assignment operator.
    constexpr Array(const Array&) noexcept = default;
    constexpr Array& operator=(const Array&) noexcept = default;

    /// @brief Move constructor and move assignment operator.
    constexpr Array(Array&&) noexcept = default;
    constexpr Array& operator=(Array&&) noexcept = default;

    /// @brief Constructs an Array with the specified arguments.
    /// @warning If the number of arguments is less than the size of the array, the remaining elements will be
    ///          value-initialized.
    /// @param[in] args The arguments to initialize the elements of the array.
    template <typename... U>
    requires(sizeof...(U) > 0 && sizeof...(U) <= N && (std::is_convertible_v<U, T> && ...))
    constexpr Array(U&&... args) noexcept
        : m_data{ std::forward<U>(args)... }
    {}

public:
    /// @brief Accesses the element at the specified index.
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing out-of-bounds
    /// indices results in undefined behavior.
    /// @param[in] index The index of the element to access.
    /// @return A reference to the element at the specified index.
    GP_NODISCARD constexpr Reference operator[](SizeType index) noexcept
    {
        GP_ASSERT(index < N, "Array index out of bounds");
        return m_data[index];
    }

    /// @brief Accesses the element at the specified index (const version).
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing out-of-bounds
    /// indices results in undefined behavior.
    /// @param[in] index The index of the element to access.
    /// @return A const reference to the element at the specified index.
    GP_NODISCARD constexpr ConstReference operator[](SizeType index) const noexcept
    {
        GP_ASSERT(index < N, "Array index out of bounds");
        return m_data[index];
    }

    /// @brief Three-way comparison result between this array and another array.
    ///        Generates ==, !=, <, >, <=, >=.
    /// @param[in] other The array to compare with.
    /// @return A three-way comparison result indicating the relative order of the two arrays.
    friend constexpr auto operator<=>(const Array&, const Array&) = default;

public:
    /// @brief Accesses the element at the specified index.
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing out-of-bounds
    /// indices results in undefined behavior.
    /// @param[in] index The index of the element to access.
    /// @return A reference to the element at the specified index.
    GP_NODISCARD constexpr Reference at(SizeType index) noexcept
    {
        GP_ASSERT(index < N, "Array index out of bounds");
        return m_data[index];
    }

    /// @brief Accesses the element at the specified index (const version).
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing out-of-bounds
    /// indices results in undefined behavior.
    /// @param[in] index The index of the element to access.
    /// @return A const reference to the element at the specified index.
    GP_NODISCARD constexpr ConstReference at(SizeType index) const noexcept
    {
        GP_ASSERT(index < N, "Array index out of bounds");
        return m_data[index];
    }

    /// @brief Returns a reference to the first element in the array.
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing the first element
    ///          of an empty array results in undefined behavior.
    /// @return A reference to the first element in the array.
    GP_NODISCARD constexpr Reference front() noexcept
    {
        GP_ASSERT(N > 0, "Array is empty");
        return m_data[0];
    }

    /// @brief Returns a const reference to the first element in the array.
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing the first element
    ///          of an empty array results in undefined behavior.
    /// @return A const reference to the first element in the array.
    GP_NODISCARD constexpr ConstReference front() const noexcept
    {
        GP_ASSERT(N > 0, "Array is empty");
        return m_data[0];
    }

    /// @brief Returns a reference to the last element in the array.
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing the last element of
    ///          an empty array results in undefined behavior.
    /// @return A reference to the last element in the array.
    GP_NODISCARD constexpr Reference back() noexcept
    {
        GP_ASSERT(N > 0, "Array is empty");
        return m_data[N - 1];
    }

    /// @brief Returns a const reference to the last element in the array.
    /// @warning Boundary checks are performed in debug builds, but not in release builds. Accessing the last element of
    ///          an empty array results in undefined behavior.
    /// @return A const reference to the last element in the array.
    GP_NODISCARD constexpr ConstReference back() const noexcept
    {
        GP_ASSERT(N > 0, "Array is empty");
        return m_data[N - 1];
    }

    /// @brief Returns a pointer to the underlying array.
    /// @return A pointer to the underlying array, or `nullptr` if the array is empty.
    GP_NODISCARD constexpr Pointer data() noexcept
    {
        return N > 0 ? m_data : nullptr;
    }

    /// @brief Returns a pointer to the underlying array.
    /// @return A pointer to the underlying array, or `nullptr` if the array is empty.
    GP_NODISCARD constexpr ConstPointer data() const noexcept
    {
        return N > 0 ? m_data : nullptr;
    }

    /// @brief Returns the number of elements in the array.
    /// @return The number of elements in the array.
    GP_NODISCARD constexpr SizeType size() const noexcept
    {
        return N;
    }

    /// @brief Returns the maximum number of elements that the array can hold.
    /// @return The maximum number of elements that the array can hold.
    GP_NODISCARD constexpr SizeType capacity() const noexcept
    {
        return N;
    }

    /// @brief Checks if the array is empty.
    /// @return True if the array is empty, false otherwise.
    GP_NODISCARD constexpr bool empty() const noexcept
    {
        return N == 0;
    }

    /// @brief Fills the array with the specified value.
    /// @param[in] value The value to fill the array with.
    constexpr void fill(const T& value) noexcept
    {
        std::ranges::fill(m_data, value);
    }

    /// @brief Clears the array by setting all elements to their default value.
    constexpr void clear() noexcept
    {
        std::ranges::fill(m_data, T{});
    }

    /// @brief Returns an iterator to the beginning of the array.
    /// @return An iterator to the beginning of the array.
    GP_NODISCARD constexpr Iterator begin() noexcept
    {
        return data();
    }

    /// @brief Returns a const iterator to the beginning of the array.
    /// @return A const iterator to the beginning of the array.
    GP_NODISCARD constexpr ConstIterator begin() const noexcept
    {
        return data();
    }

    /// @brief Returns an iterator to the end of the array.
    /// @return An iterator to the end of the array.
    GP_NODISCARD constexpr Iterator end() noexcept
    {
        return data() + N;
    }

    /// @brief Returns a const iterator to the end of the array.
    /// @return A const iterator to the end of the array.
    GP_NODISCARD constexpr ConstIterator end() const noexcept
    {
        return data() + N;
    }

    /// @brief Returns a reverse iterator to the beginning of the array.
    /// @return A reverse iterator to the beginning of the array.
    GP_NODISCARD constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator to the beginning of the array.
    /// @return A const reverse iterator to the beginning of the array.
    GP_NODISCARD constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a reverse iterator to the end of the array.
    /// @return A reverse iterator to the end of the array.
    GP_NODISCARD constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator to the end of the array.
    /// @return A const reverse iterator to the end of the array.
    GP_NODISCARD constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Returns a const iterator to the beginning of the array.
    /// @return A const iterator to the beginning of the array.
    GP_NODISCARD constexpr ConstIterator cbegin() const noexcept
    {
        return data();
    }

    /// @brief Returns a const iterator to the end of the array.
    /// @return A const iterator to the end of the array.
    GP_NODISCARD constexpr ConstIterator cend() const noexcept
    {
        return data() + N;
    }

    /// @brief Returns a const reverse iterator to the beginning of the array.
    /// @return A const reverse iterator to the beginning of the array.
    GP_NODISCARD constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator to the end of the array.
    /// @return A const reverse iterator to the end of the array.
    GP_NODISCARD constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Finds the first occurrence of the specified value in the array.
    /// @param[in] value The value to search for.
    /// @return An iterator to the first occurrence of the specified value in the array, or `end()` if the value is not
    ///         found.
    GP_NODISCARD constexpr Iterator find(const T& value) noexcept
    {
        return std::ranges::find(m_data, value);
    }

    /// @brief Finds the first occurrence of the specified value in the array (const version).
    /// @param[in] value The value to search for.
    /// @return An iterator to the first occurrence of the specified value in the array, or `end()` if the value is not
    ///         found.
    GP_NODISCARD constexpr ConstIterator find(const T& value) const noexcept
    {
        return std::ranges::find(m_data, value);
    }

    /// @brief Checks if the array contains the specified value.
    /// @param[in] value The value to search for.
    /// @return True if the array contains the specified value, false otherwise.
    GP_NODISCARD constexpr bool contains(const T& value) const noexcept
    {
        return find(value) != end();
    }

    /// @brief Counts the number of occurrences of a value in the array.
    /// @param[in] value The value to count.
    /// @return The number of occurrences of the specified value in the array.
    GP_NODISCARD constexpr SizeType count(const T& value) const noexcept
    {
        return static_cast<SizeType>(std::ranges::count(m_data, value));
    }

    /// @brief Finds the first index of the specified value in the array.
    /// @param[in] value The value to search for.
    /// @return The first index of the specified value in the array, or `npos` if the value is not found.
    GP_NODISCARD constexpr SizeType indexOf(const T& value) const noexcept
    {
        auto it = std::ranges::find(m_data, value);
        return it != end() ? static_cast<SizeType>(it - begin()) : npos;
    }

    /// @brief Finds the last index of the specified value in the array.
    /// @param[in] value The value to search for.
    /// @return The last index of the specified value in the array, or `npos` if the value is not found.
    GP_NODISCARD constexpr SizeType lastIndexOf(const T& value) const noexcept
    {
        auto it = std::ranges::find(std::views::reverse(m_data), value);
        return it != std::views::reverse(m_data).end() ? static_cast<SizeType>(it.base() - begin() - 1) : npos;
    }

    /// @brief Finds the first index of an element that does not match the specified value.
    /// @param[in] value The value to compare against.
    /// @return The first index of an element that does not match the specified value, or `npos` if no such element is
    ///         found.
    GP_NODISCARD constexpr SizeType indexNotOf(const T& value) const noexcept
    {
        auto it = std::ranges::find_if_not(
            m_data,
            [&value](const T& elem)
            {
                return elem == value;
            }
        );
        return it != end() ? static_cast<SizeType>(it - begin()) : npos;
    }

    /// @brief Finds the last index of an element that does not match the specified value.
    /// @param[in] value The value to compare against.
    /// @return The last index of an element that does not match the specified value, or `npos` if no such element is
    ///         found.
    GP_NODISCARD constexpr SizeType lastIndexNotOf(const T& value) const noexcept
    {
        auto it = std::ranges::find_if_not(
            std::views::reverse(m_data),
            [&value](const T& elem)
            {
                return elem == value;
            }
        );
        return it != std::views::reverse(m_data).end() ? static_cast<SizeType>(it.base() - begin() - 1) : npos;
    }

    /// @brief Swaps the contents of this array with another array.
    /// @param[in] other The array to swap with.
    constexpr void swap(Array& other) noexcept
    {
        if constexpr (N > 0)
        {
            std::ranges::swap_ranges(m_data, other.m_data);
        }
    }
};

/// @brief Swaps the contents of two arrays.
/// @param[in] lhs The first array to swap.
/// @param[in] rhs The second array to swap.
template <typename T, gp::USize N>
constexpr void swap(Array<T, N>& lhs, Array<T, N>& rhs) noexcept
{
    lhs.swap(rhs);
}

}   // namespace gp
