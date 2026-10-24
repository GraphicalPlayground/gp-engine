// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "display/SDL3Display.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

namespace detail
{

/// @brief Translates an `SDL_PixelFormat` into `gp::hal::DisplayPixelFormat`.
/// @param[in] format SDL3 pixel format enumerator.
/// @return Engine `DisplayPixelFormat` value.
[[nodiscard]] constexpr DisplayPixelFormat translatePixelFormat(SDL_PixelFormat format) noexcept
{
    switch (format)
    {
    case SDL_PIXELFORMAT_RGB24:
        return DisplayPixelFormat::RGB888;
    case SDL_PIXELFORMAT_RGBX8888:
    case SDL_PIXELFORMAT_XRGB8888:
        return DisplayPixelFormat::RGBX8888;
    case SDL_PIXELFORMAT_BGRX8888:
    case SDL_PIXELFORMAT_XBGR8888:
        return DisplayPixelFormat::BGRX8888;
    case SDL_PIXELFORMAT_RGBA8888:
    case SDL_PIXELFORMAT_ABGR8888:
        return DisplayPixelFormat::RGBA8888;
    case SDL_PIXELFORMAT_BGRA8888:
    case SDL_PIXELFORMAT_ARGB8888:
        return DisplayPixelFormat::BGRA8888;
    case SDL_PIXELFORMAT_RGB565:
        return DisplayPixelFormat::RGB565;
    case SDL_PIXELFORMAT_XRGB2101010:
        return DisplayPixelFormat::RGB101010;
    case SDL_PIXELFORMAT_XBGR2101010:
        return DisplayPixelFormat::BGR101010;
    case SDL_PIXELFORMAT_RGBA64_FLOAT:
        return DisplayPixelFormat::RGBA16Float;
    default:
        return DisplayPixelFormat::Unknown;
    }
}

/// @brief Translates an `SDL_DisplayOrientation` into `gp::hal::DisplayOrientation`.
/// @param[in] orientation SDL3 orientation enumerator.
/// @return Engine `DisplayOrientation` value.
[[nodiscard]] constexpr DisplayOrientation translateOrientation(SDL_DisplayOrientation orientation) noexcept
{
    switch (orientation)
    {
    case SDL_ORIENTATION_LANDSCAPE:
        return DisplayOrientation::Landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return DisplayOrientation::LandscapeFlipped;
    case SDL_ORIENTATION_PORTRAIT:
        return DisplayOrientation::Portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return DisplayOrientation::PortraitFlipped;
    case SDL_ORIENTATION_UNKNOWN:
    default:
        return DisplayOrientation::Unknown;
    }
}

/// @brief Converts a raw `SDL_DisplayMode` pointer into an engine `DisplayMode` POD.
/// @param[in] sdlMode Pointer to an SDL3 display mode structure.
/// @return Populated `DisplayMode` struct.
[[nodiscard]] constexpr DisplayMode translateDisplayMode(const SDL_DisplayMode* sdlMode) noexcept
{
    if (sdlMode == nullptr)
    {
        return DisplayMode{};
    }

    return DisplayMode(
        Extent2D(
            static_cast<UInt32>(sdlMode->w > 0 ? sdlMode->w : 0), static_cast<UInt32>(sdlMode->h > 0 ? sdlMode->h : 0)
        ),
        (sdlMode->refresh_rate > 0.0f) ? sdlMode->refresh_rate : 60.0f,
        (sdlMode->pixel_density > 0.0f) ? sdlMode->pixel_density : 1.0f,
        translatePixelFormat(sdlMode->format)
    );
}

/// @brief Computes the length of a null-terminated UTF-8 string up to `maxLen` without `<cstring>`.
/// @param[in] str Input C-string pointer.
/// @param[in] maxLen Maximum number of bytes to scan.
/// @return String length in bytes.
[[nodiscard]] constexpr gp::USize boundedStrLen(const char* str, gp::USize maxLen) noexcept
{
    if (str == nullptr)
    {
        return 0u;
    }

    gp::USize len = 0u;
    while (len < maxLen && str[len] != '\0')
    {
        ++len;
    }
    return len;
}

}   // namespace detail

Display::Display(DisplayId sdlDisplayId, bool isPrimaryDisplay) noexcept
    : gp::hal::generic::Display()
{
    initialize(sdlDisplayId, isPrimaryDisplay);
}

void Display::initialize(DisplayId sdlDisplayId, bool isPrimaryDisplay) noexcept
{
    if (sdlDisplayId == kInvalidDisplayId) [[unlikely]]
    {
        reset();
        return;
    }

    m_desc = DisplayDesc{};
    m_desc.id = sdlDisplayId;
    m_desc.flags = DisplayFlags::Connected;

    if (isPrimaryDisplay)
    {
        m_desc.flags |= DisplayFlags::Primary;
    }

    refresh();
}

