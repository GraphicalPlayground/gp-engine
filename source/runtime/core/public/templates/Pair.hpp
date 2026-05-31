// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "concepts/Construction.hpp"
#include "CoreMinimal.hpp"
#include <compare>

namespace gp
{

/// @brief A simple struct that holds two values, potentially of different types. Provides constructors, assignment
///        operators, comparison operators, and a swap function. Similar to std::pair but with additional features and
///        optimizations for GP's use cases.
/// @tparam T1 Type of the first value.
/// @tparam T2 Type of the second value.
template <typename T1, typename T2>
struct Pair
{
public:
    using FirstType = T1;
    using SecondType = T2;
    using ThisType = Pair<T1, T2>;

public:
    T1 first;
    T2 second;

public:
    /// @brief Default constructor, value-initializes both elements.
    GP_NODISCARD constexpr Pair() requires(concepts::IsDefaultConstructible<T1> && concepts::IsDefaultConstructible<T2>)
        : first()
        , second()
    {}

    /// @brief Constructs a pair from two values.
    /// @param inFirst First element.
    /// @param inSecond Second element.
    GP_NODISCARD constexpr Pair(const T1& inFirst, const T2& inSecond) noexcept(
        noexcept(T1(inFirst)) && noexcept(T2(inSecond))
    )
        : first(inFirst)
        , second(inSecond)
    {}

    /// @brief Constructs a pair from two values using move semantics.
    /// @param inFirst First element (moved).
    /// @param inSecond Second element (moved).
    GP_NODISCARD constexpr Pair(T1&& inFirst, T2&& inSecond) noexcept(
        noexcept(T1(std::move(inFirst))) && noexcept(T2(std::move(inSecond)))
    )
        : first(std::move(inFirst))
        , second(std::move(inSecond))
    {}

    /// @brief Constructs a pair from two values of potentially different types, using perfect forwarding.
    /// @tparam U Type of the second value, which may differ from T2 but must be convertible to it.
    /// @tparam V Type of the second value, which may differ from T2 but must be convertible to it.
    /// @param inFirst First element, perfectly forwarded to construct T1.
    /// @param inSecond Second element, perfectly forwarded to construct T2.
    template <typename U, typename V>
    requires concepts::IsConstructibleWith<T1, U&&> && concepts::IsConstructibleWith<T2, V&&>
    GP_NODISCARD constexpr Pair(U&& inFirst, V&& inSecond) noexcept(
        noexcept(T1(std::forward<U>(inFirst))) && noexcept(T2(std::forward<V>(inSecond)))
    )
        : first(std::forward<U>(inFirst))
        , second(std::forward<V>(inSecond))
    {}

    /// @brief Default copy constructor.
    GP_NODISCARD constexpr Pair(const Pair&) = default;

    /// @brief Default move constructor.
    GP_NODISCARD constexpr Pair(Pair&&) = default;

public:
    /// @brief Default copy assignment operator.
    /// @return Reference to the current pair instance.
    constexpr Pair& operator=(const Pair&) = default;

    /// @brief Default move assignment operator.
    /// @return Reference to the current pair instance.
    constexpr Pair& operator=(Pair&&) = default;

    /// @brief Equality comparison operator, compares both elements for equality.
    /// @param other Pair to compare against.
    /// @return true if both elements are equal.
    GP_NODISCARD constexpr bool operator==(const Pair& other) const noexcept
        requires concepts::IsEqualityComparable<T1> && concepts::IsEqualityComparable<T2>
    {
        return first == other.first && second == other.second;
    }

    /// @brief Inequality comparison operator, negates the result of operator==.
    /// @param other Pair to compare against.
    /// @return true if any element differs.
    GP_NODISCARD constexpr bool operator!=(const Pair& other) const noexcept
        requires concepts::IsEqualityComparable<T1> && concepts::IsEqualityComparable<T2>
    {
        return !(*this == other);
    }

    /// @brief Three-way comparison operator, compares first elements and then second elements if first are equal.
    /// @param other Pair to compare against.
    /// @return Comparison result based on the first differing element, or the common comparison category equal if
    ///         both elements are equal.
    GP_NODISCARD constexpr auto operator<=>(const Pair& other) const noexcept
        requires concepts::IsThreeWayComparable<T1> && concepts::IsThreeWayComparable<T2>
    {
        using Category = std::common_comparison_category_t<
            decltype(std::declval<const T1&>() <=> std::declval<const T1&>()),
            decltype(std::declval<const T2&>() <=> std::declval<const T2&>())>;
        if (const Category cmp = first <=> other.first; cmp != 0)
        {
            return cmp;
        }
        return Category(second <=> other.second);
    }

public:
    /// @brief Swaps the contents of this pair with another pair.
    /// @param other Pair to swap with.
    constexpr void swap(Pair& other) noexcept
    {
        std::swap(first, other.first);
        std::swap(second, other.second);
    }
};

/// @brief Class template argument deduction guide for Pair, allowing construction from two values without specifying
///        template parameters explicitly.
/// @tparam A Type of the first value, deduced from the constructor arguments.
/// @tparam B Type of the second value, deduced from the constructor arguments.
template <typename A, typename B>
Pair(A, B) -> Pair<A, B>;

/// @brief Factory function to create a Pair from two values, with template argument deduction and perfect forwarding.
/// @tparam T1 Type of the first value, deduced from the function arguments.
/// @tparam T2 Type of the second value, deduced from the function arguments.
/// @param a First value, perfectly forwarded to construct the first element of the pair.
/// @param b Second value, perfectly forwarded to construct the second element of the pair.
/// @return A Pair containing the provided values.
template <typename T1, typename T2>
GP_NODISCARD constexpr Pair<std::decay_t<T1>, std::decay_t<T2>> makePair(T1&& a, T2&& b) noexcept(
    noexcept(Pair<std::decay_t<T1>, std::decay_t<T2>>(std::forward<T1>(a), std::forward<T2>(b)))
)
{
    return Pair<std::decay_t<T1>, std::decay_t<T2>>(std::forward<T1>(a), std::forward<T2>(b));
}

}   // namespace gp
