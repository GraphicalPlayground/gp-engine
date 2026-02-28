// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicString.hpp"
#include "Container/BasicStringView.hpp"
#include "Container/ContainerForward.hpp"
#include "Events/MulticastDelegate.hpp"
#include "Events/ScopedDelegateHandle.hpp"
#include "HALBuild.hpp"
#include "Math/Geometry/IntRectangle.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"
#include "Monitor/GammaRamp.hpp"
#include "Monitor/MonitorEnums.hpp"
#include "Monitor/MonitorEvents.hpp"
#include "Monitor/MonitorTypes.hpp"
#include "Monitor/VideoMode.hpp"

namespace GP::HAL
{

/// \brief Interface representing a single physical or virtual display monitor.
///
/// IMonitor exposes the complete set of observable and configurable properties for a display device, including its
/// geometry, supported video modes, color capabilities, HDR metadata, gamma ramp, and DPI information.
///
/// ### Coordinate System
/// All position/area values use the virtual-desktop coordinate system: the primary monitor's work area origin is
/// (0, 0). Secondary monitors may have negative or large positive coordinates. Physical pixel values are always
/// non-negative and independent of DPI scaling.
///
/// ### Event System Integration
/// Observable monitor state changes are surfaced as public TMulticastDelegate members. Subscribe using Add*()
/// and retain the returned handle:
/// \code
/// GP::Events::FScopedDelegateHandle m_modeHandle(
///     monitor.OnVideoModeChanged,
///     monitor.OnVideoModeChanged.AddLambda([](const FMonitorEvents::VideoModeChanged& e) {
///         UpdateSwapChainForNewMode(e.newWidth, e.newHeight, e.newRefreshRateHz);
///     })
/// );
/// \endcode
///
/// ### Threading
/// IMonitor instances are created and owned by the platform HAL. All methods must be called from the main thread
/// unless the implementation explicitly documents thread safety for a given method.
class IMonitor
{
public:
    using FEventHandle = GP::Events::FDelegateHandle;
    using FScopedHandle = GP::Events::FScopedDelegateHandle;
    template <typename Signature>
    using TEvent = GP::Events::TMulticastDelegate<Signature>;

public:
    /// \brief Virtual destructor to allow for proper cleanup of derived classes.
    virtual ~IMonitor() = default;

public:
    TEvent<void(const FMonitorEvents::Connected&)> OnConnected;
    TEvent<void(const FMonitorEvents::Disconnected&)> OnDisconnected;
    TEvent<void(const FMonitorEvents::VideoModeChanged&)> OnVideoModeChanged;
    TEvent<void(const FMonitorEvents::DPIChanged&)> OnDPIChanged;
    TEvent<void(const FMonitorEvents::OrientationChanged&)> OnOrientationChanged;
    TEvent<void(const FMonitorEvents::PrimaryChanged&)> OnPrimaryChanged;
    TEvent<void(const FMonitorEvents::HDRStateChanged&)> OnHDRStateChanged;
    TEvent<void(const FMonitorEvents::BrightnessChanged&)> OnBrightnessChanged;
    TEvent<void(const FMonitorEvents::Sleeping&)> OnSleeping;
    TEvent<void(const FMonitorEvents::WokeUp&)> OnWokeUp;
    TEvent<void(const FMonitorEvents::ColorProfileChanged&)> OnColorProfileChanged;

public:
    /// \brief Checks whether this monitor is the OS-designated primary display.
    /// \note The primary monitor typically hosts the OS taskbar/dock and defines the origin (0, 0) of the virtual
    /// desktop.
    /// \return True if this is the primary monitor, false otherwise.
    GP_NODISCARD virtual bool IsPrimary() const noexcept = 0;

    /// \brief Checks whether this monitor is currently connected and recognized by the OS.
    /// \return True if the monitor is connected, false if it has been unplugged or disabled.
    GP_NODISCARD virtual bool IsConnected() const noexcept = 0;

    /// \brief Checks whether this monitor is currently active (powered on and displaying output).
    /// \return True if the monitor is active, false if it is sleeping or powered off.
    GP_NODISCARD virtual bool IsActive() const noexcept = 0;

