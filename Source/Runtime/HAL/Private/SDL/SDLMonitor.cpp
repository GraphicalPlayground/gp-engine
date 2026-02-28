// Copyright (c) - Graphical Playground. All rights reserved.

#include "SDL/SDLMonitor.hpp"
#include <cfloat>
#include <cstdio>

namespace GP::HAL
{

SDLMonitor::SDLMonitor(SDL_DisplayID displayID) noexcept
    : FBaseMonitor(false)
    , m_displayID(displayID)
{
    Refresh();
}

GP_NODISCARD bool SDLMonitor::IsPrimary() const noexcept
{
    return (m_displayID != 0u) && (SDL_GetPrimaryDisplay() == m_displayID);
}

GP_NODISCARD FIntRectangle SDLMonitor::GetVirtualDesktopBounds() const noexcept { return QueryBounds(); }

GP_NODISCARD FIntRectangle SDLMonitor::GetWorkAreaBounds() const noexcept { return QueryWorkArea(); }

GP_NODISCARD FIntPoint2D SDLMonitor::GetVirtualDesktopPosition() const noexcept
{
    const FIntRectangle bounds = QueryBounds();
    return bounds.position;
}

GP_NODISCARD Int32 SDLMonitor::GetVirtualDesktopX() const noexcept { return QueryBounds().position.x; }

GP_NODISCARD Int32 SDLMonitor::GetVirtualDesktopY() const noexcept { return QueryBounds().position.y; }

GP_NODISCARD FIntPoint2D SDLMonitor::GetVirtualDesktopCenter() const noexcept
{
    const FIntRectangle bounds = QueryBounds();
    return { bounds.position.x + static_cast<Int32>(bounds.size.width / 2u),
             bounds.position.y + static_cast<Int32>(bounds.size.height / 2u) };
}

GP_NODISCARD FIntExtent2D SDLMonitor::GetNativeResolution() const noexcept
{
    const FVideoMode mode = QueryCurrentMode();
    return FIntExtent2D{ mode.width, mode.height };
}

GP_NODISCARD UInt32 SDLMonitor::GetNativeWidth() const noexcept { return QueryCurrentMode().width; }

GP_NODISCARD UInt32 SDLMonitor::GetNativeHeight() const noexcept { return QueryCurrentMode().height; }

GP_NODISCARD Float32 SDLMonitor::GetAspectRatio() const noexcept
{
    const FVideoMode mode = QueryCurrentMode();
    return (mode.height > 0u) ? (static_cast<Float32>(mode.width) / static_cast<Float32>(mode.height)) : 0.0f;
}

GP_NODISCARD Float32 SDLMonitor::GetContentScaleX() const noexcept { return QueryContentScale(); }

GP_NODISCARD Float32 SDLMonitor::GetContentScaleY() const noexcept { return QueryContentScale(); }

GP_NODISCARD Float32 SDLMonitor::GetDPIX() const noexcept { return QueryContentScale() * DEFAULT_DPI_X; }

GP_NODISCARD Float32 SDLMonitor::GetDPIY() const noexcept { return QueryContentScale() * DEFAULT_DPI_Y; }

GP_NODISCARD Float32 SDLMonitor::GetPhysicalDPIX() const noexcept
{
    // SDL3 does not provide raw physical DPI; fall back to the logical DPI estimate.
    return GetDPIX();
}

GP_NODISCARD Float32 SDLMonitor::GetPhysicalDPIY() const noexcept { return GetDPIY(); }

GP_NODISCARD bool SDLMonitor::IsHighDPI() const noexcept { return QueryContentScale() > 1.0f; }

GP_NODISCARD UInt32 SDLMonitor::GetRefreshRate() const noexcept { return QueryCurrentMode().refreshRateHz; }

GP_NODISCARD FVideoMode SDLMonitor::GetCurrentVideoMode() const noexcept { return QueryCurrentMode(); }

GP_NODISCARD EMonitorOrientation SDLMonitor::GetOrientation() const noexcept
{
    if (m_displayID == 0u) { return EMonitorOrientation::Landscape; }
    return ConvertOrientation(SDL_GetCurrentDisplayOrientation(m_displayID));
}

GP_NODISCARD bool SDLMonitor::IsPortrait() const noexcept
{
    const EMonitorOrientation o = GetOrientation();
    return o == EMonitorOrientation::Portrait || o == EMonitorOrientation::PortraitFlipped;
}

GP_NODISCARD bool SDLMonitor::IsHDREnabled() const noexcept
{
    if (m_displayID == 0u) { return false; }
    const SDL_PropertiesID props = SDL_GetDisplayProperties(m_displayID);
    if (props == 0u) { return false; }
    return SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);
}

