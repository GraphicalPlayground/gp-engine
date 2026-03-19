// Copyright (c) - Graphical Playground. All rights reserved.

#include "Display/SDL3/SDL3Display.hpp"
#include <algorithm>
#include <SDL3/SDL_properties.h>

namespace GP
{

///
/// @section Helper
///

/// @brief Converts an SDL3 display mode descriptor into a GP FDisplayMode.
///
/// refresh_rate in SDL3 is a float (e.g. 59.94 Hz). It is rounded to the nearest integer because
/// FDisplayMode::refreshRate carries an integer Hz approximation, consistent with the values reported by DXGI and the
/// macOS AVDisplayManager APIs.
///
/// bits_per_pixel is read from the SDL pixel format details to produce the correct colour depth for packed formats
/// (e.g. 32 bpp for RGBA8, 16 bpp for B5G6R5).
///
/// @param sdlMode A valid SDL_DisplayMode obtained from SDL3.
/// @return A populated FDisplayMode. IsValid() is guaranteed to return true when sdlMode contains sensible non-zero
///         fields.
GP_NODISCARD static FDisplayMode ConvertSDLMode(const SDL_DisplayMode& sdlMode) noexcept
{
    // Resolve bits per pixel from the pixel format. SDL_GetPixelFormatDetails returns
    // nullptr for SDL_PIXELFORMAT_UNKNOWN; fall back to 32 bpp in that case.
    const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(sdlMode.format);
    const UInt32 bpp = details ? static_cast<UInt32>(details->bits_per_pixel) : 32u;

    // Round the floating-point refresh rate to the nearest integer Hz.
    const UInt32 refreshRate = static_cast<UInt32>(sdlMode.refresh_rate + 0.5f);

    return FDisplayMode{
        .width = static_cast<UInt32>(sdlMode.w),
        .height = static_cast<UInt32>(sdlMode.h),
        .refreshRate = refreshRate,
        .bitsPerPixel = bpp,
    };
}

/// @brief Maps an SDL3 display orientation enum to the GP equivalent.
/// @param sdlOrientation A value from SDL_DisplayOrientation.
/// @return The corresponding EDisplayOrientation value.
GP_NODISCARD static EDisplayOrientation ConvertSDLOrientation(SDL_DisplayOrientation sdlOrientation) noexcept
{
    switch (sdlOrientation)
    {
    case SDL_ORIENTATION_LANDSCAPE        : return EDisplayOrientation::Landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED: return EDisplayOrientation::LandscapeFlipped;
    case SDL_ORIENTATION_PORTRAIT         : return EDisplayOrientation::Portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED : return EDisplayOrientation::PortraitFlipped;
    case SDL_ORIENTATION_UNKNOWN          : [[fallthrough]];
    default                               : return EDisplayOrientation::Unknown;
    }
}

///
/// @section Constructor
///

FSDL3Display::FSDL3Display(FSDL3Display::FDisplayId displayId) noexcept
    : m_displayId(displayId)
{
    Refresh();
}

///
/// @section Methods - SDL3-specific
///

void FSDL3Display::Refresh() noexcept
{
    QueryIdentity();
    QueryGeometry();
    QueryModes();
    QueryPhysicalProperties();
    QueryCapabilities();
}

GP_NODISCARD FSDL3Display::FDisplayId FSDL3Display::GetSDLDisplayId() const noexcept { return m_displayId; }

///
/// @section Identity
///

GP_NODISCARD SizeT FSDL3Display::GetId() const noexcept { return static_cast<SizeT>(m_displayId); }

GP_NODISCARD FStringView FSDL3Display::GetName() const noexcept
{
    return m_name ? FStringView(m_name) : FStringView("");
}

GP_NODISCARD void* FSDL3Display::GetNativeHandle() const noexcept
{
    return reinterpret_cast<void*>(static_cast<UIntPtrT>(m_displayId));
}

GP_NODISCARD bool FSDL3Display::IsPrimary() const noexcept { return m_isPrimary; }

///
/// @section Screen Geometry
///

GP_NODISCARD FIntPoint2D FSDL3Display::GetPosition() const noexcept { return m_position; }

GP_NODISCARD FIntExtent2D FSDL3Display::GetSize() const noexcept { return m_size; }

GP_NODISCARD FIntPoint2D FSDL3Display::GetWorkAreaPosition() const noexcept { return m_workAreaPosition; }

GP_NODISCARD FIntExtent2D FSDL3Display::GetWorkAreaSize() const noexcept { return m_workAreaSize; }

///
/// @section Display Modes
///

GP_NODISCARD FDisplayMode FSDL3Display::GetCurrentMode() const noexcept { return m_currentMode; }

GP_NODISCARD TArrayView<const FDisplayMode> FSDL3Display::GetSupportedModes() const noexcept
{
    return m_supportedModes.AsView();
}

GP_NODISCARD bool FSDL3Display::SupportsMode(const FDisplayMode& mode) const noexcept
{
    return m_supportedModes.Contains(mode);
}

///
/// @section Physical Properties
///

GP_NODISCARD Float32 FSDL3Display::GetDPI() const noexcept { return m_dpi; }

GP_NODISCARD Float32 FSDL3Display::GetContentScale() const noexcept { return m_contentScale; }

GP_NODISCARD EDisplayOrientation FSDL3Display::GetOrientation() const noexcept { return m_orientation; }

///
/// @section Capabilities
///

GP_NODISCARD EDisplayColorSpace FSDL3Display::GetColorSpace() const noexcept { return m_colorSpace; }

GP_NODISCARD bool FSDL3Display::SupportsHDR() const noexcept { return m_supportsHDR; }

GP_NODISCARD bool FSDL3Display::IsHDREnabled() const noexcept { return m_hdrEnabled; }

GP_NODISCARD Float32 FSDL3Display::GetPeakLuminance() const noexcept { return m_peakLuminance; }

GP_NODISCARD Float32 FSDL3Display::GetMinLuminance() const noexcept { return m_minLuminance; }

///
/// @section Query Helpers
///

void FSDL3Display::QueryIdentity() noexcept
{
    m_name = SDL_GetDisplayName(m_displayId);
    m_isPrimary = (m_displayId == SDL_GetPrimaryDisplay());
}

void FSDL3Display::QueryGeometry() noexcept
{
    // Full display bounds (including OS-reserved shell area).
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(m_displayId, &bounds))
    {
        m_position = { bounds.x, bounds.y };
        m_size = { static_cast<UInt32>(bounds.w), static_cast<UInt32>(bounds.h) };
    }

