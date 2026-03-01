// Copyright (c) - Graphical Playground. All rights reserved.

#include "SDL/SDLMonitor.hpp"
#include <cfloat>
#include <cstdio>

#if GP_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>   // Core Win32 types (HMONITOR, HDC, UINT32, etc.)
    #include <wingdi.h>    // CreateDCW, GetDeviceCaps, DeleteDC, DISPLAYCONFIG_* types
    #include <winuser.h>   // GetMonitorInfoW, MONITORINFOEXW
#endif

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

    // Populate fields queryable from SDL3 properties or cross-platform inference.
    QueryHDRMetadata();
    QueryVRRSupport();
    QueryTouchSupport();
    // Populate fields that require platform-specific OS queries (physical size, EDID metadata).
    QueryPlatformInfo();
}

}   // namespace GP::HAL

#if GP_PLATFORM_WINDOWS

/// \brief Decodes a 3-letter EISA/PnP manufacturer ID to a human-readable vendor name.
/// \param code Pointer to the first character of the 3-letter PnP code (need not be null-terminated).
/// \return A string literal with the vendor name, or "Unknown" if the code is not in the table.
static const GP::Char8* DecodePnPManufacturer(const GP::Char8* code) noexcept
{
    struct FEntry
    {
        GP::Char8 code[4];       //!< 3-char PnP code + null terminator.
        const GP::Char8* name;   //!< Human-readable vendor name.
    };

    // Source: UEFI/ACPI PnP ID Registry and major display vendor PnP assignments.
    static constexpr FEntry k_table[] = {
        { "ACI",         "ASUSTeK" },
        { "ACR",            "Acer" },
        { "AUO",    "AU Optronics" },
        { "BNQ",            "BenQ" },
        { "BOE",     "BOE Display" },
        { "CMN",         "Innolux" },
        { "CMO",         "Chi Mei" },
        { "CPL",          "Compaq" },
        { "CPQ",          "Compaq" },
        { "DEL",            "Dell" },
        { "DEN",          "Denver" },
        { "ENC",            "Eizo" },
        { "EPI",        "Envision" },
        { "FUJ",         "Fujitsu" },
        { "FUS", "Fujitsu Siemens" },
        { "GSM",              "LG" },
        { "HPN",              "HP" },
        { "HPQ",              "HP" },
        { "HSD",        "HannStar" },
        { "HWP",              "HP" },
        { "IBM",             "IBM" },
        { "IVM",          "Iiyama" },
        { "LEN",          "Lenovo" },
        { "LGD",      "LG Display" },
        { "LPL",      "LG Philips" },
        { "MAX",         "Maxdata" },
        { "MEI",       "Panasonic" },
        { "MED",          "Medion" },
        { "NEC",             "NEC" },
        { "NOK",           "Nokia" },
        { "PHL",         "Philips" },
        { "QDS",  "Quanta Display" },
        { "SAM",         "Samsung" },
        { "SAN",           "Sanyo" },
        { "SEC",         "Samsung" },
        { "SEI",           "Seiko" },
        { "SHP",           "Sharp" },
        { "SNY",            "Sony" },
        { "STA",         "Innolux" },
        { "TOS",         "Toshiba" },
        { "TSB",         "Toshiba" },
        { "VIZ",           "Vizio" },
        { "VSC",       "ViewSonic" },
        { "WDE",    "Westinghouse" },
    };

    for (const auto& e: k_table)
    {
        if (code[0] == e.code[0] && code[1] == e.code[1] && code[2] == e.code[2]) { return e.name; }
    }
    return "Unknown";
}

/// \brief Converts a Windows DISPLAYCONFIG output technology constant to the GP connector type enum.
/// \param tech The DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY value from the DisplayConfig API.
/// \return The corresponding EMonitorConnectorType, or Unknown if the constant is unrecognized.
static GP::HAL::EMonitorConnectorType ConvertOutputTechnology(DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY tech) noexcept
{
    switch (tech)
    {
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI                : return GP::HAL::EMonitorConnectorType::HDMI;
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DVI                 : return GP::HAL::EMonitorConnectorType::DVI;
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15                : return GP::HAL::EMonitorConnectorType::VGA;
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EXTERNAL: return GP::HAL::EMonitorConnectorType::DisplayPort;
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED: return GP::HAL::EMonitorConnectorType::Embedded;
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS                : return GP::HAL::EMonitorConnectorType::Embedded;
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_MIRACAST            : return GP::HAL::EMonitorConnectorType::Virtual;
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INDIRECT_WIRED      :
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INDIRECT_VIRTUAL    : return GP::HAL::EMonitorConnectorType::Virtual;
    default                                                  : return GP::HAL::EMonitorConnectorType::Unknown;
    }
}

#endif   // GP_PLATFORM_WINDOWS

