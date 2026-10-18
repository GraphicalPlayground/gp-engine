// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
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
/// @tparam SizeType Container size type.
/// @param[in] newSize Requested new size in elements.
/// @param[in] currentSize Current capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, snaps the resulting capacity to the underlying allocator block size.
/// @param[in] alignment Memory alignment requirement.
/// @return The computed capacity in elements.
template <typename SizeType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType defaultCalculateSlackShrink(
    SizeType newSize,
    SizeType currentSize,
    gp::USize bytesPerElement,
    bool allowQuantize,
    gp::UInt32 alignment = gp::memory::kDefaultAlignment
)
{
    SizeType result{ 0 };
    // TODO: Assert that newSize < currentSize

    const SizeType currentSlackElements = currentSize - newSize;
    const gp::USize currentSlackBytes = currentSlackElements * bytesPerElement;
    const bool hasTooManySlackBytes = currentSlackBytes >= 16'384;
    const bool hasTooManySlackElements = 3 * newSize < 2 * currentSize;

    if ((hasTooManySlackBytes || hasTooManySlackElements) && (currentSlackElements > 64 || !newSize))
    {
        result = newSize;
        if (result > 0)
        {
            if (allowQuantize)
            {
                result = static_cast<SizeType>(
                    gp::memory::getActualAllocationSize(result * bytesPerElement, alignment) / bytesPerElement
                );
            }
        }
    }
    else
    {
        result = currentSize;
    }

    return result;
}

/// @brief Computes the optimal capacity for a strict container reservation.
/// @tparam SizeType Container size type.
/// @param[in] newSize Minimum required capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, snaps the resulting capacity to the underlying allocator block size.
/// @param[in] alignment Memory alignment requirement.
/// @return The computed capacity in elements.
template <typename SizeType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType defaultCalculateSlackReserve(
    SizeType newSize, gp::USize bytesPerElement, bool allowQuantize, gp::UInt32 alignment = kDefaultAlignment
) noexcept
{
    SizeType result = newSize;

    if (allowQuantize)
    {
        result = static_cast<SizeType>(
            gp::memory::getActualAllocationSize(result * bytesPerElement, alignment) / bytesPerElement
        );
        if (newSize > result)
        {
            result = std::numeric_limits<SizeType>::max();
        }
    }

    return result;
}

#ifndef GP_CONTAINER_INITIAL_ALLOC_ZERO_SLACK
    #define GP_CONTAINER_INITIAL_ALLOC_ZERO_SLACK GP_TRUE
#endif

#if defined(GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR) && !defined(GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR)
    #error If GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR is defined you must also define GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR
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
/// @tparam SizeType Container size type.
/// @param[in] newSize Minimum capacity required to fulfill the current operation.
/// @param[in] currentSize Current capacity in elements.
/// @param[in] bytesPerElement Size of a single element in bytes.
/// @param[in] allowQuantize If true, snaps the resulting capacity to the underlying allocator block size.
/// @param[in] alignment Memory alignment requirement.
/// @return The computed capacity in elements.
template <typename SizeType>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType defaultCalculateSlackGrow(
    SizeType newSize,
    SizeType currentSize,
    gp::USize bytesPerElement,
    bool allowQuantize,
    gp::UInt32 alignment = kDefaultAlignment
) noexcept
{
#if GP_AGGRESSIVE_MEMORY_SAVING
    const gp::USize firstGrow = 1;
    const gp::USize constantGrow = 0;
#else
    const gp::USize firstGrow = 4;
    const gp::USize constantGrow = 16;
#endif

    SizeType result;
    // TODO: Assert that newSize > currentSize and newSize > 0

    gp::USize grow = firstGrow;

#if GP_CONTAINER_INITIAL_ALLOC_ZERO_SLACK
    if (currentSize)
    {
        grow = static_cast<gp::USize>(newSize) +
               GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR * static_cast<gp::USize>(newSize) /
                   GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR +
               constantGrow;
    }
    else if (static_cast<gp::USize>(newSize) > grow)
    {
        grow = static_cast<gp::USize>(newSize);
    }
#else
    if (currentSize || static_cast<gp::USize>(newSize) > grow)
    {
        grow = static_cast<gp::USize>(newSize) +
               GP_CONTAINER_SLACK_GROWTH_FACTOR_NUMERATOR * static_cast<gp::USize>(newSize) /
                   GP_CONTAINER_SLACK_GROWTH_FACTOR_DENOMINATOR +
               constantGrow;
    }
#endif

    if (allowQuantize)
    {
        result = static_cast<SizeType>(
            gp::memory::getActualAllocationSize(grow * bytesPerElement, alignment) / bytesPerElement
        );
    }
    else
    {
        result = static_cast<SizeType>(grow);
    }

    if (newSize > result)
    {
        result = std::numeric_limits<SizeType>::max();
    }

    return result;
}

}   // namespace memory::detail

}   // namespace gp