    // Usable work area (excludes taskbar, Dock, menu bar, etc.).
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(m_displayId, &usable))
    {
        m_workAreaPosition = { usable.x, usable.y };
        m_workAreaSize = { static_cast<UInt32>(usable.w), static_cast<UInt32>(usable.h) };
    }
}

void FSDL3Display::QueryModes() noexcept
{
    m_supportedModes.Clear();

    // Cache the current (desktop) mode. This is the mode the compositor is using right now
    // and is always valid, even on platforms that do not enumerate fullscreen modes.
    const SDL_DisplayMode* sdlCurrent = SDL_GetCurrentDisplayMode(m_displayId);
    if (sdlCurrent) { m_currentMode = ConvertSDLMode(*sdlCurrent); }

    // Enumerate all fullscreen-capable modes. SDL3 returns a heap-allocated null-terminated
    // array of pointers; we are responsible for freeing it with SDL_free.
    int count = 0;
    SDL_DisplayMode** sdlModes = SDL_GetFullscreenDisplayModes(m_displayId, &count);

    if (sdlModes)
    {
        m_supportedModes.Reserve(static_cast<SizeT>(count));

        for (int i = 0; i < count; ++i)
        {
            if (sdlModes[i])
            {
                FDisplayMode converted = ConvertSDLMode(*sdlModes[i]);

                // Avoid duplicates that arise when SDL reports the same resolution at
                // multiple pixel densities (pixel_density > 1 on HiDPI outputs).
                if (converted.IsValid()) { m_supportedModes.PushBack(converted); }
            }
        }

        SDL_free(sdlModes);
    }

    // Guarantee the list is in ascending order (pixel count → Hz → bpp) so that callers
    // can use std::ranges::max_element to find the best mode without additional sorting.
    FDisplayMode* first = m_supportedModes.Data();
    FDisplayMode* last = first + m_supportedModes.Size();
    std::sort(first, last);
}

