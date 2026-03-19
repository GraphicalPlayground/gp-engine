// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include <compare>

namespace GP
{

/// @brief Structure representing a display mode, including resolution and refresh rate.
struct FDisplayMode
{
public:
    UInt32 width{ 0u };           //<! The width of the display mode in pixels.
    UInt32 height{ 0u };          //<! The height of the display mode in pixels.
    UInt32 refreshRate{ 0u };     //<! The refresh rate of the display mode in Hz.
    UInt32 bitsPerPixel{ 32u };   //<! Total colour depth of the scanout buffer in bits (e.g. 24, 32).

public:
    /// @brief Equality comparison (all four fields must match exactly).
    GP_NODISCARD GP_FORCEINLINE constexpr bool operator==(const FDisplayMode&) const noexcept = default;

    /// @brief Three-way comparison providing a stable, lexicographic sort order.
    ///        Sort key (ascending priority):
    ///          1. Total pixel count (width * height)   - lower resolution first.
    ///          2. Refresh rate                         - lower Hz first.
    ///          3. Bits per pixel                       - lower depth first.
    /// @param other The mode to compare against.
    /// @return A std::strong_ordering value reflecting the relative rank of this mode.
    GP_NODISCARD GP_FORCEINLINE constexpr auto operator<=>(const FDisplayMode& other) const noexcept
        -> std::strong_ordering
    {
        if (const auto cmp = GetPixelCount() <=> other.GetPixelCount(); cmp != 0) { return cmp; }
        if (const auto cmp = refreshRate <=> other.refreshRate; cmp != 0) { return cmp; }
        return bitsPerPixel <=> other.bitsPerPixel;
    }

public:
    /// @brief Returns true when all fields carry plausible, non-zero values.
    GP_NODISCARD GP_FORCEINLINE constexpr bool IsValid() const noexcept
    {
        return (width > 0u) && (height > 0u) && (refreshRate > 0u) && (bitsPerPixel > 0u);
    }

    /// @brief Returns the total number of physical pixels in one frame (width * height).
    GP_NODISCARD GP_FORCEINLINE constexpr UInt64 GetPixelCount() const noexcept
    {
        return static_cast<UInt64>(width) * static_cast<UInt64>(height);
    }
};

}   // namespace GP