    /// \brief Checks whether this monitor is in a low-power (sleep / DPMS off) state.
    /// \return True if the monitor is sleeping, false otherwise.
    GP_NODISCARD virtual bool IsSleeping() const noexcept = 0;

    /// \brief Checks whether this monitor is a virtual / software display (e.g., headless rendering, RDP session,
    /// emulator).
    /// \return True if the monitor is virtual, false if it is a physical hardware display.
    GP_NODISCARD virtual bool IsVirtual() const noexcept = 0;

    /// \brief Checks whether this monitor is a built-in panel (laptop screen, tablet, all-in-one).
    /// \return True if the monitor is built-in / embedded, false if it is an external display.
    GP_NODISCARD virtual bool IsBuiltIn() const noexcept = 0;

    /// \brief Gets a human-readable name for the monitor, as reported by the OS or display EDID.
    /// \note The string lifetime is valid for as long as the IMonitor object is alive.
    /// \return The display name as a string view (e.g., "DELL U2722D", "Built-in Retina Display").
    GP_NODISCARD virtual FStringView GetName() const noexcept = 0;

    /// \brief Gets the OS-assigned unique identifier for this monitor.
    /// \note The format is platform-specific (e.g., a device path on Windows, an RROutput ID on Linux, a
    /// CGDirectDisplayID on macOS). It can be used for persistence (e.g., saving window-to-monitor assignments).
    /// \return The unique identifier as a string view.
    GP_NODISCARD virtual FStringView GetUniqueID() const noexcept = 0;

    /// \brief Gets the hardware manufacturer name as reported by the display's EDID.
    /// \return Manufacturer name (e.g., "Dell", "LG", "Samsung"), or an empty string if unavailable.
    GP_NODISCARD virtual FStringView GetManufacturerName() const noexcept = 0;

    /// \brief Gets the model identifier string as reported by the display's EDID.
    /// \return Model identifier (e.g., "U2722D"), or an empty string if unavailable.
    GP_NODISCARD virtual FStringView GetModelName() const noexcept = 0;

    /// \brief Gets the serial number of the monitor as reported by the EDID.
    /// \return Serial number string, or an empty string if unavailable.
    GP_NODISCARD virtual FStringView GetSerialNumber() const noexcept = 0;

    /// \brief Gets the physical connector type connecting this monitor to the GPU.
    /// \return The connector type enumeration value.
    GP_NODISCARD virtual EMonitorConnectorType GetConnectorType() const noexcept = 0;

    /// \brief Gets the panel technology used by this monitor.
    /// \return The panel type enumeration value.
    GP_NODISCARD virtual EMonitorPanelType GetPanelType() const noexcept = 0;

    /// \brief Gets the full bounds of the monitor in the virtual desktop coordinate space.
    /// \note This rectangle covers the entire monitor area, including any OS toolbars or docks. Coordinates are in
    /// logical (DPI-scaled) pixels. For physical pixel dimensions, use GetCurrentVideoMode().
    /// \return An FIntRect describing the monitor's position and size in the virtual desktop.
    GP_NODISCARD virtual FIntRectangle GetVirtualDesktopBounds() const noexcept = 0;

    /// \brief Gets the work area of the monitor — the usable region of the desktop excluding OS UI elements such as
    /// the taskbar (Windows), menu bar (macOS), or panel (Linux).
    /// \note Coordinates are in logical (DPI-scaled) pixels, consistent with GetVirtualDesktopBounds().
    /// \return An FIntRect describing the work area in virtual desktop coordinates.
    GP_NODISCARD virtual FIntRectangle GetWorkAreaBounds() const noexcept = 0;

    /// \brief Gets the position of the monitor's top-left corner in the virtual desktop coordinate space.
    /// \return The top-left position of the monitor, in logical pixels.
    GP_NODISCARD virtual FIntPoint2D GetVirtualDesktopPosition() const noexcept = 0;

    /// \brief Gets the X coordinate of the monitor's top-left corner in the virtual desktop.
    /// \return X offset in logical pixels.
    GP_NODISCARD virtual Int32 GetVirtualDesktopX() const noexcept = 0;

    /// \brief Gets the Y coordinate of the monitor's top-left corner in the virtual desktop.
    /// \return Y offset in logical pixels.
    GP_NODISCARD virtual Int32 GetVirtualDesktopY() const noexcept = 0;

