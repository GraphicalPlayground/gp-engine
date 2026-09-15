// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"         // IWYU pragma: keep

namespace gp::memory
{

namespace detail
{

/// @brief A base class for sized allocators that provides common functionality and type definitions.
template <gp::Int32 Bits>
struct BitsToSizeType
{
    /// @details Compile-time failure for unsupported allocator index sizes.
    static_assert(Bits == Bits + 1, "Unsupported allocator index size.");
};

// clang-format off

/// @details We use signed integer types for size types to allow for negative values to be used safely.
template<> struct BitsToSizeType<8>  { using Type = gp::Int8; };
template<> struct BitsToSizeType<16> { using Type = gp::Int16; };
template<> struct BitsToSizeType<32> { using Type = gp::Int32; };
template<> struct BitsToSizeType<64> { using Type = gp::Int64; };

// clang-format on

}   // namespace detail

/// @brief A base class for sized allocators that provides common functionality and type definitions.
/// @tparam IndexSize The size of the index type used by the allocator (in bits).
/// @tparam SubClass The derived class that inherits from this base class.
template <gp::Int32 IndexSize, typename SubClass>
class SizedAllocatorBase
{
public:
    using SizeType = detail::BitsToSizeType<IndexSize>::Type;
    using USizeType = std::make_unsigned_t<SizeType>;

public:
    static constexpr bool kNeedsElementType = false;
    static constexpr bool kRequireRangeCheck = true;
};

}   // namespace gp::memory
