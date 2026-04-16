// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreDefines.hpp"
#include <cstddef>
#include <cstdint>

namespace gp
{

/// @section Integer types with fixed widths.
/// @see https://en.cppreference.com/w/cpp/types/integer

/// @brief 8-bit unsigned integer type.
///        limits: [0, 255]
using UInt8 = std::uint8_t;

/// @brief 16-bit unsigned integer type.
///        limits: [0, 65535]
using UInt16 = std::uint16_t;

/// @brief 32-bit unsigned integer type.
///        limits: [0, 4294967295]
using UInt32 = std::uint32_t;

/// @brief 64-bit unsigned integer type.
///        limits: [0, 18446744073709551615]
using UInt64 = std::uint64_t;

/// @brief 8-bit signed integer type.
///        limits: [-128, 127]
using Int8 = std::int8_t;

/// @brief 16-bit signed integer type.
///        limits: [-32768, 32767]
using Int16 = std::int16_t;

/// @brief 32-bit signed integer type.
///        limits: [-2147483648, 2147483647]
using Int32 = std::int32_t;

/// @brief 64-bit signed integer type.
///        limits: [-9223372036854775808, 9223372036854775807]
using Int64 = std::int64_t;

/// @section Floating-point types with fixed widths.

/// @brief 32-bit floating-point type (single precision).
///        Approximate limits: [1.18e-38, 3.4e+38]
using Float32 = float;

/// @brief 64-bit floating-point type (double precision).
///        Approximate limits: [2.22e-308, 1.8e+308]
using Float64 = double;

/// @section Size and pointer difference types.

using USize = std::size_t;
using ISize = std::ptrdiff_t;
using UIntPtr = std::uintptr_t;
using IntPtr = std::intptr_t;
using PtrDiff = std::ptrdiff_t;

/// @section Character types.

using Char8 = char8_t;
using Char16 = char16_t;
using Char32 = char32_t;
using Char = char;
using WChar = wchar_t;

/// @section Types related to alignment.

using MaxAlign = std::max_align_t;
using AlignVal = std::align_val_t;

/// @section Special values.

using Byte = std::byte;

/// @brief A special value that can be used to indicate a dynamic extent in template parameters.
inline constexpr USize DynamicExtent = static_cast<USize>(-1);

}   // namespace gp
