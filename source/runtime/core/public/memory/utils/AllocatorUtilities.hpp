// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"
#include "platforms/base/Platform.hpp"
#include <limits>

namespace gp
{

/// @brief Opaque type used to represent container elements whose type is not known at compile time.
struct UntypedContainerElement
{};

namespace memory::detail
{

/// @brief Computes the new capacity when shrinking a container, minimizing excessive reallocation.
/// @details Shrinks the allocation only when slack exceeds 16 KB or more than one-third of the capacity is unused,
///          preventing pathological reallocation thrashing during alternating push/pop workloads.
/// @tparam InSizeType Container size type.
/// @param[in] newSize Requested new size in elements.
/// @param[in] currentSize Current capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, snaps the resulting capacity to the underlying allocator block size.
/// @param[in] alignment Memory alignment requirement in bytes.
/// @return The computed capacity in elements.
template <typename InSizeType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InSizeType defaultCalculateSlackShrink(
    const InSizeType newSize,
    const InSizeType currentSize,
    const gp::USize bytesPerElement,
    const bool allowQuantize,
    const gp::UInt32 alignment = gp::memory::kDefaultAlignment
) noexcept
{
    if (newSize <= static_cast<InSizeType>(0))
    {
        return static_cast<InSizeType>(0);
    }

    if (currentSize <= newSize || bytesPerElement == 0u) [[unlikely]]
    {
        return currentSize;
    }

    InSizeType result{ 0 };

    const gp::USize newElements = static_cast<gp::USize>(newSize);
    const gp::USize currentElements = static_cast<gp::USize>(currentSize);
    const gp::USize currentSlackElements = currentElements - newElements;
    const gp::USize currentSlackBytes = currentSlackElements * bytesPerElement;
    const bool hasTooManySlackBytes = currentSlackBytes >= 16'384u;
    const bool hasTooManySlackElements = (3u * newElements) < (2u * currentElements);

    if ((hasTooManySlackBytes || hasTooManySlackElements) && (currentSlackElements > 64u))
    {
        result = newSize;
        if (allowQuantize)
        {
            const gp::USize quantizedBytes =
                gp::memory::getActualAllocationSize(newElements * bytesPerElement, alignment);
            const gp::USize quantizedElements = quantizedBytes / bytesPerElement;
            const gp::USize maxElements = static_cast<gp::USize>(std::numeric_limits<InSizeType>::max());
            result = (quantizedElements > maxElements) ? std::numeric_limits<InSizeType>::max()
                                                       : static_cast<InSizeType>(quantizedElements);
        }
    }
    else
    {
        result = currentSize;
    }

    return result;
}

/// @brief Computes the optimal capacity for an explicit container reservation.
/// @tparam InSizeType Container size type.
/// @param[in] newSize Minimum required capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, snaps the resulting capacity to the underlying allocator block size.
/// @param[in] alignment Memory alignment requirement in bytes.
/// @return The computed capacity in elements.
template <typename InSizeType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InSizeType defaultCalculateSlackReserve(
    const InSizeType newSize,
    const gp::USize bytesPerElement,
    const bool allowQuantize,
    const gp::UInt32 alignment = kDefaultAlignment
) noexcept
{
    if (newSize <= static_cast<InSizeType>(0)) [[unlikely]]
    {
        return static_cast<InSizeType>(0);
    }

    if (allowQuantize && bytesPerElement > 0u)
    {
        const gp::USize requestedElements = static_cast<gp::USize>(newSize);
        const gp::USize quantizedBytes =
            gp::memory::getActualAllocationSize(requestedElements * bytesPerElement, alignment);
        const gp::USize quantizedElements = quantizedBytes / bytesPerElement;
        const gp::USize maxElements = static_cast<gp::USize>(std::numeric_limits<InSizeType>::max());

        if (quantizedElements > maxElements || quantizedElements < requestedElements)
        {
            return std::numeric_limits<InSizeType>::max();
        }

        return static_cast<InSizeType>(quantizedElements);
    }

    return newSize;
}

#ifndef GP_CONTAINER_INITIAL_ALLOC_ZERO_SLACK
    #define GP_CONTAINER_INITIAL_ALLOC_ZERO_SLACK GP_TRUE
#endif

#if defined(GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR) && !defined(GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR)
    #error If GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR is defined you must also define GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR
#endif
#if defined(GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR) && !defined(GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR)
    #error If GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR is defined you must also define GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR
#endif

#ifndef GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR
    #if GP_AGGRESSIVE_MEMORY_SAVING
        #define GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR 1
    #else
        #define GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR 3
    #endif
#endif

#ifndef GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR
    #if GP_AGGRESSIVE_MEMORY_SAVING
        #define GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR 4
    #else
        #define GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR 8
    #endif
#endif

static_assert(
    GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR > 0,
    "GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR must be greater than 0"
);
static_assert(
    GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR > GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR,
    "GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR must be greater than GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR"
);

/// @brief Computes the next capacity when a container exhausts its current allocation.
/// @details Uses a geometric growth factor (default 1.375x + constant slack) to guarantee amortized O(1) push_back
///          while avoiding the 2x memory waste of naive doubling.
/// @tparam InSizeType Container size type.
/// @param[in] newSize Minimum capacity required to fulfill the current operation.
/// @param[in] currentSize Current capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, snaps the resulting capacity to the underlying allocator block size.
/// @param[in] alignment Memory alignment requirement in bytes.
/// @return The computed capacity in elements.
template <typename InSizeType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InSizeType defaultCalculateSlackGrow(
    const InSizeType newSize,
    const InSizeType currentSize,
    const gp::USize bytesPerElement,
    const bool allowQuantize,
    const gp::UInt32 alignment = kDefaultAlignment
) noexcept
{
#if GP_AGGRESSIVE_MEMORY_SAVING
    const gp::USize firstGrow = 1u;
    const gp::USize constantGrow = 0u;
#else
    const gp::USize firstGrow = 4u;
    const gp::USize constantGrow = 16u;
#endif

    if (newSize <= static_cast<InSizeType>(0)) [[unlikely]]
    {
        return static_cast<InSizeType>(0);
    }

    const gp::USize requestedElements = static_cast<gp::USize>(newSize);
    gp::USize grow = firstGrow;

#if GP_CONTAINER_INITIAL_ALLOC_ZERO_SLACK
    if (currentSize)
    {
        const gp::USize proportionalSlack = (requestedElements / GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR) *
                                                GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR +
                                            ((requestedElements % GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR) *
                                             GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR) /
                                                GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR;
        grow = requestedElements + proportionalSlack + constantGrow;
    }
    else if (requestedElements > grow)
    {
        grow = requestedElements;
    }
#else
    if (currentSize || requestedElements > grow)
    {
        const gp::USize proportionalSlack = (requestedElements / GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR) *
                                                GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR +
                                            ((requestedElements % GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR) *
                                             GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR) /
                                                GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR;
        grow = requestedElements + proportionalSlack + constantGrow;
    }
#endif

    if (allowQuantize && bytesPerElement > 0u)
    {
        grow = gp::memory::getActualAllocationSize(grow * bytesPerElement, alignment) / bytesPerElement;
    }

    const gp::USize maxElements = static_cast<gp::USize>(std::numeric_limits<InSizeType>::max());
    if (grow > maxElements || grow < requestedElements)
    {
        return std::numeric_limits<InSizeType>::max();
    }

    return static_cast<InSizeType>(grow);
}

}   // namespace memory::detail

namespace memory
{

/// @brief Allocator-aware reservation slack calculator that quantizes capacity using the bound `allocator` instance.
/// @tparam InSizeType Container size type.
/// @tparam InAllocatorType Engine allocator satisfying `gp::concepts::IsAllocator`.
/// @param[in] allocator Bound container allocator instance.
/// @param[in] newSize Minimum required capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, queries `allocator.getActualAllocationSize(...)`.
/// @param[in] alignment Memory alignment requirement in bytes.
/// @return Optimal capacity in elements.
template <typename InSizeType, gp::concepts::IsAllocator InAllocatorType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InSizeType calculateSlackReserve(
    const InAllocatorType& allocator,
    const InSizeType newSize,
    const gp::USize bytesPerElement,
    const bool allowQuantize,
    const gp::UInt32 alignment = kDefaultAlignment
) noexcept
{
    if (newSize <= static_cast<InSizeType>(0)) [[unlikely]]
    {
        return static_cast<InSizeType>(0);
    }

    if (allowQuantize && bytesPerElement > 0u)
    {
        const gp::USize requestedElements = static_cast<gp::USize>(newSize);
        const gp::USize requestedBytes = requestedElements * bytesPerElement;
        const gp::USize actualBytes = allocator.getActualAllocationSize(requestedBytes, alignment);
        const gp::USize quantizedBytes = actualBytes >= requestedBytes ? actualBytes : requestedBytes;
        const gp::USize quantizedElements = quantizedBytes / bytesPerElement;
        const gp::USize maxElements = static_cast<gp::USize>(std::numeric_limits<InSizeType>::max());

        if (quantizedElements > maxElements || quantizedElements < requestedElements)
        {
            return std::numeric_limits<InSizeType>::max();
        }

        return static_cast<InSizeType>(quantizedElements);
    }

    return newSize;
}

/// @brief Allocator-aware geometric growth calculator that quantizes capacity using the bound `allocator` instance.
/// @tparam InSizeType Container size type.
/// @tparam InAllocatorType Engine allocator satisfying `gp::concepts::IsAllocator`.
/// @param[in] allocator Bound container allocator instance.
/// @param[in] newSize Minimum required capacity in elements.
/// @param[in] currentSize Current capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, queries `allocator.getActualAllocationSize(...)`.
/// @param[in] alignment Memory alignment requirement in bytes.
/// @return Grown capacity in elements.
template <typename InSizeType, gp::concepts::IsAllocator InAllocatorType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InSizeType calculateSlackGrow(
    const InAllocatorType& allocator,
    const InSizeType newSize,
    const InSizeType currentSize,
    const gp::USize bytesPerElement,
    const bool allowQuantize,
    const gp::UInt32 alignment = kDefaultAlignment
) noexcept
{
    const InSizeType unquantizedGrow =
        detail::defaultCalculateSlackGrow<InSizeType>(newSize, currentSize, bytesPerElement, false, alignment);
    if (!allowQuantize || bytesPerElement == 0u || unquantizedGrow <= static_cast<InSizeType>(0))
    {
        return unquantizedGrow;
    }

    return calculateSlackReserve<InSizeType>(allocator, unquantizedGrow, bytesPerElement, true, alignment);
}

/// @brief Allocator-aware shrink slack calculator that quantizes capacity using the bound `allocator` instance.
/// @tparam InSizeType Container size type.
/// @tparam InAllocatorType Engine allocator satisfying `gp::concepts::IsAllocator`.
/// @param[in] allocator Bound container allocator instance.
/// @param[in] newSize Requested new size in elements.
/// @param[in] currentSize Current capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, queries `allocator.getActualAllocationSize(...)`.
/// @param[in] alignment Memory alignment requirement in bytes.
/// @return Shrunk or retained capacity in elements.
template <typename InSizeType, gp::concepts::IsAllocator InAllocatorType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InSizeType calculateSlackShrink(
    const InAllocatorType& allocator,
    const InSizeType newSize,
    const InSizeType currentSize,
    const gp::USize bytesPerElement,
    const bool allowQuantize,
    const gp::UInt32 alignment = kDefaultAlignment
) noexcept
{
    const InSizeType unquantizedShrink =
        detail::defaultCalculateSlackShrink<InSizeType>(newSize, currentSize, bytesPerElement, false, alignment);
    if (unquantizedShrink == currentSize || unquantizedShrink <= static_cast<InSizeType>(0) || !allowQuantize ||
        bytesPerElement == 0u)
    {
        return unquantizedShrink;
    }

    return calculateSlackReserve<InSizeType>(allocator, unquantizedShrink, bytesPerElement, true, alignment);
}

}   // namespace memory

}   // namespace gp
