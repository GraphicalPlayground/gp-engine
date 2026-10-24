// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "display/DisplayEnums.hpp"
#include "window/WindowTypes.hpp"

namespace gp::hal
{

/// @brief Unique 32-bit identifier for a hardware display monitor.
using DisplayId = gp::UInt32;

/// @brief Sentinel constant representing an uninitialized or invalid display identifier.
inline constexpr DisplayId kInvalidDisplayId = 0u;

/// @brief Represents a 2D desktop rectangle (virtual origin + pixel dimensions).
struct DisplayRect
{
    Point2D origin{ 0, 0 };
    Extent2D extent{ 0u, 0u };

    /// @brief Default constructor initializes an empty rectangle at (0, 0).
    [[nodiscard]] constexpr DisplayRect() noexcept = default;

    /// @brief Constructs a display rectangle from an origin point and an extent.
    /// @param[in] inOrigin Top-left virtual desktop coordinate.
    /// @param[in] inExtent Width and height in pixels.
    [[nodiscard]] constexpr DisplayRect(const Point2D& inOrigin, const Extent2D& inExtent) noexcept
        : origin(inOrigin)
        , extent(inExtent)
    {}

    /// @brief Constructs a display rectangle from raw coordinates and dimensions.
    /// @param[in] x Top-left X coordinate.
    /// @param[in] y Top-left Y coordinate.
    /// @param[in] width Rectangle width in pixels.
    /// @param[in] height Rectangle height in pixels.
    [[nodiscard]] constexpr DisplayRect(Int32 x, Int32 y, UInt32 width, UInt32 height) noexcept
        : origin(x, y)
        , extent(width, height)
    {}

    /// @brief Checks whether two DisplayRect instances are identical.
    /// @param[in] other Rectangle to compare against.
    /// @return True if both origin and extent match, false otherwise.
    [[nodiscard]] constexpr bool operator==(const DisplayRect& other) const noexcept
    {
        return origin == other.origin && extent == other.extent;
    }

    /// @brief Checks whether two DisplayRect instances differ.
    /// @param[in] other Rectangle to compare against.
    /// @return True if origin or extent differ, false otherwise.
    [[nodiscard]] constexpr bool operator!=(const DisplayRect& other) const noexcept
    {
        return !(*this == other);
    }

    /// @brief Checks whether a point in virtual desktop coordinates lies inside this rectangle.
    /// @param[in] point Screen-space point to test.
    /// @return True if the point is within `[origin, origin + extent)`, false otherwise.
    [[nodiscard]] constexpr bool contains(const Point2D& point) const noexcept
    {
        return point.x >= origin.x && point.y >= origin.y && point.x < (origin.x + static_cast<Int32>(extent.width)) &&
               point.y < (origin.y + static_cast<Int32>(extent.height));
    }
};

/// @brief Describes a single hardware display mode (resolution, refresh rate, and pixel format).
struct DisplayMode
{
public:
    Extent2D resolution{ 0u, 0u };
    Float32 refreshRate{ 0.0f };
    Float32 pixelDensity{ 1.0f };
    DisplayPixelFormat pixelFormat{ DisplayPixelFormat::Unknown };
    gp::UInt8 reserved[3]{ 0u, 0u, 0u };

public:
    /// @brief Default constructor initializes an empty display mode.
    [[nodiscard]] constexpr DisplayMode() noexcept = default;

    /// @brief Constructs a display mode with explicit parameters.
    /// @param[in] inResolution Pixel width and height.
    /// @param[in] inRefreshRate Refresh rate in Hz.
    /// @param[in] inPixelDensity Ratio of physical pixels to logical coordinates.
    /// @param[in] inFormat Hardware scanout pixel format.
    [[nodiscard]] constexpr DisplayMode(
        const Extent2D& inResolution,
        Float32 inRefreshRate,
        Float32 inPixelDensity = 1.0f,
        DisplayPixelFormat inFormat = DisplayPixelFormat::RGBA8888
    ) noexcept
        : resolution(inResolution)
        , refreshRate(inRefreshRate)
        , pixelDensity(inPixelDensity)
        , pixelFormat(inFormat)
    {}

    /// @brief Checks equality between two display modes.
    /// @param[in] other Display mode to compare against.
    /// @return True if resolution, refresh rate, density, and format match.
    [[nodiscard]] constexpr bool operator==(const DisplayMode& other) const noexcept
    {
        return resolution == other.resolution && refreshRate == other.refreshRate &&
               pixelDensity == other.pixelDensity && pixelFormat == other.pixelFormat;
    }

    /// @brief Checks inequality between two display modes.
    /// @param[in] other Display mode to compare against.
    /// @return True if any field differs.
    [[nodiscard]] constexpr bool operator!=(const DisplayMode& other) const noexcept
    {
        return !(*this == other);
    }
};

/// @brief High Dynamic Range (HDR) luminance calibration parameters reported by the OS.
struct DisplayHDRMetadata
{
    /// @brief Reference white luminance level for Standard Dynamic Range (SDR) content in nits (default 80.0).
    Float32 sdrWhiteLevelNits{ 80.0f };

    /// @brief Peak luminance ratio above SDR white supported by the display (1.0f = SDR only, >1.0f = HDR headroom).
    Float32 hdrHeadroom{ 1.0f };

    /// @brief Default constructor initializes standard SDR luminance metadata.
    [[nodiscard]] constexpr DisplayHDRMetadata() noexcept = default;

    /// @brief Constructs HDR metadata with explicit SDR white level and HDR headroom.
    /// @param[in] inSdrWhiteNits SDR reference white luminance in nits.
    /// @param[in] inHdrHeadroom HDR peak luminance multiplier above SDR white.
    [[nodiscard]] constexpr DisplayHDRMetadata(Float32 inSdrWhiteNits, Float32 inHdrHeadroom) noexcept
        : sdrWhiteLevelNits(inSdrWhiteNits)
        , hdrHeadroom(inHdrHeadroom)
    {}
};

/// @brief Snapshot descriptor used to initialize or reconfigure a Display instance.
struct DisplayDesc
{
    DisplayId id{ kInvalidDisplayId };
    StringView name{ "Generic Display" };
    DisplayRect bounds{};
    DisplayRect workArea{};
    DisplayMode currentMode{};
    DisplayMode desktopMode{};
    DisplayHDRMetadata hdrMetadata{};
    Float32 contentScale{ 1.0f };
    DisplayOrientation orientation{ DisplayOrientation::Landscape };
    DisplayFlags flags{ DisplayFlags::None };

    /// @brief Default constructor.
    [[nodiscard]] constexpr DisplayDesc() noexcept = default;
};

}   // namespace gp::hal
