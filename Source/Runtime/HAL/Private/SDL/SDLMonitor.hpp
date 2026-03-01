// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Monitor/BaseMonitor.hpp"
#include <SDL3/SDL.h>

namespace GP::HAL
{

/// \brief SDL3-backed concrete implementation of the IMonitor interface.
///
/// Static display properties (name, supported video modes, physical size, etc.) are queried from SDL3 once
/// during construction and cached in FBaseMonitor's protected state. Truly dynamic properties — those that
/// may change at runtime without a corresponding event being dispatched (content scale, current video mode,
/// orientation, HDR enablement) — are re-queried from SDL3 on every accessor call to guarantee freshness.
///
/// \note Gamma-ramp control is not available via SDL3 (removed from SDL's public API). SetGammaRamp() and
///       ResetGammaRamp() are intentional no-ops on all platforms when using this implementation.
/// \note Display brightness is not exposed by SDL3. SetBrightness() / GetBrightness() operate on the
///       in-memory state only and do not affect the physical display.
class SDLMonitor final : public FBaseMonitor
{
private:
    SDL_DisplayID m_displayID{ 0 };   //!< SDL3 display identifier for this monitor.

public:
    /// \brief Constructs an SDLMonitor for the given SDL display identifier.
    /// \param displayID A valid SDL_DisplayID obtained from SDL_GetDisplays() or SDL_GetPrimaryDisplay(). Passing 0
    /// leaves all properties at their FBaseMonitor defaults.
    explicit SDLMonitor(SDL_DisplayID displayID) noexcept;

    ~SDLMonitor() override = default;

public:
    /// \brief Returns whether this is the currently designated primary display.
    /// \return True if this monitor is the primary display, false otherwise. Returns false if the display ID is invalid
    /// or the SDL query fails.
    GP_NODISCARD virtual bool IsPrimary() const noexcept override;

    /// \brief Returns the current virtual-desktop rectangle occupied by this monitor.
    /// \return The monitor bounds in virtual-desktop coordinates, or a default rectangle if the display ID is invalid
    /// or the SDL query fails.
    GP_NODISCARD virtual FIntRectangle GetVirtualDesktopBounds() const noexcept override;

    /// \brief Returns the current usable work-area rectangle (display bounds minus OS-reserved regions).
    /// \return The work area bounds, or the virtual desktop bounds as a fallback if the SDL query fails.
    GP_NODISCARD virtual FIntRectangle GetWorkAreaBounds() const noexcept override;

    /// \brief Returns the top-left position of this monitor in virtual-desktop space.
    /// \return The virtual-desktop origin of the monitor as an FIntPoint2D, or (0, 0) if the display ID is invalid or
    /// the SDL query fails.
    GP_NODISCARD virtual FIntPoint2D GetVirtualDesktopPosition() const noexcept override;

    /// \brief Returns the X coordinate of the monitor's virtual-desktop origin.
    /// \return X offset in logical pixels, or 0 if the display ID is invalid or the SDL query fails.
    GP_NODISCARD virtual Int32 GetVirtualDesktopX() const noexcept override;

    /// \brief Returns the Y coordinate of the monitor's virtual-desktop origin.
    /// \return Y offset in logical pixels, or 0 if the display ID is invalid or the SDL query fails.
    GP_NODISCARD virtual Int32 GetVirtualDesktopY() const noexcept override;

    /// \brief Returns the center point of this monitor in virtual-desktop space.
    /// \return The center point calculated as (origin + size / 2) in logical pixels, or (0, 0) if the display ID is
    /// invalid.
    GP_NODISCARD virtual FIntPoint2D GetVirtualDesktopCenter() const noexcept override;

    /// \brief Returns the native pixel resolution of the panel from the current display mode.
    /// \return The native resolution as an FIntExtent2D, or a default value if the display ID is invalid or the SDL
    /// query fails.
    GP_NODISCARD virtual FIntExtent2D GetNativeResolution() const noexcept override;

