// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/ContainerFwd.hpp"
#include "Container/Views/ArrayView.hpp"
#include "Container/Views/StringView.hpp"
#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Display/DisplayEnums.hpp"
#include "Display/DisplayMode.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"

namespace GP
{

/// @brief Read-only abstraction for a single physical display (monitor / screen).
///
/// An IDisplay instance is a snapshot of the display's configuration as it was known at the time the IWindowSystem last
/// refreshed its display list. Clients should call IWindowSystem::RefreshDisplayList() and re-query any cached pointers
/// after the OS fires a display-change notification (e.g. monitor hot-plug, DPI change, resolution change).
///
/// All returned geometry values are in the OS virtual screen coordinate space (logical pixels) unless otherwise noted.
/// The physical pixel count may differ on high-DPI displays; use GetContentScale() to convert between logical and
/// physical pixels.
///
/// @note Implementations are immutable by design - all methods are const. The window system owns the lifetime of every
///       IDisplay; clients should hold only non-owning TSharedPtr<const IDisplay> references and treat them as
///       invalidated after IWindowSystem::RefreshDisplayList() returns.
/// @see IWindowSystem, FDisplayMode, EDisplayColorSpace, EDisplayOrientation
class IDisplay
{
public:
    virtual ~IDisplay() = default;

public:
    ///
    /// @section Identity
    ///

    /// @brief Returns the engine-assigned unique identifier for this display.
    ///
    /// The ID is stable for the lifetime of the IWindowSystem instance. It is invalidated after
    /// IWindowSystem::RefreshDisplayList(). Use this value with IWindowSystem::FindDisplayById() to re-acquire a fresh
    /// handle after a refresh.
    ///
    /// @return Unique display identifier, guaranteed non-zero for any live display.
    /// @see IWindowSystem::FindDisplayById()
    GP_NODISCARD virtual SizeT GetId() const noexcept = 0;

    /// @brief Returns the human-readable name of the display as reported by the OS.
    /// @return A string view valid for the lifetime of this IDisplay instance.
    GP_NODISCARD virtual FStringView GetName() const noexcept = 0;

    /// @brief Returns the platform-native handle for this display.
    ///
    /// The underlying type is platform-specific:
    ///   - Windows  : HMONITOR
    ///   - macOS    : CGDirectDisplayID (cast to void*)
    ///   - Linux    : RROutput (XRandR) or wl_output* (Wayland)
    ///   - Android  : ANativeWindow* (primary display only)
    ///
    /// @return Opaque native handle. Never nullptr for a live display.
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept = 0;

    /// @brief Returns true if this is the OS-designated primary (main) display.
    ///
    /// The primary display is the one whose top-left corner is anchored to the virtual screen origin (0, 0) on most
    /// platforms.
    ///
    /// @return True if this display is the system primary.
    GP_NODISCARD virtual bool IsPrimary() const noexcept = 0;

    ///
    /// @section Screen Geometry
    ///

    /// @brief Returns the position of the display's top-left corner in virtual screen coordinates.
    ///
    /// Virtual screen coordinates are the OS logical pixel space that spans all monitors. On most platforms the primary
    /// display's origin is (0, 0), and secondary monitors are positioned relative to that origin.
    ///
    /// @return Top-left position in logical pixels within the virtual screen.
    GP_NODISCARD virtual FIntPoint2D GetPosition() const noexcept = 0;

    /// @brief Returns the full resolution of the display in logical pixels.
    ///
    /// This is the total scanout area reported by the OS, including space that may be reserved by the OS shell
    /// (taskbar, dock, menu bar). To obtain the usable content area, use GetWorkAreaSize().
    ///
    /// @note On high-DPI displays GetSize() returns logical pixels. Multiply by GetContentScale() to obtain the
    ///       physical (framebuffer) pixel count.
    /// @return Full display resolution in logical pixels.
    /// @see GetWorkAreaSize(), GetContentScale()
    GP_NODISCARD virtual FIntExtent2D GetSize() const noexcept = 0;

    /// @brief Returns the position of the usable work area in virtual screen coordinates.
    ///
    /// The work area excludes regions reserved by the OS shell (Windows taskbar, macOS menu bar and Dock, Linux
    /// panels). It defines the rectangular region inside which normal application windows are expected to be placed.
    ///
    /// @return Top-left corner of the work area in logical pixels.
    GP_NODISCARD virtual FIntPoint2D GetWorkAreaPosition() const noexcept = 0;