void FSDL3Display::QueryPhysicalProperties() noexcept
{
    // Content scale: SDL3 reports the OS DPI scaling factor for this display.
    // Returns 0.0f on failure; clamp to 1.0f (no scaling) as a safe fallback.
    {
        const float scale = SDL_GetDisplayContentScale(m_displayId);
        m_contentScale = (scale > 0.0f) ? scale : 1.0f;
    }

    // Effective logical DPI: SDL3 does not expose raw hardware DPI or EDID physical
    // dimensions. We derive an approximate logical DPI from the OS content scale and
    // the platform's reference baseline (96 DPI on Windows / Linux; 72 DPI on Apple).
    // This matches the value that UI frameworks such as Qt and wxWidgets report.
#if GP_PLATFORM_APPLE
    m_dpi = m_contentScale * kBaselineDPIApple;
#else
    m_dpi = m_contentScale * kBaselineDPIDesktop;
#endif

    // Orientation.
    m_orientation = ConvertSDLOrientation(SDL_GetCurrentDisplayOrientation(m_displayId));

    // GetRawDPI() and GetPhysicalSizeMillimeters() cannot be implemented through SDL3's
    // public API (no EDID access). Both return {0, 0} via the base class defaults and
    // are intentionally not overridden here.
}

void FSDL3Display::QueryCapabilities() noexcept
{
    // SDL3 exposes HDR metadata through the display's property bag.
    // Properties not available on the current platform return the supplied default.
    const SDL_PropertiesID props = SDL_GetDisplayProperties(m_displayId);

    m_hdrEnabled = SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);

    // SDL3 currently exposes only SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN at the display level.
    // Some SDL branches may add per-display SDR white / HDR headroom properties in the future;
    // compile-time guards keep this implementation forward-compatible.
    Float32 sdrWhitePoint = 80.0f;
    Float32 hdrHeadroom = 1.0f;

#if defined(SDL_PROP_DISPLAY_SDR_WHITE_POINT_FLOAT)
    // SDR white point: luminance (nits / cd·m^-2) corresponding to 1.0 in SDR.
    sdrWhitePoint = SDL_GetFloatProperty(props, SDL_PROP_DISPLAY_SDR_WHITE_POINT_FLOAT, 80.0f);
#endif

#if defined(SDL_PROP_DISPLAY_HDR_HEADROOM_FLOAT)
    // HDR headroom: multiplicative ratio of peak HDR luminance to SDR white.
    hdrHeadroom = SDL_GetFloatProperty(props, SDL_PROP_DISPLAY_HDR_HEADROOM_FLOAT, 1.0f);
    m_supportsHDR = (hdrHeadroom > kHDRHeadroomThreshold);
#else
    // No headroom metric available from SDL display properties.
    // Treat the boolean as best-effort HDR capability information only.
    m_supportsHDR = m_hdrEnabled;
#endif

    m_peakLuminance = sdrWhitePoint * hdrHeadroom;

    // SDL3 does not expose minimum luminance (black level) via any public property.
    // Callers should treat 0.0f as "not available" rather than "perfectly black".
    m_minLuminance = 0.0f;

    // Infer the colour space from the HDR metadata.
    //
    // SDL3 does not expose gamut information (BT.709 vs P3 vs BT.2020) through its
    // public API. The following heuristic is the best achievable approximation
    // without dropping to DXGI (Windows), AVDisplayManager (macOS), or DRM (Linux):
    //
    //   HDR enabled  + headroom > threshold → HDR10 ST.2084 (BT.2020 primaries, PQ EOTF)
    //   HDR capable  + HDR disabled         → Wide-gamut panel, assume DCI-P3
    //   No HDR range                        → Standard sRGB (BT.709)
    if (m_hdrEnabled && m_supportsHDR) { m_colorSpace = EDisplayColorSpace::HDR10_PQ; }
    else if (m_supportsHDR)
    {
        // The display can reproduce extended range but the OS HDR toggle is off.
        // Wide-gamut panels (P3) are the dominant hardware class in this category.
        m_colorSpace = EDisplayColorSpace::DisplayP3;
    }
    else { m_colorSpace = EDisplayColorSpace::sRGB; }
}

}   // namespace GP
