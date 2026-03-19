// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include <compare>

namespace GP
{

/// @brief A plain-value descriptor for a single display output mode (resolution + refresh + colour depth).
///
/// Instances are obtained via IDisplay::GetCurrentMode() or IDisplay::GetSupportedModes() and are used to enumerate
/// hardware-supported configurations, compare modes, and request mode changes.
///
/// Layout is intentionally packed to four UInt32 fields (16 bytes) for cache-friendly storage in flat arrays returned
/// by IDisplay::GetSupportedModes().
///
/// @note \c refreshRate carries the integer Hz value (e.g. 60, 144, 240). Sub-integer rates such
///       as 59.94 Hz are rounded by the platform layer; use the rational form if exact precision
///       is required in future extensions.
/// @note \c bitsPerPixel reflects the total colour depth of the scanout buffer, not the
///       per-channel precision. A standard 8-bpc RGBA surface reports 32.
/// @see IDisplay
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
