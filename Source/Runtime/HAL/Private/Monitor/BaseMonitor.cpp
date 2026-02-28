// Copyright (c) - Graphical Playground. All rights reserved.

#include "Monitor/BaseMonitor.hpp"

namespace GP::HAL
{

FBaseMonitor::FBaseMonitor(bool headless) noexcept
    : m_isHeadless(headless)
{
    // Initialise the current video mode to the default resolution.
    m_currentVideoMode.width = DEFAULT_NATIVE_WIDTH;
    m_currentVideoMode.height = DEFAULT_NATIVE_HEIGHT;
    m_currentVideoMode.refreshRateHz = DEFAULT_REFRESH_RATE;
    m_currentVideoMode.redBits = 8u;
    m_currentVideoMode.greenBits = 8u;
    m_currentVideoMode.blueBits = 8u;

    // Initialise the virtual desktop and work-area bounds to the default resolution.
    m_virtualDesktopBounds.size.width = DEFAULT_NATIVE_WIDTH;
    m_virtualDesktopBounds.size.height = DEFAULT_NATIVE_HEIGHT;
    m_workAreaBounds.size.width = DEFAULT_NATIVE_WIDTH;
    m_workAreaBounds.size.height = DEFAULT_NATIVE_HEIGHT;

    // Populate the supported video modes list with the single default mode.
    m_supportedVideoModes.PushBack(m_currentVideoMode);

    // Build an identity (linear) gamma ramp: entry i maps input i/(N-1) back to itself,
    // resulting in no gamma correction being applied.
    const Float32 step = 1.0f / static_cast<Float32>(DEFAULT_GAMMA_SIZE - 1u);
    m_gammaRamp.entries.Reserve(DEFAULT_GAMMA_SIZE);
    for (UInt32 i = 0u; i < DEFAULT_GAMMA_SIZE; ++i)
    {
        const Float32 v = step * static_cast<Float32>(i);
        m_gammaRamp.entries.PushBack(FGammaRampEntry{ v, v, v });
    }
}

GP_NODISCARD bool FBaseMonitor::IsPrimary() const noexcept { return m_isPrimary; }

GP_NODISCARD bool FBaseMonitor::IsConnected() const noexcept { return m_isConnected; }

GP_NODISCARD bool FBaseMonitor::IsActive() const noexcept { return m_isConnected && m_isActive && !m_isSleeping; }

GP_NODISCARD bool FBaseMonitor::IsSleeping() const noexcept { return m_isSleeping; }

GP_NODISCARD bool FBaseMonitor::IsVirtual() const noexcept { return m_isVirtual; }

GP_NODISCARD bool FBaseMonitor::IsBuiltIn() const noexcept { return m_isBuiltIn; }

GP_NODISCARD FStringView FBaseMonitor::GetName() const noexcept { return FStringView(m_name.Data(), m_name.Length()); }

GP_NODISCARD FStringView FBaseMonitor::GetUniqueID() const noexcept
{
    return FStringView(m_uniqueID.Data(), m_uniqueID.Length());
}

GP_NODISCARD FStringView FBaseMonitor::GetManufacturerName() const noexcept
{
    return FStringView(m_manufacturerName.Data(), m_manufacturerName.Length());
}

GP_NODISCARD FStringView FBaseMonitor::GetModelName() const noexcept
{
    return FStringView(m_modelName.Data(), m_modelName.Length());
}

GP_NODISCARD FStringView FBaseMonitor::GetSerialNumber() const noexcept
{
    return FStringView(m_serialNumber.Data(), m_serialNumber.Length());
}

GP_NODISCARD EMonitorConnectorType FBaseMonitor::GetConnectorType() const noexcept { return m_connectorType; }

GP_NODISCARD EMonitorPanelType FBaseMonitor::GetPanelType() const noexcept { return m_panelType; }

GP_NODISCARD FIntRectangle FBaseMonitor::GetVirtualDesktopBounds() const noexcept { return m_virtualDesktopBounds; }

GP_NODISCARD FIntRectangle FBaseMonitor::GetWorkAreaBounds() const noexcept { return m_workAreaBounds; }

GP_NODISCARD FIntPoint2D FBaseMonitor::GetVirtualDesktopPosition() const noexcept
{
    return m_virtualDesktopBounds.position;
}

GP_NODISCARD Int32 FBaseMonitor::GetVirtualDesktopX() const noexcept { return m_virtualDesktopBounds.position.x; }

GP_NODISCARD Int32 FBaseMonitor::GetVirtualDesktopY() const noexcept { return m_virtualDesktopBounds.position.y; }

GP_NODISCARD FIntPoint2D FBaseMonitor::GetVirtualDesktopCenter() const noexcept
{
    return { m_virtualDesktopBounds.position.x + static_cast<Int32>(m_virtualDesktopBounds.size.width / 2u),
             m_virtualDesktopBounds.position.y + static_cast<Int32>(m_virtualDesktopBounds.size.height / 2u) };
}

GP_NODISCARD FIntExtent2D FBaseMonitor::GetNativeResolution() const noexcept
{
    return FIntExtent2D{ m_currentVideoMode.width, m_currentVideoMode.height };
}

GP_NODISCARD UInt32 FBaseMonitor::GetNativeWidth() const noexcept { return m_currentVideoMode.width; }

GP_NODISCARD UInt32 FBaseMonitor::GetNativeHeight() const noexcept { return m_currentVideoMode.height; }

GP_NODISCARD Float32 FBaseMonitor::GetAspectRatio() const noexcept
{
    const UInt32 h = m_currentVideoMode.height;
    return (h > 0u) ? (static_cast<Float32>(m_currentVideoMode.width) / static_cast<Float32>(h)) : 0.0f;
}

GP_NODISCARD FMonitorPhysicalSize FBaseMonitor::GetPhysicalSize() const noexcept { return m_physicalSize; }

GP_NODISCARD Float32 FBaseMonitor::GetDPIX() const noexcept { return m_dpiX; }

GP_NODISCARD Float32 FBaseMonitor::GetDPIY() const noexcept { return m_dpiY; }

GP_NODISCARD Float32 FBaseMonitor::GetPhysicalDPIX() const noexcept { return m_physicalDpiX; }

GP_NODISCARD Float32 FBaseMonitor::GetPhysicalDPIY() const noexcept { return m_physicalDpiY; }

GP_NODISCARD Float32 FBaseMonitor::GetContentScaleX() const noexcept { return m_contentScaleX; }

GP_NODISCARD Float32 FBaseMonitor::GetContentScaleY() const noexcept { return m_contentScaleY; }

GP_NODISCARD bool FBaseMonitor::IsHighDPI() const noexcept { return m_isHighDPI; }

GP_NODISCARD UInt32 FBaseMonitor::GetRefreshRate() const noexcept { return m_refreshRate; }

GP_NODISCARD UInt32 FBaseMonitor::GetMinRefreshRate() const noexcept { return m_minRefreshRate; }

GP_NODISCARD UInt32 FBaseMonitor::GetMaxRefreshRate() const noexcept { return m_maxRefreshRate; }

GP_NODISCARD FVideoMode FBaseMonitor::GetCurrentVideoMode() const noexcept { return m_currentVideoMode; }

GP_NODISCARD std::span<const FVideoMode> FBaseMonitor::GetSupportedVideoModes() const noexcept
{
    return std::span<const FVideoMode>(m_supportedVideoModes.Data(), m_supportedVideoModes.Size());
}

GP_NODISCARD EMonitorVRRType FBaseMonitor::GetVRRType() const noexcept { return m_vrrType; }

GP_NODISCARD bool FBaseMonitor::SupportsVRR() const noexcept { return m_vrrType != EMonitorVRRType::None; }

GP_NODISCARD EMonitorColorGamut FBaseMonitor::GetColorGamut() const noexcept { return m_colorGamut; }

GP_NODISCARD EMonitorHDRFormat FBaseMonitor::GetHDRFormat() const noexcept { return m_hdrFormat; }

GP_NODISCARD bool FBaseMonitor::IsHDREnabled() const noexcept { return m_isHDREnabled; }

GP_NODISCARD bool FBaseMonitor::SupportsHDR() const noexcept { return m_hdrFormat != EMonitorHDRFormat::None; }

GP_NODISCARD FMonitorHDRMetadata FBaseMonitor::GetHDRMetadata() const noexcept { return m_hdrMetadata; }

GP_NODISCARD Float32 FBaseMonitor::GetMaxLuminance() const noexcept { return m_hdrMetadata.maxLuminance; }

GP_NODISCARD Float32 FBaseMonitor::GetMinLuminance() const noexcept { return m_hdrMetadata.minLuminance; }

GP_NODISCARD FMonitorColorProfile FBaseMonitor::GetColorProfile() const noexcept { return m_colorProfile; }

GP_NODISCARD Float32 FBaseMonitor::GetBrightness() const noexcept { return m_brightness; }

void FBaseMonitor::SetBrightness(Float32 brightness)
{
    m_brightness = brightness < 0.0f ? 0.0f : (brightness > 1.0f ? 1.0f : brightness);
}

GP_NODISCARD UInt32 FBaseMonitor::GetGammaRampSize() const noexcept
{
    return static_cast<UInt32>(m_gammaRamp.entries.Size());
}

GP_NODISCARD FGammaRamp FBaseMonitor::GetGammaRamp() const noexcept { return m_gammaRamp; }

void FBaseMonitor::SetGammaRamp(const FGammaRamp& ramp)
{
    GP_UNUSED(ramp);
    // Platform-specific subclasses should override this to apply the ramp via the OS / GPU API.
}

void FBaseMonitor::ResetGammaRamp()
{
    // Platform-specific subclasses should override this to restore the system default gamma ramp.
}

GP_NODISCARD EMonitorOrientation FBaseMonitor::GetOrientation() const noexcept { return m_orientation; }

GP_NODISCARD bool FBaseMonitor::IsPortrait() const noexcept
{
    return m_orientation == EMonitorOrientation::Portrait || m_orientation == EMonitorOrientation::PortraitFlipped;
}

GP_NODISCARD EMonitorCapabilityFlags FBaseMonitor::GetCapabilityFlags() const noexcept { return m_capabilityFlags; }

GP_NODISCARD bool FBaseMonitor::HasCapability(EMonitorCapabilityFlags flag) const noexcept
{
    return Enums::HasAll(m_capabilityFlags, flag);
}

GP_NODISCARD bool FBaseMonitor::HasTouchScreen() const noexcept
{
    return HasCapability(EMonitorCapabilityFlags::TouchScreen);
}

GP_NODISCARD bool FBaseMonitor::HasBuiltInSpeakers() const noexcept
{
    return HasCapability(EMonitorCapabilityFlags::BuiltInSpeakers);
}

GP_NODISCARD bool FBaseMonitor::HasLocalDimming() const noexcept
{
    return HasCapability(EMonitorCapabilityFlags::LocalDimming);
}

GP_NODISCARD void* FBaseMonitor::GetNativeHandle() const noexcept { return m_nativeHandle; }

}   // namespace GP::HAL
