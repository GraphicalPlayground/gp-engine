// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/CoreConcepts.hpp"
#include <compare>

namespace GP
{

// clang-format off

/// @brief Satisfied when T supports == and !=.
template <typename T, typename U = T>
concept CEqualityComparable =
    requires (const T& t, const U& u)
    {
        { t == u } -> CConvertibleTo<bool>;
        { t != u } -> CConvertibleTo<bool>;
    };

/// @brief Satisfied when T supports all comparison operators (<, <=, >, >=, ==, !=).
template <typename T, typename U = T>
concept CTotallyOrdered =
    CEqualityComparable<T, U> &&
    requires (const T& t, const U& u)
    {
        { t <  u } -> CConvertibleTo<bool>;
        { t <= u } -> CConvertibleTo<bool>;
        { t >  u } -> CConvertibleTo<bool>;
        { t >= u } -> CConvertibleTo<bool>;
    };

/// @brief Satisfied when T supports the three-way comparison operator (<=>) returning Cat.
template <typename T, typename Cat = std::partial_ordering, typename U = T>
concept CThreeWayComparable =
    requires (const T& t, const U& u)
    {
        { t <=> u } -> CConvertibleTo<Cat>;
    };

// clang-format on

}   // namespace GP
