// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"

namespace gp::math::convert
{

/// @brief Converts bytes to gigabytes, rounding up to the nearest whole number.
/// @tparam T The unsigned integral type to which the result will be converted.
/// @param[in] bytes The number of bytes to convert.
/// @return The equivalent number of gigabytes, rounded up.
template <concepts::IsUnsignedIntegral T>
GP_NODISCARD constexpr T bytesToGigabytes(UInt64 bytes)
{
    constexpr UInt64 bytesPerGigabyte = 1024ull * 1024ull * 1024ull;
    return static_cast<T>((bytes + bytesPerGigabyte - 1ull) / bytesPerGigabyte);
}

}   // namespace gp::math::convert