void Display::refresh() noexcept
{
    if (m_desc.id == kInvalidDisplayId) [[unlikely]]
    {
        return;
    }

    const SDL_DisplayID sdlId = static_cast<SDL_DisplayID>(m_desc.id);

    // Query human-readable monitor name
    if (const char* rawName = SDL_GetDisplayName(sdlId))
    {
        cacheNameString(StringView(rawName, detail::boundedStrLen(rawName, kMaxDisplayNameBytes - 1u)));
    }
    else
    {
        cacheNameString(StringView("SDL3 Display"));
    }

    // 2. Query full virtual desktop bounds
    SDL_Rect sdlBounds{};
    if (SDL_GetDisplayBounds(sdlId, &sdlBounds))
    {
        m_desc.bounds = DisplayRect(
            Point2D(sdlBounds.x, sdlBounds.y),
            Extent2D(
                static_cast<UInt32>(sdlBounds.w > 0 ? sdlBounds.w : 0),
                static_cast<UInt32>(sdlBounds.h > 0 ? sdlBounds.h : 0)
            )
        );
    }

    // Query usable desktop work area (excludes OS taskbar/dock/menu bar)
    SDL_Rect sdlUsable{};
    if (SDL_GetDisplayUsableBounds(sdlId, &sdlUsable))
    {
        m_desc.workArea = DisplayRect(
            Point2D(sdlUsable.x, sdlUsable.y),
            Extent2D(
                static_cast<UInt32>(sdlUsable.w > 0 ? sdlUsable.w : 0),
                static_cast<UInt32>(sdlUsable.h > 0 ? sdlUsable.h : 0)
            )
        );
    }
    else
    {
        m_desc.workArea = m_desc.bounds;
    }

    // Query panel orientation and desktop DPI content scale
    m_desc.orientation = detail::translateOrientation(SDL_GetCurrentDisplayOrientation(sdlId));
    const float scale = SDL_GetDisplayContentScale(sdlId);
    m_desc.contentScale = (scale > 0.0f) ? scale : 1.0f;

    // Query active scanout mode and native desktop mode
    m_desc.currentMode = detail::translateDisplayMode(SDL_GetCurrentDisplayMode(sdlId));
    m_desc.desktopMode = detail::translateDisplayMode(SDL_GetDesktopDisplayMode(sdlId));

    // Fallback if currentMode resolution wasn't reported (e.g., certain Wayland/headless drivers)
    if (m_desc.currentMode.resolution.width == 0u && m_desc.bounds.extent.width > 0u)
    {
        m_desc.currentMode.resolution = m_desc.bounds.extent;
        m_desc.currentMode.refreshRate = 60.0f;
        m_desc.currentMode.pixelDensity = m_desc.contentScale;
        m_desc.currentMode.pixelFormat = DisplayPixelFormat::RGBA8888;
    }

    if (m_desc.desktopMode.resolution.width == 0u)
    {
        m_desc.desktopMode = m_desc.currentMode;
    }

    // Query HDR state and luminance metadata via SDL3 display properties
    const SDL_PropertiesID props = SDL_GetDisplayProperties(sdlId);
    if (props != 0u)
    {
        const bool hdrEnabled = SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);

        // Note: SDL3 does not expose display-level SDR white point or HDR headroom properties.
        // HDR headroom and SDR white level are window-level properties in SDL3
        // (SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT and SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT).
        // For display-level metrics, keep baseline metadata or update when a window binds to this display.
        const float hdrHeadroom = 1.0f;
        const float sdrWhiteNits = 80.0f;

        if (hdrEnabled)
        {
            m_desc.flags |= DisplayFlags::HDREnabled | DisplayFlags::HDRCapable;
        }
        else
        {
            m_desc.flags &= ~DisplayFlags::HDREnabled;
            if (hdrHeadroom > 1.0f)
            {
                m_desc.flags |= DisplayFlags::HDRCapable;
            }
        }

        m_desc.hdrMetadata = DisplayHDRMetadata(sdrWhiteNits, hdrHeadroom);
    }

    // Populate deduplicated fullscreen display modes into the inline shadow table
    m_supportedModeCount = 0u;
    int modeCount = 0;
    if (SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(sdlId, &modeCount))
    {
        for (int i = 0; i < modeCount; ++i)
        {
            addSupportedModeUnique(detail::translateDisplayMode(modes[i]));
        }
        SDL_free(modes);
    }

    // Ensure desktop and current modes are always represented in the mode table
    addSupportedModeUnique(m_desc.desktopMode);
    addSupportedModeUnique(m_desc.currentMode);
}

}   // namespace gp::hal::sdl3