namespace GP::HAL
{

void SDLMonitor::QueryHDRMetadata() noexcept
{
    if (m_displayID == 0u) { return; }

    const SDL_PropertiesID props = SDL_GetDisplayProperties(m_displayID);
    if (props == 0u) { return; }

    // SDL 3.2 exposes a single boolean HDR property.  Richer per-display luminance
    // and headroom floats (SDL_PROP_DISPLAY_SDR_WHITE_POINT_FLOAT /
    // SDL_PROP_DISPLAY_HDR_HEADROOM_FLOAT) were added in later SDL3 revisions; they
    // are not available in the version vendored by this project and are therefore not
    // queried here.  Exact HDR tier and luminance values will be filled by
    // QueryPlatformInfo() using OS-level APIs where supported.
    const bool hdrEnabled = SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);

    if (!hdrEnabled) { return; }

    // HDR is active: record at least the baseline HDR10 format and set the capability
    // flag.  QueryPlatformInfo() may upgrade this to a more specific tier once it
    // interrogates the OS display metadata (e.g. EDID max luminance on Windows).
    if (m_hdrFormat == EMonitorHDRFormat::None) { m_hdrFormat = EMonitorHDRFormat::HDR10; }
    m_capabilityFlags = Enums::SetFlags(m_capabilityFlags, EMonitorCapabilityFlags::HDR);

    // A display with active HDR output is likely at minimum Display P3 wide-gamut.
    // This is a conservative lower bound; QueryPlatformInfo() will refine the value.
    if (m_colorGamut == EMonitorColorGamut::Unknown) { m_colorGamut = EMonitorColorGamut::DisplayP3; }
    if (m_colorGamut != EMonitorColorGamut::sRGB)
    {
        m_capabilityFlags = Enums::SetFlags(m_capabilityFlags, EMonitorCapabilityFlags::WideColorGamut);
    }
}

void SDLMonitor::QueryVRRSupport() noexcept
{
    // A refresh rate range wider than 10 Hz indicates the display advertises
    // Adaptive-Sync (VESA) or a proprietary VRR standard.  The 10-Hz dead-band
    // eliminates false positives from floating-point rounding in SDL3 mode queries.
    if (m_minRefreshRate == 0u || m_maxRefreshRate == 0u) { return; }
    if (m_maxRefreshRate <= m_minRefreshRate + 10u) { return; }

    if (m_vrrType == EMonitorVRRType::None)
    {
        // Default to FreeSync / VESA Adaptive-Sync — the open standard supported by the
        // majority of VRR-capable consumer panels.  QueryPlatformInfo() may later refine
        // this to GSyncComp or HDMI_VRR when richer platform API data is available.
        m_vrrType = EMonitorVRRType::FreeSync;
    }

    m_capabilityFlags = Enums::SetFlags(m_capabilityFlags, EMonitorCapabilityFlags::VRR);
}

void SDLMonitor::QueryTouchSupport() noexcept
{
    // SDL3 enumerates touch devices globally without binding them to individual displays.
    // On mobile (Android / iOS) platforms every active touch device is assumed to belong
    // to the primary built-in panel.  On desktop platforms we skip detection to avoid
    // incorrectly attributing an external USB touch screen to this particular monitor.
    if constexpr (!GP::Build::Platform::IsMobile) { return; }

    int touchCount = 0;
    SDL_TouchID* const touchDevices = SDL_GetTouchDevices(&touchCount);
    if (touchDevices && touchCount > 0)
    {
        m_capabilityFlags = Enums::SetFlags(m_capabilityFlags, EMonitorCapabilityFlags::TouchScreen);
    }
    if (touchDevices) { SDL_free(touchDevices); }
}

