// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

/// @brief Compile-time constant value wrapper.
/// @tparam T Type of the constant value.
/// @tparam Value The constant value itself.
template <typename T, T Val>
struct TIntegralConstant
{
public:
    static constexpr T Value = Val;   //<! The constant value.
    using ValueType = T;              //<! The type of the constant value.
    using Type = TIntegralConstant;   //<! The type of this integral constant.

public:
    /// @brief Implicit conversion operator to the constant value type.
    constexpr operator ValueType() const noexcept { return Value; }

    /// @brief Function call operator to retrieve the constant value.
    /// @return The constant value.
    constexpr ValueType operator()() const noexcept { return Value; }
};

/// @brief Convenience alias for boolean integral constants.
/// @tparam B The boolean constant value.
template <bool B>
using TBoolConstant = TIntegralConstant<bool, B>;

/// @brief Compile-time boolean constant representing `true`.
using FTrueType = TBoolConstant<true>;

/// @brief Compile-time boolean constant representing `false`.
using FFalseType = TBoolConstant<false>;

}   // namespace GP