    /// \brief Returns the native pixel width from the current display mode.
    /// \return Width in physical pixels, or 0 if the display ID is invalid or the SDL query fails.
    GP_NODISCARD virtual UInt32 GetNativeWidth() const noexcept override;

    /// \brief Returns the native pixel height from the current display mode.
    /// \return Height in physical pixels, or 0 if the display ID is invalid or the SDL query fails.
    GP_NODISCARD virtual UInt32 GetNativeHeight() const noexcept override;

    /// \brief Returns the aspect ratio of the current display mode (width / height).
    /// \return The aspect ratio as a floating-point value, or 0.0f if the height is zero or the display ID is invalid.
    GP_NODISCARD virtual Float32 GetAspectRatio() const noexcept override;

    /// \brief Returns the current horizontal logical DPI (content scale * 96 reference DPI).
    /// \note SDL3 no longer exposes raw DPI. This value is derived from the content scale factor.
    /// \return The horizontal logical DPI, or 96.0f if the display ID is invalid or the SDL query fails.
    GP_NODISCARD virtual Float32 GetDPIX() const noexcept override;

    /// \brief Returns the current vertical logical DPI.
    /// \return Vertical logical DPI, derived from the content scale factor. SDL3 does not provide raw DPI values.
    GP_NODISCARD virtual Float32 GetDPIY() const noexcept override;

    /// \brief Returns the physical horizontal DPI.
    /// \note SDL3 does not expose physical panel dimensions; this falls back to the logical DPI.
    /// \return Physical horizontal DPI, or logical DPI as a fallback.
    GP_NODISCARD virtual Float32 GetPhysicalDPIX() const noexcept override;

    /// \brief Returns the physical vertical DPI.
    /// \return Physical vertical DPI. Returns the logical DPI as a fallback since SDL3 does not provide physical size
    /// data.
    GP_NODISCARD virtual Float32 GetPhysicalDPIY() const noexcept override;

    /// \brief Returns the horizontal content scale factor as reported by SDL3.
    /// \return The content scale factor, or 1.0f if the query fails or the display ID is invalid.
    GP_NODISCARD virtual Float32 GetContentScaleX() const noexcept override;

    /// \brief Returns the vertical content scale factor (equal to the horizontal scale in SDL3).
    /// \return The vertical content scale factor, or 1.0f if the SDL query fails or the display ID is invalid.
    GP_NODISCARD virtual Float32 GetContentScaleY() const noexcept override;

    /// \brief Returns true if the content scale factor is greater than 1.0.
    /// \return True if the monitor is considered HiDPI / Retina; false otherwise.
    GP_NODISCARD virtual bool IsHighDPI() const noexcept override;

    /// \brief Returns the current refresh rate in Hz derived from the active display mode.
    /// \return Refresh rate in whole Hz, or 0 if the display ID is invalid or the SDL query fails.
    GP_NODISCARD virtual UInt32 GetRefreshRate() const noexcept override;

    /// \brief Returns the currently active display mode queried live from SDL3.
    /// \return The current FVideoMode, or the cached default if the SDL query fails.
    GP_NODISCARD virtual FVideoMode GetCurrentVideoMode() const noexcept override;

    /// \brief Returns the current display orientation as reported by SDL3.
    /// \return The display orientation, or EMonitorOrientation::Landscape if the display ID is invalid or the query
    /// fails.
    GP_NODISCARD virtual EMonitorOrientation GetOrientation() const noexcept override;

    /// \brief Returns true if the display is currently in portrait or portrait-flipped orientation.
    /// \return True if the orientation is portrait or portrait-flipped; false if landscape or if the display ID is
    /// invalid.
    GP_NODISCARD virtual bool IsPortrait() const noexcept override;

    /// \brief Returns true if HDR output is currently active on this monitor, as reported by SDL3.
    /// \return True if HDR is enabled, false if disabled or if the display ID is invalid / query fails.
    GP_NODISCARD virtual bool IsHDREnabled() const noexcept override;

    /// \brief Returns the platform-native display handle. On Windows this is an HMONITOR pointer; on Linux/Wayland a
    /// wl_output pointer; otherwise nullptr.
    /// \return A pointer to the native display handle, or nullptr if unavailable.
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept override;

