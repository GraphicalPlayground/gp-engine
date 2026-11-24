// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::allocator
{

/// @brief Atomic counter used to track live descriptor allocation counts across descriptor allocators.
using DescriptorStatCounter = gp::thread::Atomic<gp::Int64>;

/// @brief Inclusive descriptor index range `[first, last]` within a descriptor heap.
struct DescriptorAllocatorRange
{
    /// @brief First descriptor index in the inclusive range.
    gp::UInt32 first{ 0u };

    /// @brief Last descriptor index in the inclusive range.
    gp::UInt32 last{ 0u };

    /// @brief Constructs a zero-initialized range `[0, 0]`.
    constexpr DescriptorAllocatorRange() noexcept = default;

    /// @brief Constructs an inclusive range spanning `[inFirst, inLast]`.
    /// @param[in] inFirst First index in the range.
    /// @param[in] inLast Last index in the range.
    constexpr DescriptorAllocatorRange(const gp::UInt32 inFirst, const gp::UInt32 inLast) noexcept
        : first(inFirst)
        , last(inLast)
    {}

    /// @brief Compares two descriptor ranges for equality.
    /// @param[in] rhs Range to compare against.
    /// @return True when both endpoints match.
    [[nodiscard]] constexpr bool operator==(const DescriptorAllocatorRange& rhs) const noexcept = default;
};

/// @brief Contiguous descriptor allocation defined by its starting heap index and slot count.
struct DescriptorAllocation
{
    /// @brief Zero-based starting index of the allocated descriptor span.
    gp::UInt32 startIndex{ 0u };

    /// @brief Number of contiguous descriptors in the allocation.
    gp::UInt32 count{ 0u };

    /// @brief Constructs an empty allocation (`startIndex == 0`, `count == 0`).
    constexpr DescriptorAllocation() noexcept = default;

    /// @brief Constructs a descriptor allocation starting at `inStartIndex` covering `inCount` slots.
    /// @param[in] inStartIndex Starting index in the descriptor heap.
    /// @param[in] inCount Number of contiguous descriptor slots allocated.
    constexpr DescriptorAllocation(const gp::UInt32 inStartIndex, const gp::UInt32 inCount) noexcept
        : startIndex(inStartIndex)
        , count(inCount)
    {}

    /// @brief Compares two descriptor allocations for equality.
    /// @param[in] rhs Allocation to compare against.
    /// @return True when both start index and count match.
    [[nodiscard]] constexpr bool operator==(const DescriptorAllocation& rhs) const noexcept = default;
};

}   // namespace gp::rhi::allocator