    /// @brief Returns the size of the usable work area in logical pixels.
    ///
    /// The work area excludes regions reserved by the OS shell. It is always less than or equal to GetSize().
    ///
    /// @return Work area dimensions in logical pixels.
    GP_NODISCARD virtual FIntExtent2D GetWorkAreaSize() const noexcept = 0;

    ///
    /// @section Display Modes
    ///

    /// @brief Returns the display mode that is currently active.
    ///
    /// For windowed applications this is typically the desktop resolution and refresh rate. For exclusive-fullscreen
    /// applications the active mode may differ from the desktop mode if a mode change was requested via the windowing
    /// layer.
    ///
    /// @return The currently active FDisplayMode. Always valid (IsValid() == true).
    GP_NODISCARD virtual FDisplayMode GetCurrentMode() const noexcept = 0;

    /// @brief Returns a view over all display modes supported by this output.
    ///
    /// The list is sorted in ascending order by total pixel count, then refresh rate, then bits per pixel (consistent
    /// with FDisplayMode::operator<=>). The best mode is therefore always the last element in the view.
    ///
    /// @note The returned view is valid for the lifetime of this IDisplay instance. It is invalidated by
    ///       IWindowSystem::RefreshDisplayList().
    /// @return A non-owning view over the list of supported modes. May be empty on platforms that do not expose mode
    ///         enumeration (e.g. Android, some Wayland compositors).
    /// @see SupportsMode()
    GP_NODISCARD virtual TArrayView<const FDisplayMode> GetSupportedModes() const noexcept = 0;

    /// @brief Returns true if the supplied mode is present in GetSupportedModes().
    ///
    /// Equality is defined by FDisplayMode::operator== (all four fields must match).
    ///
    /// @param mode The candidate mode to test.
    /// @return True if the mode is supported by this display.
    /// @see GetSupportedModes()
    GP_NODISCARD virtual bool SupportsMode(const FDisplayMode& mode) const noexcept = 0;

    ///
    /// @section Physical Properties
    ///

    /// @brief Returns the effective dots-per-inch of the display, accounting for OS scaling.
    ///
    /// This is the logical DPI seen by applications: the product of the raw physical DPI and the OS content scale
    /// factor. At 100% scaling on a 96-DPI monitor this returns 96.0f. At 200% scaling (Retina/HiDPI) on the same
    /// monitor it returns 192.0f.
    ///
    /// Use this value for UI layout calculations. For raw hardware DPI, use GetRawDPI().
    ///
    /// @return Effective DPI as a positive floating-point value. Returns 0.0f if unavailable.
    /// @see GetRawDPI(), GetContentScale()
    GP_NODISCARD virtual Float32 GetDPI() const noexcept = 0;

    /// @brief Returns the raw physical dots-per-inch as measured from EDID data.
    ///
    /// Horizontal (X) and vertical (Y) values are returned separately because physically non-square pixels exist in
    /// some embedded and industrial displays.
    ///
    /// @note Not all platforms expose this; returns {0, 0} if unavailable. Use GetDPI() for a reliable per-display
    ///       density that works on all platforms.
    /// @return Raw horizontal and vertical DPI derived from the physical panel dimensions.
    /// @see GetDPI()
    GP_NODISCARD virtual FIntExtent2D GetRawDPI() const noexcept = 0;

    /// @brief Returns the OS content scale (DPI scale) factor for this display.
    ///
    /// The scale factor is the ratio of physical pixels to logical pixels:
    ///   - 1.0f = 100% scaling  (96 DPI baseline on Windows, 72 DPI on macOS)
    ///   - 1.5f = 150% scaling  (144 DPI effective on a 96-DPI baseline)
    ///   - 2.0f = 200% scaling  (Retina / HiDPI; 1 logical pixel = 2x2 physical pixels)
    ///
    /// Multiply any logical pixel dimension by this factor to obtain the physical pixel count required for the
    /// framebuffer (swap-chain) or a high-DPI render target.
    ///
    /// @note Subscribe to FWindowDPIScaleChangedEvent to receive notification when this value changes at runtime (e.g.
    ///       user drags window to a different monitor).
    /// @return Content scale factor (>= 1.0f). Returns 1.0f if the platform does not report it.
    /// @see GetDPI()
    GP_NODISCARD virtual Float32 GetContentScale() const noexcept = 0;

    /// @brief Returns the physical dimensions of the display panel in millimetres.
    ///
    /// Derived from EDID data where available. The width member is the horizontal extent and the height member is the
    /// vertical extent. Both values reflect the physical panel area regardless of the current OS rotation setting.
    ///
    /// @note Returns {0, 0} when the platform does not expose physical panel dimensions (common for virtual/remote
    ///       displays and some Wayland compositors).
    /// @return Physical panel width (x) and height (y) in millimetres.
    GP_NODISCARD virtual FIntExtent2D GetPhysicalSizeMillimeters() const noexcept = 0;

