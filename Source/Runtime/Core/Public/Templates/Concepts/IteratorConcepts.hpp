// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/ComparisonConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

// clang-format off

/// @brief Satisfied when It is a readable iterator (can be dereferenced).
template <typename TIt>
concept CIndirectlyReadable =
    requires (const TIt it)
    {
        typename TIt::ValueType;
        { *it } -> CSameAs<typename TIt::ValueType&>;
    };

/// @brief Satisfied when TIt supports prefix and postfix increment.
template <typename TIt>
concept CWeaklyIncrementable =
    CMovable<TIt> &&
    requires (TIt it)
    {
        { ++it } -> CSameAs<TIt&>;
        it++;
    };

/// @brief Satisfied when TIt is an input iterator.
template <typename TIt>
concept CInputIterator =
    CIndirectlyReadable<TIt> &&
    CWeaklyIncrementable<TIt>;

/// @brief Satisfied when TIt is an output iterator that can be written through.
template <typename TIt, typename T>
concept COutputIterator =
    CWeaklyIncrementable<TIt> &&
    requires (TIt it, T&& val) { *it++ = GP::Forward<T>(val); };

/// @brief Satisfied when TIt is a forward iterator (multi-pass, equality comparable).
template <typename TIt>
concept CForwardIterator =
    CInputIterator<TIt>         &&
    CCopyConstructible<TIt>     &&
    CEqualityComparable<TIt>;

/// @brief Satisfied when TIt is a bidirectional iterator (can be decremented).
template <typename TIt>
concept CBidirectionalIterator =
    CForwardIterator<TIt> &&
    requires (TIt it)
    {
        { --it  } -> CSameAs<TIt&>;
        { it--  } -> CSameAs<TIt>;
    };

/// @brief Satisfied when TIt is a random-access iterator.
template <typename TIt>
concept CRandomAccessIterator =
    CBidirectionalIterator<TIt> &&
    CTotallyOrdered<TIt>        &&
    requires (TIt it, typename TIt::DifferenceType n)
    {
        { it += n  } -> CSameAs<TIt&>;
        { it -= n  } -> CSameAs<TIt&>;
        { it +  n  } -> CSameAs<TIt>;
        { it -  n  } -> CSameAs<TIt>;
        { it[n]    } -> CSameAs<typename TIt::ValueType&>;
    };

/// @brief Satisfied when TIt is a contiguous iterator (pointer-equivalent addressing).
template <typename TIt>
concept CContiguousIterator =
    CRandomAccessIterator<TIt> &&
    requires (TIt it) { { it.operator->() } -> CPointer; };

/// @brief Satisfied when T provides begin() and end() returning compatible iterators.
template <typename T>
concept CRange =
    requires (T& range)
    {
        { range.begin() };
        { range.end()   };
    };

/// @brief Satisfied when T is a range whose iterators are contiguous.
template <typename T>
concept CContiguousRange =
    CRange<T> &&
    requires (T& range)
    {
        { range.data() } -> CPointer;
    };

/// @brief Satisfied when T is a contiguous range with a known element count.
template <typename T>
concept CSizedRange =
    CRange<T> &&
    requires (T& range)
    {
        { range.size() } -> CIntegral;
    };

// clang-format on

}   // namespace GP