    /// \brief Returns the SDL_DisplayID associated with this monitor, which can be used for direct SDL3 queries if
    /// needed.
    /// \return The SDL_DisplayID for this monitor, or 0 if the monitor was constructed with an invalid display ID.
    GP_NODISCARD SDL_DisplayID GetDisplayID() const noexcept;

    /// \brief Refreshes all dynamic properties by re-querying SDL3. This should be called in response to any
    /// SDL_DisplayEvent that may affect display properties (orientation change, content scale change, video mode
    /// change, etc.) to ensure that cached state is up to date for future queries.
    void Refresh() noexcept;

private:
    /// \brief Converts an SDL_DisplayMode to the GP FVideoMode representation.
    /// \param mode A populated SDL_DisplayMode structure. Must not be an uninitialized struct.
    /// \return The equivalent FVideoMode; refresh rate is rounded to the nearest integer Hz.
    GP_NODISCARD static FVideoMode ConvertDisplayMode(const SDL_DisplayMode& mode) noexcept;

    /// \brief Converts an SDL_DisplayOrientation value to the GP EMonitorOrientation enumeration.
    /// \param orientation The SDL orientation to convert.
    /// \return The corresponding EMonitorOrientation, defaulting to Landscape for unknown values.
    GP_NODISCARD static EMonitorOrientation ConvertOrientation(SDL_DisplayOrientation orientation) noexcept;

    /// \brief Queries the current display bounds from SDL3 and returns them as an FIntRectangle.
    /// \return The virtual-desktop bounds, or a zero-initialized rectangle if the SDL query fails.
    GP_NODISCARD FIntRectangle QueryBounds() const noexcept;

    /// \brief Queries the usable work-area bounds from SDL3.
    /// \return The work-area bounds, or a zero-initialized rectangle if the SDL query fails.
    GP_NODISCARD FIntRectangle QueryWorkArea() const noexcept;

    /// \brief Returns the content scale from SDL3, clamped to a minimum of 1.0.
    /// \return The content scale factor, or 1.0 if the query fails or the display ID is invalid.
    GP_NODISCARD Float32 QueryContentScale() const noexcept;

    /// \brief Queries the current display mode from SDL3.
    /// \return The active FVideoMode, or the cached base-class default if the SDL query fails.
    GP_NODISCARD FVideoMode QueryCurrentMode() const noexcept;

    /// \brief Fills HDR luminance metadata and infers color gamut from SDL3 display properties.
    /// \note maxLuminance is computed as (SDR white point) * (HDR headroom) when the SDL3 properties are available. HDR
    /// capability is inferred from the headroom value even when the OS HDR output toggle is currently disabled. Values
    /// remain at their defaults when SDL3 does not expose the relevant property data on the current platform.
    void QueryHDRMetadata() noexcept;

    /// \brief Infers VRR (variable refresh rate) support from the monitor's advertised refresh range.
    /// \note A gap of more than 10 Hz between the minimum and maximum refresh rates is treated as an indicator of
    /// Adaptive-Sync / FreeSync support. QueryPlatformInfo() may override the specific VRR standard to G-Sync or HDMI
    /// VRR when platform API data is available.
    void QueryVRRSupport() noexcept;

    /// \brief Detects touch-screen capability for built-in panels via the SDL3 touch device API.
    /// \note SDL3 does not bind individual touch devices to specific displays. This method conservatively enables the
    /// TouchScreen capability flag only on mobile and Apple platforms where the primary display is always the built-in
    /// touch panel.
    void QueryTouchSupport() noexcept;

    /// \brief Populates platform-specific display metadata not exposed by SDL3.
    /// \note On Windows this queries GDI for physical panel dimensions and the DisplayConfig API for model name,
    /// manufacturer identity, and connector type. Physical DPI is recomputed once the real panel dimensions are
    /// available. On other platforms this is a no-op pending the implementation of the respective OS-level queries.
    void QueryPlatformInfo() noexcept;
};

}   // namespace GP::HAL
