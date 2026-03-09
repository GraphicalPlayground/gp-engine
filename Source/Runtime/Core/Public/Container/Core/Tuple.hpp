// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Concepts/ComparisonConcepts.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Core/Decay.hpp"
#include "Templates/Core/Utility.hpp"
#include <compare>
#include <utility>

namespace GP
{

namespace Detail
{

/// @brief Leaf storage node for the Nth element of a tuple.
/// @tparam Index Compile-time index of the element in the tuple.
/// @tparam T Type of the element.
template <SizeT Index, typename T>
struct TTupleLeaf
{
public:
    T value{};   //<! Value of the tuple element, value-initialized by default.

public:
    /// @brief Default constructor, value-initializes the stored element.
    constexpr TTupleLeaf() = default;

    /// @brief Constructs the leaf by perfectly forwarding the given value into the stored element.
    /// @tparam U Type of the incoming value (deduced). Must be constructible to T.
    /// @param inValue Value used to initialize the stored element.
    template <typename U>
    requires CConstructibleFrom<T, U&&>
    constexpr explicit TTupleLeaf(U&& inValue) noexcept(noexcept(T(GP::Forward<U>(inValue))))
        : value(GP::Forward<U>(inValue))
    {}
};

/// @brief Flat storage base for TTuple; inherits directly from every TTupleLeaf using an unpacked
///        index sequence. A single variadic constructor pack is expanded simultaneously with
///        the index and type sequences so the compiler never sees two separate parameters of
///        the same type in the same function signature (which triggers a false-positive
///        "duplicate parameter name" diagnostic in some IntelliSense implementations).
template <typename Indices, typename... Ts>
struct TTupleStorage;

/// @brief Partial specialisation that unpacks the compile-time index sequence into direct
///        multiple-base inheritance from each TTupleLeaf.
/// @tparam Is  Compile-time indices for each element (0, 1, ..., N-1).
/// @tparam Ts  Element types corresponding to each index.
template <SizeT... Is, typename... Ts>
struct TTupleStorage<std::index_sequence<Is...>, Ts...> : TTupleLeaf<Is, Ts>...
{
public:
    /// @brief Default constructor, value-initializes every leaf.
    constexpr TTupleStorage() = default;

    /// @brief Constructs each leaf from the corresponding argument via perfect forwarding.
    ///        A single variadic pack is forwarded in lockstep with the index and type
    ///        sequences via a simultaneous pack expansion in the member-initializer-list.
    /// @tparam Args Argument types (deduced); count must equal sizeof...(Is).
    /// @param args Arguments forwarded into each leaf in index order.
    template <typename... Args>
    requires(sizeof...(Args) == sizeof...(Is) && (CConstructibleFrom<Ts, Args &&> && ...))
    constexpr explicit TTupleStorage(Args&&... args) noexcept((noexcept(TTupleLeaf<Is, Ts>(GP::Declval<Args>())) && ...)
    )
        : TTupleLeaf<Is, Ts>(GP::Forward<Args>(args))...
    {}
};

/// @brief Helper metafunction to retrieve the type at compile-time position Index in a parameter pack.
template <SizeT Index, typename... Ts>
struct TTypeAt;

/// @brief Recursive case for TTypeAt, peeling the head type until Index reaches zero.
template <SizeT Index, typename Head, typename... Tail>
struct TTypeAt<Index, Head, Tail...> : TTypeAt<Index - 1, Tail...>
{};

/// @brief Base case for TTypeAt; Index reached zero so Type is the current head.
template <typename Head, typename... Tail>
struct TTypeAt<0, Head, Tail...>
{
    using Type = Head;
};

/// @brief Alias template to extract the type at position Index from a parameter pack.
template <SizeT Index, typename... Ts>
using TTypeAtT = typename TTypeAt<Index, Ts...>::Type;

}   // namespace Detail

/// @brief Fixed-size heterogeneous collection of values. Provides compile-time indexed access, structured-binding
///        support, and full comparison operators. Equivalent to std::tuple without STL dependencies.
/// @tparam Ts Element types stored in the tuple.
template <typename... Ts>
class TTuple : public Detail::TTupleStorage<std::index_sequence_for<Ts...>, Ts...>
{
private:
    using Base = Detail::TTupleStorage<std::index_sequence_for<Ts...>, Ts...>;

public:
    /// @brief Number of elements in this tuple instantiation.
    static constexpr SizeT kSize = sizeof...(Ts);

public:
    /// @brief Default constructor, value-initializes all elements.
    /// @note Implicitly deleted by the compiler when any element type is not default-constructible.
    constexpr TTuple() = default;

