// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Concepts/ComparisonConcepts.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Core/Decay.hpp"
#include "Templates/Core/Utility.hpp"
#include <compare>

namespace GP
{

/// @brief A simple struct that holds two values, potentially of different types. Provides constructors, assignment
///        operators, comparison operators, and a swap function. Similar to std::pair but with additional features and
///        optimizations for GP's use cases.
/// @tparam TFirst Type of the first value.
/// @tparam TSecond Type of the second value.
template <typename TFirst, typename TSecond>
class TPair
{
public:
    using FirstType = TFirst;
    using SecondType = TSecond;
    using ThisType = TPair<TFirst, TSecond>;

public:
    TFirst first;     //<! First element of the pair.
    TSecond second;   //<! Second element of the pair.

public:
    /// @brief Default constructor, value-initializes both elements.
    constexpr TPair() requires(CDefaultConstructible<TFirst> && CDefaultConstructible<TSecond>)
        : first()
        , second()
    {}

    /// @brief Constructs a pair from two values.
    /// @param inFirst First element.
    /// @param inSecond Second element.
    constexpr TPair(const TFirst& inFirst, const TSecond& inSecond) noexcept(
        noexcept(TFirst(inFirst)) && noexcept(TSecond(inSecond))
    )
        : first(inFirst)
        , second(inSecond)
    {}

    /// @brief Constructs a pair from two values using move semantics.
    /// @param inFirst First element (moved).
    /// @param inSecond Second element (moved).
    constexpr TPair(TFirst&& inFirst, TSecond&& inSecond) noexcept(
        noexcept(TFirst(GP::Move(inFirst))) && noexcept(TSecond(GP::Move(inSecond)))
    )
        : first(GP::Move(inFirst))
        , second(GP::Move(inSecond))
    {}

    /// @brief Constructs a pair from two values of potentially different types, using perfect forwarding.
    /// @tparam U Type of the second value, which may differ from TSecond but must be convertible to it.
    /// @tparam V Type of the second value, which may differ from TSecond but must be convertible to it.
    /// @param inFirst First element, perfectly forwarded to construct TFirst.
    /// @param inSecond Second element, perfectly forwarded to construct TSecond.
    template <typename U, typename V>
    requires CConstructibleFrom<TFirst, U&&> && CConstructibleFrom<TSecond, V&&> constexpr TPair(
        U&& inFirst, V&& inSecond
    ) noexcept(noexcept(TFirst(GP::Forward<U>(inFirst))) && noexcept(TSecond(GP::Forward<V>(inSecond))))
        : first(GP::Forward<U>(inFirst))
        , second(GP::Forward<V>(inSecond))
    {}

    /// @brief Default copy constructor.
    constexpr TPair(const TPair&) = default;

    /// @brief Default move constructor.
    constexpr TPair(TPair&&) = default;

public:
    /// @brief Default copy assignment operator.
    /// @return Reference to the current pair instance.
    constexpr TPair& operator=(const TPair&) = default;

    /// @brief Default move assignment operator.
    /// @return Reference to the current pair instance.
    constexpr TPair& operator=(TPair&&) = default;

    /// @brief Equality comparison operator, compares both elements for equality.
    /// @param other Pair to compare against.
    /// @return true if both elements are equal.
    GP_NODISCARD constexpr bool operator==(const TPair& other) const noexcept
        requires CEqualityComparable<TFirst> && CEqualityComparable<TSecond>
    {
        return first == other.first && second == other.second;
    }

    /// @brief Inequality comparison operator, negates the result of operator==.
    /// @param other Pair to compare against.
    /// @return true if any element differs.
    GP_NODISCARD constexpr bool operator!=(const TPair& other) const noexcept
        requires CEqualityComparable<TFirst> && CEqualityComparable<TSecond>
    {
        return !(*this == other);
    }

    /// @brief Three-way comparison operator, compares first elements and then second elements if first are equal.
    /// @param other Pair to compare against.
    /// @return Comparison result based on the first differing element, or the common comparison category equal if
    ///         both elements are equal.
    GP_NODISCARD constexpr auto operator<=>(const TPair& other) const noexcept
        requires CThreeWayComparable<TFirst> && CThreeWayComparable<TSecond>
    {
        using Cat = std::common_comparison_category_t<
            decltype(GP::Declval<const TFirst&>() <=> GP::Declval<const TFirst&>()),
            decltype(GP::Declval<const TSecond&>() <=> GP::Declval<const TSecond&>())>;
        if (const Cat cmp = first <=> other.first; cmp != 0) { return cmp; }
        return Cat(second <=> other.second);
    }

public:
    /// @brief Swaps the contents of this pair with another pair.
    /// @param other Pair to swap with.
    constexpr void Swap(TPair& other) noexcept
    {
        GP::Swap(first, other.first);
        GP::Swap(second, other.second);
    }
};

/// @brief Class template argument deduction guide for TPair, allowing construction from two values without specifying
///        template parameters explicitly.
/// @tparam A Type of the first value, deduced from the constructor arguments.
/// @tparam B Type of the second value, deduced from the constructor arguments.
template <typename A, typename B>
TPair(A, B) -> TPair<A, B>;

/// @brief Factory function to create a TPair from two values, with template argument deduction and perfect forwarding.
/// @tparam A Type of the first value, deduced from the function arguments.
/// @tparam B Type of the second value, deduced from the function arguments.
/// @param a First value, perfectly forwarded to construct the first element of the pair.
/// @param b Second value, perfectly forwarded to construct the second element of the pair.
/// @return A TPair containing the provided values.
template <typename A, typename B>
GP_NODISCARD constexpr TPair<TDecay_T<A>, TDecay_T<B>>
    MakePair(A&& a, B&& b) noexcept(noexcept(TPair<TDecay_T<A>, TDecay_T<B>>(GP::Forward<A>(a), GP::Forward<B>(b))))
{
    return TPair<TDecay_T<A>, TDecay_T<B>>(GP::Forward<A>(a), GP::Forward<B>(b));
}

}   // namespace GP
