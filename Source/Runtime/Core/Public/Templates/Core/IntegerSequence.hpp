// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"

namespace GP
{

// clang-format off

/// @brief Represents a compile-time sequence of integer values.
/// @tparam T The integer type of the sequence elements.
/// @tparam ...Ns The sequence values.
template <typename T, T... Ns>
struct TIntegerSequence
{
public:
    using ValueType = T; //<! The element type.

public:
    /// @brief Returns the number of elements in the sequence.
    /// @return The size of the sequence.
    static constexpr SizeT Size() noexcept { return sizeof...(Ns); }
};

/// @brief Specialisation for index sequences (SizeT).
template <SizeT... Ns>
using TIndexSequence = TIntegerSequence<SizeT, Ns...>;

/// @brief Generates TIntegerSequence<T, 0, 1, ..., N-1> at compile time.
///        Uses __make_integer_seq on MSVC/Clang or __integer_pack on GCC.
#if GP_COMPILER_CLANG || GP_COMPILER_MSVC

template <typename T, T N>
using TMakeIntegerSequence = __make_integer_seq<TIntegerSequence, T, N>;

#elif GP_COMPILER_GCC

namespace Detail
{

/// @brief Helper struct to generate TIntegerSequence using __integer_pack on GCC.
template <typename T, T N>
struct TMakeIntegerSequenceImpl
{
    using Type = TIntegerSequence<T, __integer_pack(N)...>;
};

}   // namespace Detail

template <typename T, T N>
using TMakeIntegerSequence = typename Detail::TMakeIntegerSequenceImpl<T, N>::Type;

#endif

/// @brief Generates TIndexSequence<0, 1, ..., N-1>.
template <SizeT N>
using TMakeIndexSequence = TMakeIntegerSequence<SizeT, N>;

/// @brief Generates an index sequence spanning the parameter pack of Ts.
template <typename... Ts>
using TIndexSequenceFor = TMakeIndexSequence<sizeof...(Ts)>;

// clang-format on

}   // namespace GP