    /// \brief Gets the center point of the monitor in the virtual desktop coordinate space.
    /// \return The center position of the monitor, in logical pixels.
    GP_NODISCARD virtual FIntPoint2D GetVirtualDesktopCenter() const noexcept = 0;

    /// \brief Gets the native (physical) resolution of the monitor at its current video mode.
    /// \note Use this when allocating GPU framebuffers or swap chains. This differs from the logical (DPI-scaled)
    /// size reported by GetVirtualDesktopBounds() on HiDPI displays.
    /// \return The native resolution as an FIntExtent2D.
    GP_NODISCARD virtual FIntExtent2D GetNativeResolution() const noexcept = 0;

    /// \brief Gets the native (physical) width of the monitor at its current video mode.
    /// \return Width in physical pixels.
    GP_NODISCARD virtual UInt32 GetNativeWidth() const noexcept = 0;

    /// \brief Gets the native (physical) height of the monitor at its current video mode.
    /// \return Height in physical pixels.
    GP_NODISCARD virtual UInt32 GetNativeHeight() const noexcept = 0;

    /// \brief Gets the aspect ratio of the monitor based on its current native resolution.
    /// \return Width divided by height. Returns 0.0f if the height is zero.
    GP_NODISCARD virtual Float32 GetAspectRatio() const noexcept = 0;

    /// \brief Gets the physical (hardware) size of the display panel.
    /// \return An FMonitorPhysicalSize containing the width and height in millimetres.
    GP_NODISCARD virtual FMonitorPhysicalSize GetPhysicalSize() const noexcept = 0;

    /// \brief Gets the horizontal dots-per-inch (DPI) of the monitor, as reported by the OS.
    /// \note On Windows, this reflects the OS-configured DPI (96 * scale factor). On macOS, it reflects the physical
    /// DPI. On Linux/Wayland, availability depends on the compositor.
    /// \return Horizontal DPI as a floating-point value.
    GP_NODISCARD virtual Float32 GetDPIX() const noexcept = 0;

    /// \brief Gets the vertical dots-per-inch (DPI) of the monitor, as reported by the OS.
    /// \return Vertical DPI as a floating-point value.
    GP_NODISCARD virtual Float32 GetDPIY() const noexcept = 0;

    /// \brief Gets the computed physical horizontal DPI based on native resolution and panel physical width.
    /// \note This value is derived entirely from EDID data and is unaffected by OS DPI scaling settings.
    /// \return Physical horizontal DPI. Returns 0.0f if physical size data is unavailable.
    GP_NODISCARD virtual Float32 GetPhysicalDPIX() const noexcept = 0;

    /// \brief Gets the computed physical vertical DPI based on native resolution and panel physical height.
    /// \return Physical vertical DPI. Returns 0.0f if physical size data is unavailable.
    GP_NODISCARD virtual Float32 GetPhysicalDPIY() const noexcept = 0;

    /// \brief Gets the horizontal content scale factor for this monitor.
    /// \note This is the ratio of physical pixels to logical pixels along the X axis (e.g., 2.0 on a Retina display).
    /// \return Horizontal content scale factor (1.0 = standard DPI).
    GP_NODISCARD virtual Float32 GetContentScaleX() const noexcept = 0;

    /// \brief Gets the vertical content scale factor for this monitor.
    /// \return Vertical content scale factor (1.0 = standard DPI).
    GP_NODISCARD virtual Float32 GetContentScaleY() const noexcept = 0;

    /// \brief Checks whether the monitor is a high-DPI display (any content scale axis is strictly greater than 1.0).
    /// \return True if the display is high-DPI / Retina, false otherwise.
    GP_NODISCARD virtual bool IsHighDPI() const noexcept = 0;

    /// \brief Gets the current refresh rate of the monitor.
    /// \return The current refresh rate in Hz.
    GP_NODISCARD virtual UInt32 GetRefreshRate() const noexcept = 0;

    /// \brief Gets the minimum refresh rate supported by the monitor (relevant for VRR).
    /// \note For monitors without VRR, the minimum equals the maximum (i.e., GetRefreshRate()).
    /// \return Minimum supported refresh rate in Hz.
    GP_NODISCARD virtual UInt32 GetMinRefreshRate() const noexcept = 0;