    /// @brief Constructs the tuple by perfectly forwarding one argument per element type.
    /// @tparam Args Argument types (deduced). Count must match the number of element types.
    /// @param args Arguments forwarded into each element in order.
    template <typename... Args>
    requires(sizeof...(Args) == sizeof...(Ts) && (CConstructibleFrom<Ts, Args &&> && ...))
    constexpr explicit TTuple(Args&&... args) noexcept(noexcept(Base(GP::Declval<Args>()...)))
        : Base(GP::Forward<Args>(args)...)
    {}

    /// @note Copy and move constructors and assignment operators are defaulted to preserve value semantics.

    constexpr TTuple(const TTuple&) = default;
    constexpr TTuple(TTuple&&) = default;

public:
    constexpr TTuple& operator=(const TTuple&) = default;
    constexpr TTuple& operator=(TTuple&&) = default;

    /// @brief Equality operator; compares tuples element-wise in index order.
    /// @param other Tuple to compare against.
    /// @return True if every corresponding pair of elements compares equal.
    /// @note Requires all element types to satisfy CEqualityComparable.
    GP_NODISCARD constexpr bool operator==(const TTuple& other) const
        noexcept((noexcept(GP::Declval<const Ts&>() == GP::Declval<const Ts&>()) && ...))
            requires(CEqualityComparable<Ts> && ...)
    {
        return EqualImpl(other, std::index_sequence_for<Ts...>{});
    }

    /// @brief Inequality operator; negates operator==.
    /// @param other Tuple to compare against.
    /// @return True if any corresponding pair of elements is unequal.
    /// @note Requires all element types to satisfy CEqualityComparable.
    GP_NODISCARD constexpr bool operator!=(const TTuple& other) const
        noexcept((noexcept(GP::Declval<const Ts&>() == GP::Declval<const Ts&>()) && ...))
            requires(CEqualityComparable<Ts> && ...)
    {
        return !(*this == other);
    }

    /// @brief Three-way comparison operator; compares tuples lexicographically element-wise.
    ///        Returns the result for the first element pair that is not equivalent, or equivalent if all match.
    /// @param other Tuple to compare against.
    /// @return Comparison result in the common comparison category of all element types.
    /// @note Requires all element types to satisfy CThreeWayComparable.
    GP_NODISCARD constexpr auto operator<=>(const TTuple& other) const noexcept requires(CThreeWayComparable<Ts> && ...)
    {
        return ThreeWayImpl(other, std::index_sequence_for<Ts...>{});
    }

public:
    /// @brief Returns the number of elements in the tuple.
    /// @return Compile-time constant equal to sizeof...(Ts).
    GP_NODISCARD static constexpr SizeT Size() noexcept { return kSize; }

    /// @brief Returns a mutable reference to the element at compile-time index Index.
    /// @tparam Index Zero-based index of the requested element. Must be less than kSize.
    /// @return Mutable reference to the element.
    template <SizeT Index>
    requires(Index < kSize) GP_NODISCARD constexpr auto& Get() noexcept
    {
        return static_cast<Detail::TTupleLeaf<Index, Detail::TTypeAtT<Index, Ts...>>&>(*this).value;
    }

    /// @brief Returns a const reference to the element at compile-time index Index.
    /// @tparam Index Zero-based index of the requested element. Must be less than kSize.
    /// @return Const reference to the element.
    template <SizeT Index>
    requires(Index < kSize) GP_NODISCARD constexpr const auto& Get() const noexcept
    {
        return static_cast<const Detail::TTupleLeaf<Index, Detail::TTypeAtT<Index, Ts...>>&>(*this).value;
    }

    /// @brief Swaps the contents of this tuple with another tuple element-wise.
    /// @param other Tuple to swap with.
    constexpr void Swap(TTuple& other) noexcept((noexcept(GP::Swap(GP::Declval<Ts&>(), GP::Declval<Ts&>())) && ...))
    {
        SwapImpl(other, std::index_sequence_for<Ts...>{});
    }

private:
    /// @brief Compares all element pairs for equality using index pack expansion.
    /// @tparam Is Compile-time sequence of element indices.
    /// @param other Tuple to compare against.
    /// @return True if all corresponding element pairs are equal.
    template <SizeT... Is>
    constexpr bool EqualImpl(const TTuple& other, std::index_sequence<Is...>) const noexcept
    {
        return ((Get<Is>() == other.template Get<Is>()) && ...);
    }

    /// @brief Performs lexicographic three-way comparison using index pack expansion.
    /// @tparam Is Compile-time sequence of element indices.
    /// @param other Tuple to compare against.
    /// @return Comparison result in the common category of all element-pair results.
    template <SizeT... Is>
    constexpr auto ThreeWayImpl(const TTuple& other, std::index_sequence<Is...>) const noexcept
    {
        using Cat =
            std::common_comparison_category_t<decltype(GP::Declval<const Ts&>() <=> GP::Declval<const Ts&>())...>;
        Cat result = Cat::equivalent;
        ((result == Cat::equivalent ? result = (Get<Is>() <=> other.template Get<Is>()) : result), ...);
        return result;
    }