    /// @brief Returns the current physical orientation of the display panel.
    ///
    /// Reflects the OS-reported rotation state. The returned value changes dynamically when the user rotates a mobile
    /// device or adjusts the display rotation in the OS settings.
    ///
    /// @return Current EDisplayOrientation value. Returns EDisplayOrientation::Unknown if the
    ///         platform does not report orientation.
    GP_NODISCARD virtual EDisplayOrientation GetOrientation() const noexcept = 0;

    ///
    /// @section Capabilities
    ///

    /// @brief Returns the colour gamut and transfer function this display is capable of rendering.
    ///
    /// Use this to determine which RHI swap-chain surface format and colour space (GP::EColorSpace) to request. A
    /// display that reports HDR10_PQ capability should be paired with a swap chain using EColorSpace::HDR10_ST2084 and
    /// an EFormat of RGB10A2_UNORM or RGBA16_FLOAT.
    ///
    /// @note  This reflects the display hardware capability, not the current OS configuration. The user may have
    ///        HDR-capable hardware but HDR disabled in the OS settings. Check IsHDREnabled() in addition to this value
    ///        before enabling HDR rendering.
    /// @return The highest colour space tier the physical panel supports.
    /// @see IsHDREnabled(), SupportsHDR()
    GP_NODISCARD virtual EDisplayColorSpace GetColorSpace() const noexcept = 0;

    /// @brief Returns true if the display hardware supports HDR output (any HDR tier).
    ///
    /// Hardware HDR capability means the display can reproduce a luminance range beyond the standard 0–100 cd/m² SDR
    /// window. This is a necessary but not sufficient condition for HDR rendering; the OS HDR toggle must also be
    /// active.
    ///
    /// @note Equivalent to:
    ///       @code
    ///       GetColorSpace() >= EDisplayColorSpace::HDR10_PQ
    ///       @endcode
    /// @return True if the panel is capable of HDR output.
    /// @see GetColorSpace(), IsHDREnabled()
    GP_NODISCARD virtual bool SupportsHDR() const noexcept = 0;

    /// @brief Returns true if HDR output is currently enabled in the OS display settings.
    ///
    /// A display may be HDR-capable (SupportsHDR() == true) but have HDR output toggled off by the user in the OS
    /// control panel. When this returns false the swap chain should use SDR formats regardless of what GetColorSpace()
    /// reports.
    ///
    /// @note Always returns false on platforms that do not expose an OS-level HDR toggle (e.g. Linux/Wayland prior to
    ///       colour management protocol support).
    /// @return True if HDR output is active on this display right now.
    /// @see GetColorSpace(), SupportsHDR()
    GP_NODISCARD virtual bool IsHDREnabled() const noexcept = 0;

    /// @brief Returns the peak brightness (maximum luminance) of the display in nits (cd/m²).
    ///
    /// For SDR displays this is typically in the range [80, 500] nits. For HDR10 displays this is typically in the
    /// range [600, 10000] nits.
    ///
    /// Use this value to parameterise tone-mapping operators and to construct the HDR metadata (MaxContentLightLevel /
    /// MaxFrameAverageLightLevel) required by DXGI or Vulkan HDR swap chains.
    ///
    /// @note Returns 0.0f if the platform does not expose luminance metadata.
    ///       On Windows this is populated from DXGI_OUTPUT_DESC1::MaxLuminance.
    ///       On macOS it is available via NSScreen.maximumExtendedDynamicRangeColorComponentValue.
    /// @return Peak luminance in nits, or 0.0f if unavailable.
    /// @see GetMinLuminance()
    GP_NODISCARD virtual Float32 GetPeakLuminance() const noexcept = 0;

    /// @brief Returns the minimum displayable brightness of the display in nits (cd/m²).
    ///
    /// Represents the deepest black the panel can produce. Typical values are 0.0001–0.005 nits for OLED/MiniLED panels
    /// and 0.1–0.4 nits for standard IPS/VA displays.
    ///
    /// Used together with GetPeakLuminance() to define the full luminance range for physically-based tone mapping (e.g.
    /// ACES, Reinhard with scene-referred HDR).
    ///
    /// @note Returns 0.0f if the platform does not expose luminance metadata.
    /// @return Minimum luminance in nits, or 0.0f if unavailable.
    /// @see GetPeakLuminance()
    GP_NODISCARD virtual Float32 GetMinLuminance() const noexcept = 0;
};

}   // namespace GP