    /// \brief Gets the maximum refresh rate supported by the monitor across all enumerated video modes.
    /// \return Maximum supported refresh rate in Hz.
    GP_NODISCARD virtual UInt32 GetMaxRefreshRate() const noexcept = 0;

    /// \brief Gets the currently active video mode of the monitor.
    /// \return An FVideoMode describing the current resolution, refresh rate, and bit depth.
    GP_NODISCARD virtual FVideoMode GetCurrentVideoMode() const noexcept = 0;

    /// \brief Gets a read-only view of all video modes supported by this monitor.
    /// \note The returned span remains valid until the monitor is disconnected or the mode list is refreshed.
    /// \return A span of FVideoMode structs representing each supported mode, sorted by descending area then
    /// descending refresh rate.
    GP_NODISCARD virtual std::span<const FVideoMode> GetSupportedVideoModes() const noexcept = 0;

    /// \brief Gets the VRR (variable refresh rate) technology supported by this monitor.
    /// \return The VRR type, or EMonitorVRRType::None if VRR is not supported.
    GP_NODISCARD virtual EMonitorVRRType GetVRRType() const noexcept = 0;

    /// \brief Checks whether the monitor supports any form of variable refresh rate.
    /// \return True if VRR is available (FreeSync, G-Sync, or HDMI VRR), false otherwise.
    GP_NODISCARD virtual bool SupportsVRR() const noexcept = 0;

    /// \brief Gets the color gamut the monitor natively covers.
    /// \return The color gamut enumeration value.
    GP_NODISCARD virtual EMonitorColorGamut GetColorGamut() const noexcept = 0;

    /// \brief Gets the HDR format / standard supported by the monitor.
    /// \return The HDR format, or EMonitorHDRFormat::None if HDR is not supported.
    GP_NODISCARD virtual EMonitorHDRFormat GetHDRFormat() const noexcept = 0;

    /// \brief Checks whether the monitor currently has HDR output enabled at the OS level.
    /// \note A monitor may support HDR (GetHDRFormat() != None) while HDR is currently disabled in OS settings.
    /// \return True if HDR is currently active, false otherwise.
    GP_NODISCARD virtual bool IsHDREnabled() const noexcept = 0;

    /// \brief Checks whether the monitor supports HDR output at the hardware level.
    /// \return True if the monitor can display HDR content, false otherwise.
    GP_NODISCARD virtual bool SupportsHDR() const noexcept = 0;

    /// \brief Gets the HDR luminance metadata reported by the monitor's EDID.
    /// \note Values may be zero if the monitor does not support HDR or the driver cannot provide them.
    /// \return An FMonitorHDRMetadata struct with peak, frame-average, and minimum luminance values.
    GP_NODISCARD virtual FMonitorHDRMetadata GetHDRMetadata() const noexcept = 0;

    /// \brief Gets the maximum sustained luminance in nits (cd/m²) the panel can output.
    /// \return Peak luminance in nits, or 0.0f if unavailable.
    GP_NODISCARD virtual Float32 GetMaxLuminance() const noexcept = 0;

    /// \brief Gets the minimum achievable black luminance in nits.
    /// \return Minimum luminance in nits, or 0.0f if unavailable.
    GP_NODISCARD virtual Float32 GetMinLuminance() const noexcept = 0;

    /// \brief Gets the chromaticity and gamma information for this monitor as an FMonitorColorProfile.
    /// \note This is sourced from the OS color management system (ICC profile) when available, falling back to EDID
    /// chromaticity data.
    /// \return An FMonitorColorProfile struct describing the monitor's color space.
    GP_NODISCARD virtual FMonitorColorProfile GetColorProfile() const noexcept = 0;

    /// \brief Gets the current brightness level of the monitor.
    /// \note Availability is platform-dependent; some platforms only support brightness control for built-in panels.
    /// Returns 1.0f if the brightness cannot be queried.
    /// \return Brightness in the normalized range [0.0, 1.0].
    GP_NODISCARD virtual Float32 GetBrightness() const noexcept = 0;

