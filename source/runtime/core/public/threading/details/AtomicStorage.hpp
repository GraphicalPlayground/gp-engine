// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::thread::detail
{

/// @brief Maps a byte size to fundamental integer types used for atomic storage.
/// @tparam ByteSize Size of the target value in bytes (valid values are 1, 2, 4, and 8).
template <gp::USize ByteSize>
struct AtomicStorageSelector;

/// @brief 1-byte atomic storage specialization.
template <>
struct AtomicStorageSelector<1u>
{
    /// @brief Unsigned integer type holding the raw bit pattern.
    using UnsignedType = gp::UInt8;

    /// @brief Signed integer type used for signed arithmetic.
    using SignedType = gp::Int8;

#if GP_COMPILER_MSVC
    /// @brief MSVC intrinsic parameter type for 8-bit interlocked operations.
    using MsvcInterlockedType = char;
#endif
};

/// @brief 2-byte atomic storage specialization.
template <>
struct AtomicStorageSelector<2u>
{
    /// @brief Unsigned integer type holding the raw bit pattern.
    using UnsignedType = gp::UInt16;

    /// @brief Signed integer type used for signed arithmetic.
    using SignedType = gp::Int16;

#if GP_COMPILER_MSVC
    /// @brief MSVC intrinsic parameter type for 16-bit interlocked operations.
    using MsvcInterlockedType = short;
#endif
};

/// @brief 4-byte atomic storage specialization.
template <>
struct AtomicStorageSelector<4u>
{
    /// @brief Unsigned integer type holding the raw bit pattern.
    using UnsignedType = gp::UInt32;

    /// @brief Signed integer type used for signed arithmetic.
    using SignedType = gp::Int32;

#if GP_COMPILER_MSVC
    /// @brief MSVC intrinsic parameter type for 32-bit interlocked operations.
    using MsvcInterlockedType = long;
#endif
};

/// @brief 8-byte atomic storage specialization.
template <>
struct AtomicStorageSelector<8u>
{
    /// @brief Unsigned integer type holding the raw bit pattern.
    using UnsignedType = gp::UInt64;

    /// @brief Signed integer type used for signed arithmetic.
    using SignedType = gp::Int64;

#if GP_COMPILER_MSVC
    /// @brief MSVC intrinsic parameter type for 64-bit interlocked operations.
    using MsvcInterlockedType = __int64;
#endif
};

}   // namespace gp::thread::detail