GP_NODISCARD void* SDLMonitor::GetNativeHandle() const noexcept
{
    if (m_displayID == 0u) { return nullptr; }
    const SDL_PropertiesID props = SDL_GetDisplayProperties(m_displayID);
    if (props == 0u) { return nullptr; }

#if GP_PLATFORM_WINDOWS
    return SDL_GetPointerProperty(props, SDL_PROP_DISPLAY_WINDOWS_HMONITOR_POINTER, nullptr);
#elif GP_PLATFORM_LINUX
    return SDL_GetPointerProperty(props, SDL_PROP_DISPLAY_WAYLAND_WL_OUTPUT_POINTER, nullptr);
#else
    return nullptr;
#endif
}

GP_NODISCARD FVideoMode SDLMonitor::ConvertDisplayMode(const SDL_DisplayMode& mode) noexcept
{
    FVideoMode vm{};
    vm.width = (mode.w > 0) ? static_cast<UInt32>(mode.w) : 0u;
    vm.height = (mode.h > 0) ? static_cast<UInt32>(mode.h) : 0u;
    vm.refreshRateHz = (mode.refresh_rate > 0.0f)
                           ? static_cast<UInt32>(mode.refresh_rate + 0.5f)   // nearest-integer rounding
                           : 0u;

    // Extract per-channel bit depths from the SDL pixel format descriptor.
    if (const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(mode.format))
    {
        vm.redBits = details->Rbits;
        vm.greenBits = details->Gbits;
        vm.blueBits = details->Bbits;
    }
    else
    {
        // Fallback to standard 8-bit per channel when format details are unavailable.
        vm.redBits = 8u;
        vm.greenBits = 8u;
        vm.blueBits = 8u;
    }

    return vm;
}

GP_NODISCARD EMonitorOrientation SDLMonitor::ConvertOrientation(SDL_DisplayOrientation orientation) noexcept
{
    switch (orientation)
    {
    case SDL_ORIENTATION_LANDSCAPE        : return EMonitorOrientation::Landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED: return EMonitorOrientation::LandscapeFlipped;
    case SDL_ORIENTATION_PORTRAIT         : return EMonitorOrientation::Portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED : return EMonitorOrientation::PortraitFlipped;
    case SDL_ORIENTATION_UNKNOWN          :
    default                               : return EMonitorOrientation::Landscape;
    }
}

GP_NODISCARD FIntRectangle SDLMonitor::QueryBounds() const noexcept
{
    if (m_displayID == 0u) { return FBaseMonitor::GetVirtualDesktopBounds(); }

    SDL_Rect rect{};
    if (!SDL_GetDisplayBounds(m_displayID, &rect)) { return FBaseMonitor::GetVirtualDesktopBounds(); }

    FIntRectangle result{};
    result.position.x = rect.x;
    result.position.y = rect.y;
    result.size.width = static_cast<UInt32>(rect.w > 0 ? rect.w : 0);
    result.size.height = static_cast<UInt32>(rect.h > 0 ? rect.h : 0);
    return result;
}

GP_NODISCARD FIntRectangle SDLMonitor::QueryWorkArea() const noexcept
{
    if (m_displayID == 0u) { return FBaseMonitor::GetWorkAreaBounds(); }

    SDL_Rect rect{};
    if (!SDL_GetDisplayUsableBounds(m_displayID, &rect)) { return FBaseMonitor::GetWorkAreaBounds(); }

    FIntRectangle result{};
    result.position.x = rect.x;
    result.position.y = rect.y;
    result.size.width = static_cast<UInt32>(rect.w > 0 ? rect.w : 0);
    result.size.height = static_cast<UInt32>(rect.h > 0 ? rect.h : 0);
    return result;
}

GP_NODISCARD Float32 SDLMonitor::QueryContentScale() const noexcept
{
    if (m_displayID == 0u) { return DEFAULT_CONTENT_SCALE; }
    const Float32 scale = SDL_GetDisplayContentScale(m_displayID);
    return (scale > 0.0f) ? scale : DEFAULT_CONTENT_SCALE;
}

GP_NODISCARD FVideoMode SDLMonitor::QueryCurrentMode() const noexcept
{
    if (m_displayID == 0u) { return m_currentVideoMode; }
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(m_displayID);
    return (mode != nullptr) ? ConvertDisplayMode(*mode) : m_currentVideoMode;
}