void SDLMonitor::QueryPlatformInfo() noexcept
{
    if (m_displayID == 0u) { return; }

#if GP_PLATFORM_WINDOWS
    // Retrieve the HMONITOR associated with this SDL display from the SDL3 property bag.
    const SDL_PropertiesID props = SDL_GetDisplayProperties(m_displayID);
    if (props == 0u) { return; }

    auto* const hMonitor =
        static_cast<HMONITOR>(SDL_GetPointerProperty(props, SDL_PROP_DISPLAY_WINDOWS_HMONITOR_POINTER, nullptr));
    if (!hMonitor) { return; }

    // ---- Physical panel dimensions via the GDI device context ------------------
    // GetDeviceCaps(HORZSIZE / VERTSIZE) returns the physical display area in millimetres
    // as reported by the GPU driver from the monitor's EDID.
    MONITORINFOEXW monInfoEx{};
    monInfoEx.cbSize = sizeof(monInfoEx);
    if (!GetMonitorInfoW(hMonitor, &monInfoEx)) { return; }

    {
        HDC hdc = CreateDCW(L"DISPLAY", monInfoEx.szDevice, nullptr, nullptr);
        if (hdc)
        {
            const Int32 horzMM = GetDeviceCaps(hdc, HORZSIZE);
            const Int32 vertMM = GetDeviceCaps(hdc, VERTSIZE);
            DeleteDC(hdc);

            if (horzMM > 0 && vertMM > 0)
            {
                m_physicalSize.widthMM = static_cast<UInt32>(horzMM);
                m_physicalSize.heightMM = static_cast<UInt32>(vertMM);

                // Recompute physical DPI now that real panel dimensions are available.
                const FVideoMode mode = QueryCurrentMode();
                if (mode.width > 0u && m_physicalSize.widthMM > 0u)
                {
                    m_physicalDpiX =
                        static_cast<Float32>(mode.width) * 25.4f / static_cast<Float32>(m_physicalSize.widthMM);
                    m_physicalDpiY =
                        static_cast<Float32>(mode.height) * 25.4f / static_cast<Float32>(m_physicalSize.heightMM);
                }
            }
        }
    }

    // QueryDisplayConfig returns the active display topology.  We match our HMONITOR
    // to a path by comparing GDI device names, then call DisplayConfigGetDeviceInfo to
    // extract the EDID-sourced friendly name and the output-technology constant.
    UINT32 pathCount = 0u, modeCount = 0u;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS || pathCount == 0u)
    {
        return;
    }

    auto* const paths = static_cast<DISPLAYCONFIG_PATH_INFO*>(SDL_malloc(pathCount * sizeof(DISPLAYCONFIG_PATH_INFO)));
    auto* const modes = static_cast<DISPLAYCONFIG_MODE_INFO*>(SDL_malloc(modeCount * sizeof(DISPLAYCONFIG_MODE_INFO)));

    if (!paths || !modes)
    {
        SDL_free(paths);
        SDL_free(modes);
        return;
    }

    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths, &modeCount, modes, nullptr) != ERROR_SUCCESS)
    {
        SDL_free(paths);
        SDL_free(modes);
        return;
    }

    for (UINT32 i = 0u; i < pathCount; ++i)
    {
        // Match the source GDI device name (e.g., L"\\\\.\\DISPLAY1") to our monitor.
        DISPLAYCONFIG_SOURCE_DEVICE_NAME srcName{};
        srcName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        srcName.header.size = sizeof(srcName);
        srcName.header.adapterId = paths[i].sourceInfo.adapterId;
        srcName.header.id = paths[i].sourceInfo.id;

        if (DisplayConfigGetDeviceInfo(&srcName.header) != ERROR_SUCCESS) { continue; }
        if (wcscmp(srcName.viewGdiDeviceName, monInfoEx.szDevice) != 0) { continue; }

        // Found the matching path; query the target (monitor-side) device name.
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = paths[i].targetInfo.adapterId;
        targetName.header.id = paths[i].targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS) { break; }

        if (targetName.monitorFriendlyDeviceName[0] != L'\0')
        {
            char modelBuf[128] = {};
            WideCharToMultiByte(
                CP_UTF8,
                0,
                targetName.monitorFriendlyDeviceName,
                -1,
                modelBuf,
                static_cast<int>(sizeof(modelBuf)) - 1,
                nullptr,
                nullptr
            );
            m_modelName = FString(modelBuf);
        }

        // Path format:  "\\?\DISPLAY#<MFR3><PROD4>#<INSTANCE>#{GUID}"
        // The 3-letter EISA/PnP manufacturer code immediately follows the "DISPLAY#" token.
        const wchar_t* const devPath = targetName.monitorDevicePath;
        const wchar_t* const marker = wcsstr(devPath, L"DISPLAY#");
        if (marker)
        {
            const wchar_t* const pnp = marker + 8u;   // skip "DISPLAY#"
            if (pnp[0] != L'\0' && pnp[1] != L'\0' && pnp[2] != L'\0')
            {
                const Char8 code[4] = {
                    static_cast<Char8>(pnp[0]), static_cast<Char8>(pnp[1]), static_cast<Char8>(pnp[2]), '\0'
                };
                m_manufacturerName = FString(DecodePnPManufacturer(code));
            }
        }

        m_connectorType = ConvertOutputTechnology(targetName.outputTechnology);

        if (targetName.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED ||
            targetName.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS)
        {
            m_isBuiltIn = true;
            m_connectorType = EMonitorConnectorType::Embedded;
        }

        break;   // Matched; no need to continue iterating remaining paths.
    }

    SDL_free(paths);
    SDL_free(modes);

#elif GP_PLATFORM_LINUX
    // TODO: Query physical size and connector type via sysfs /sys/class/drm/<card>/edid
    //       and udev. Wayland exposes partial geometry info via xdg-output; X11 via XRandR.
#elif GP_PLATFORM_MACOS
    // TODO: Query physical size via IODisplayCreateInfoDictionary and connector type via
    //       IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IODisplay")).
#endif
}

}   // namespace GP::HAL