    /// @brief Swaps all element pairs using index pack expansion.
    /// @tparam Is Compile-time sequence of element indices.
    /// @param other Tuple to swap with.
    template <SizeT... Is>
    constexpr void SwapImpl(TTuple& other, std::index_sequence<Is...>) noexcept(
        (noexcept(GP::Swap(GP::Declval<Ts&>(), GP::Declval<Ts&>())) && ...)
    )
    {
        (GP::Swap(Get<Is>(), other.template Get<Is>()), ...);
    }
};

/// @brief Deduction guide for TTuple; deduces decayed element types from constructor arguments.
/// @tparam Ts Argument types (deduced); stored as TDecay_T<Ts> to strip references and cv-qualifiers.
template <typename... Ts>
TTuple(Ts...) -> TTuple<TDecay_T<Ts>...>;

/// @brief Free function accessor; returns a mutable reference to element at compile-time index Index.
/// @tparam Index Zero-based element index. Must be less than the tuple's size.
/// @tparam Ts Element types of the tuple.
/// @param tuple The tuple to access.
/// @return Mutable reference to the element at Index.
template <SizeT Index, typename... Ts>
GP_NODISCARD constexpr auto& Get(TTuple<Ts...>& tuple) noexcept
{
    return tuple.template Get<Index>();
}

/// @brief Free function accessor; returns a const reference to element at compile-time index Index.
/// @tparam Index Zero-based element index. Must be less than the tuple's size.
/// @tparam Ts Element types of the tuple.
/// @param tuple The tuple to access.
/// @return Const reference to the element at Index.
template <SizeT Index, typename... Ts>
GP_NODISCARD constexpr const auto& Get(const TTuple<Ts...>& tuple) noexcept
{
    return tuple.template Get<Index>();
}

/// @brief Free function accessor; returns an rvalue reference to element at compile-time index Index.
///        Allows moving elements out of a temporary or explicitly-moved tuple.
/// @tparam Index Zero-based element index. Must be less than the tuple's size.
/// @tparam Ts Element types of the tuple.
/// @param tuple Rvalue reference to the tuple.
/// @return Rvalue reference to the element at Index.
template <SizeT Index, typename... Ts>
GP_NODISCARD constexpr auto&& Get(TTuple<Ts...>&& tuple) noexcept
{
    return GP::Move(tuple.template Get<Index>());
}

/// @brief Factory function that constructs a TTuple from the provided arguments, deducing decayed element types.
/// @tparam Args Argument types (deduced); element types are TDecay_T<Args>.
/// @param args Values to store in the tuple, perfectly forwarded.
/// @return A TTuple<TDecay_T<Args>...> holding copies or moved values of the arguments.
template <typename... Args>
GP_NODISCARD constexpr TTuple<TDecay_T<Args>...> MakeTuple(Args&&... args
) noexcept((noexcept(TDecay_T<Args>(GP::Forward<Args>(args))) && ...))
{
    return TTuple<TDecay_T<Args>...>(GP::Forward<Args>(args)...);
}

/// @brief ADL-reachable get<I> accessor for the structured-binding protocol; returns a mutable reference.
/// @tparam Index Zero-based element index.
/// @tparam Ts    Element types of the tuple.
/// @param  tuple The tuple to access.
/// @return Mutable reference to the element at Index.
template <SizeT Index, typename... Ts>
GP_NODISCARD constexpr auto& get(TTuple<Ts...>& tuple) noexcept
{
    return GP::Get<Index>(tuple);
}

/// @brief ADL-reachable get<I> accessor for the structured-binding protocol; returns a const reference.
/// @tparam Index Zero-based element index.
/// @tparam Ts    Element types of the tuple.
/// @param  tuple The tuple to access.
/// @return Const reference to the element at Index.
template <SizeT Index, typename... Ts>
GP_NODISCARD constexpr const auto& get(const TTuple<Ts...>& tuple) noexcept
{
    return GP::Get<Index>(tuple);
}

/// @brief ADL-reachable get<I> accessor for the structured-binding protocol; returns an rvalue reference.
/// @tparam Index Zero-based element index.
/// @tparam Ts    Element types of the tuple.
/// @param  tuple Rvalue reference to the tuple.
/// @return Rvalue reference to the element at Index.
template <SizeT Index, typename... Ts>
GP_NODISCARD constexpr auto&& get(TTuple<Ts...>&& tuple) noexcept
{
    return GP::Get<Index>(GP::Move(tuple));
}

}   // namespace GP

namespace std
{

template <typename... Ts>
struct tuple_size<GP::TTuple<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)>
{};

template <size_t I, typename... Ts>
struct tuple_element<I, GP::TTuple<Ts...>>
{
    using type = GP::Detail::TTypeAtT<I, Ts...>;
};

}   // namespace std