GP_NODISCARD SDL_DisplayID SDLMonitor::GetDisplayID() const noexcept { return m_displayID; }

void SDLMonitor::Refresh() noexcept
{
    if (m_displayID == 0u) { return; }

    if (const char* name = SDL_GetDisplayName(m_displayID)) { m_name = FString(name); }

    {
        // TODO: Replace by GP::Format when available.
        char buf[32];
        std::snprintf(buf, sizeof(buf), "SDL_DISPLAY_%u", static_cast<UInt32>(m_displayID));
        m_uniqueID = FString(buf);
    }

    m_isPrimary = (SDL_GetPrimaryDisplay() == m_displayID);
    if (m_isPrimary)
    {
        m_capabilityFlags = Enums::SetFlags(m_capabilityFlags, EMonitorCapabilityFlags::PrimaryDisplay);
    }

    // SDL3 removed SDL_GetDisplayDPI; the content scale factor is the canonical source for HiDPI awareness.
    const Float32 scale = SDL_GetDisplayContentScale(m_displayID);
    m_contentScaleX = (scale > 0.0f) ? scale : DEFAULT_CONTENT_SCALE;
    m_contentScaleY = m_contentScaleX;
    m_isHighDPI = (m_contentScaleX > 1.0f);
    // Derive logical DPI by multiplying the 96 dpi reference by the content scale.
    m_dpiX = m_contentScaleX * DEFAULT_DPI_X;
    m_dpiY = m_contentScaleY * DEFAULT_DPI_Y;
    // Physical DPI cannot be determined without EDID physical-size data (not exposed by SDL3).
    m_physicalDpiX = m_dpiX;
    m_physicalDpiY = m_dpiY;
    if (m_isHighDPI) { m_capabilityFlags = Enums::SetFlags(m_capabilityFlags, EMonitorCapabilityFlags::HighDPI); }

    if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(m_displayID))
    {
        m_currentVideoMode = ConvertDisplayMode(*mode);
        m_refreshRate = m_currentVideoMode.refreshRateHz;
    }

    m_supportedVideoModes.Clear();

    int modeCount = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(m_displayID, &modeCount);
    if (modes != nullptr && modeCount > 0)
    {
        m_supportedVideoModes.Reserve(static_cast<SizeT>(modeCount));

        Float32 maxRefreshF = 0.0f;
        Float32 minRefreshF = FLT_MAX;

        for (int i = 0; i < modeCount; ++i)
        {
            if (modes[i] == nullptr) { continue; }

            m_supportedVideoModes.PushBack(ConvertDisplayMode(*modes[i]));

            if (modes[i]->refresh_rate > maxRefreshF) { maxRefreshF = modes[i]->refresh_rate; }
            if (modes[i]->refresh_rate > 0.0f && modes[i]->refresh_rate < minRefreshF)
            {
                minRefreshF = modes[i]->refresh_rate;
            }
        }

        SDL_free(modes);

        m_maxRefreshRate = static_cast<UInt32>(maxRefreshF);
        m_minRefreshRate = (minRefreshF < FLT_MAX) ? static_cast<UInt32>(minRefreshF) : m_refreshRate;
    }
    else
    {
        if (modes != nullptr) { SDL_free(modes); }

        // Fallback: the current mode is the only known supported mode.
        m_supportedVideoModes.PushBack(m_currentVideoMode);
        m_minRefreshRate = m_refreshRate;
        m_maxRefreshRate = m_refreshRate;
    }

    m_orientation = ConvertOrientation(SDL_GetCurrentDisplayOrientation(m_displayID));

    const SDL_PropertiesID props = SDL_GetDisplayProperties(m_displayID);
    if (props != 0u)
    {
        m_isHDREnabled = SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);
        if (m_isHDREnabled)
        {
            // SDL3 does not report the specific HDR format; HDR10 is the most widely deployed standard.
            m_hdrFormat = EMonitorHDRFormat::HDR10;
            m_capabilityFlags = Enums::SetFlags(m_capabilityFlags, EMonitorCapabilityFlags::HDR);
        }

        // Retrieve the platform-native display handle from SDL's property bag.
#if GP_PLATFORM_WINDOWS
        m_nativeHandle = SDL_GetPointerProperty(props, SDL_PROP_DISPLAY_WINDOWS_HMONITOR_POINTER, nullptr);
#elif GP_PLATFORM_LINUX
        m_nativeHandle = SDL_GetPointerProperty(props, SDL_PROP_DISPLAY_WAYLAND_WL_OUTPUT_POINTER, nullptr);
#endif
    }
}

}   // namespace GP::HAL
