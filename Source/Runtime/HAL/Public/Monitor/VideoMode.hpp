// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "HALBuild.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"

namespace GP::HAL
{

/// \brief Describes a single discrete video mode (resolution + refresh rate + color depth) supported by a monitor.
/// Instances are returned by IMonitor::GetSupportedVideoModes() and IMonitor::GetCurrentVideoMode(). All fields
/// reflect physical (hardware) pixel counts, not logical/DPI-scaled values.
struct FVideoMode
{
public:
    UInt32 width{ 0 };           //<! Horizontal resolution in physical pixels.
    UInt32 height{ 0 };          //<! Vertical resolution in physical pixels.
    UInt32 refreshRateHz{ 0 };   //<! Refresh rate in Hz (integer; fractional rates such as 59.94 are rounded).
    UInt8 redBits{ 8 };          //<! Bits per channel — red.
    UInt8 greenBits{ 8 };        //<! Bits per channel — green.
    UInt8 blueBits{ 8 };         //<! Bits per channel — blue.

public:
    /// \brief Equality comparison — two video modes are identical when all fields match.
    /// \param other The video mode to compare against.
    /// \return True if the video modes are identical in resolution, refresh rate, and color depth; false otherwise.
    GP_NODISCARD GP_INLINE bool operator==(const FVideoMode& other) const noexcept
    {
        return (width == other.width) && (height == other.height) && (refreshRateHz == other.refreshRateHz) &&
               (redBits == other.redBits) && (greenBits == other.greenBits) && (blueBits == other.blueBits);
    }

    /// \brief Inequality comparison.
    /// \param other The video mode to compare against.
    /// \return True if any field differs between the two video modes; false if all fields are identical.
    GP_NODISCARD GP_INLINE bool operator!=(const FVideoMode& other) const noexcept { return !(*this == other); }

public:
    /// \brief Returns the total bit depth of the video mode (red + green + blue channels).
    /// \return Total bits per pixel across all color channels.
    GP_NODISCARD GP_INLINE UInt8 GetBitDepth() const noexcept
    {
        return static_cast<UInt8>(redBits + greenBits + blueBits);
    }

    /// \brief Returns the aspect ratio of the video mode.
    /// \return Width divided by height. Returns 0.0f if height is zero.
    GP_NODISCARD GP_INLINE Float32 GetAspectRatio() const noexcept
    {
        return (height > 0u) ? (static_cast<Float32>(width) / static_cast<Float32>(height)) : 0.0f;
    }

    /// \brief Returns the resolution as an FIntExtent2D.
    /// \return The resolution of this video mode.
    GP_NODISCARD GP_INLINE FIntExtent2D ToExtent() const noexcept { return FIntExtent2D{ width, height }; }
};

}   // namespace GP::HAL