    /// \brief Sets the brightness level of the monitor.
    /// \note This writes to the OS backlight control API (e.g., IOKit on macOS, ACPI/WMI on Windows) and is
    /// typically only available for built-in laptop or tablet panels. On unsupported monitors, this call is a no-op.
    /// \param brightness The desired brightness, clamped to [0.0, 1.0].
    virtual void SetBrightness(Float32 brightness) = 0;

    /// \brief Gets the number of entries in the monitor's hardware gamma ramp lookup table.
    /// \note Standard value is 256 (8-bit LUT). Some platforms expose 1024 or 4096 entries for higher precision.
    /// \return The number of LUT entries.
    GP_NODISCARD virtual UInt32 GetGammaRampSize() const noexcept = 0;

    /// \brief Gets the current hardware gamma ramp from the monitor.
    /// \note The returned ramp contains exactly GetGammaRampSize() entries. Values are normalized to [0.0, 1.0].
    /// \return An FGammaRamp reflecting the current hardware state. Returns an empty ramp if the query fails.
    GP_NODISCARD virtual FGammaRamp GetGammaRamp() const noexcept = 0;

    /// \brief Overwrites the monitor's hardware gamma ramp.
    /// \note The ramp must contain exactly GetGammaRampSize() entries. Setting a non-linear ramp can be used to
    /// apply color grading or gamma correction globally at the display hardware level.
    /// \warning Modifying the gamma ramp affects the entire screen output for all applications. Restore the default
    /// ramp on application exit to avoid leaving the display in a corrupted state.
    /// \param ramp The gamma ramp to apply. Its entries array size must equal GetGammaRampSize().
    virtual void SetGammaRamp(const FGammaRamp& ramp) = 0;

    /// \brief Resets the monitor's hardware gamma ramp to the OS default (linear ramp, gamma 2.2 approximation).
    virtual void ResetGammaRamp() = 0;

    /// \brief Gets the current physical orientation of the monitor panel.
    /// \return The current orientation (Landscape, Portrait, LandscapeFlipped, PortraitFlipped).
    GP_NODISCARD virtual EMonitorOrientation GetOrientation() const noexcept = 0;

    /// \brief Checks whether the monitor is currently in a portrait orientation (rotated 90 or 270 degrees).
    /// \return True if the monitor is in portrait orientation, false if landscape.
    GP_NODISCARD virtual bool IsPortrait() const noexcept = 0;

    /// \brief Gets the full capability flags bitmask for this monitor.
    /// \return A bitmask of EMonitorCapabilityFlags values describing all supported features.
    GP_NODISCARD virtual EMonitorCapabilityFlags GetCapabilityFlags() const noexcept = 0;

    /// \brief Checks whether the monitor has a specific capability.
    /// \param flag The capability flag to test.
    /// \return True if the monitor supports the specified capability, false otherwise.
    GP_NODISCARD virtual bool HasCapability(EMonitorCapabilityFlags flag) const noexcept = 0;

    /// \brief Checks whether the monitor has an integrated touch screen.
    /// \return True if touch input is available, false otherwise.
    GP_NODISCARD virtual bool HasTouchScreen() const noexcept = 0;

    /// \brief Checks whether the monitor exposes built-in audio output (e.g., HDMI or DisplayPort speakers).
    /// \return True if integrated audio output is present, false otherwise.
    GP_NODISCARD virtual bool HasBuiltInSpeakers() const noexcept = 0;

    /// \brief Checks whether the monitor supports local dimming zones (FALD / mini-LED).
    /// \return True if hardware local dimming is available, false otherwise.
    GP_NODISCARD virtual bool HasLocalDimming() const noexcept = 0;

    /// \brief Gets the native OS handle for this monitor.
    /// \note The concrete type is platform-dependent: HMONITOR on Windows, CGDirectDisplayID (cast to void*) on
    /// macOS, RROutput on Linux/X11, or wl_output* on Linux/Wayland. This handle can be used to call
    /// platform-specific APIs that require a direct monitor reference (e.g., swap-chain creation for DX12/Vulkan
    /// fullscreen exclusive mode).
    /// \return The native monitor handle as a type-erased void pointer. Never nullptr on a connected monitor.
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept = 0;
};
}   // namespace GP::HAL
