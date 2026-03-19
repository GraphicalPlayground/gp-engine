// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Sequential/Array.hpp"
#include "Display/BaseDisplay.hpp"
#include <SDL3/SDL_video.h>

namespace GP
{

/// @brief SDL3 backend implementation of IDisplay.
///
/// Wraps a single \c SDL_DisplayID and caches all display properties at construction time. Cached state can be
/// refreshed on demand via Refresh(), which should be called by the SDL3 window system whenever SDL fires an \c
/// SDL_EVENT_DISPLAY_* notification.
///
/// All property queries are O(1) reads from cached members, SDL3 is never called on the hot path after initialisation.
/// Refresh() is the only point of SDL3 re-entry.
///
/// @note SDL3 does not expose EDID physical dimensions or raw per-axis DPI. GetPhysicalSizeMillimeters() and
///       GetRawDPI() therefore always return {0, 0}. GetDPI() returns an effective logical DPI derived from
///       GetContentScale() and the platform baseline (96 DPI on Windows / Linux, 72 DPI on Apple).
/// @note SDL3 does not expose the native OS display handle (HMONITOR, CGDirectDisplayID) through its public API.
///       GetNativeHandle() returns the SDL_DisplayID reinterpreted as a void*; this is sufficient for
///       IWindowSystem::FindDisplayByNativeHandle() when the same reinterpretation is applied consistently by the SDL3
///       window system backend.
/// @see IDisplay, FBaseDisplay, IWindowSystem
class FSDL3Display final : public FBaseDisplay
{
public:
    using FDisplayId = UInt32;   //<! Alias for SDL3's display identifier type (uint32_t).

public:
    /// @brief Effective logical DPI baseline on Windows and Linux (VESA / Windows standard).
    static constexpr Float32 kBaselineDPIDesktop = 96.0f;

    /// @brief Effective logical DPI baseline on Apple platforms (historical Mac resolution).
    static constexpr Float32 kBaselineDPIApple = 72.0f;

    /// @brief Minimum HDR headroom ratio above which the display is considered HDR-capable.
    /// A headroom of exactly 1.0 means the display cannot exceed the SDR white point.
    /// Any value measurably above 1.0 indicates extended dynamic range capability.
    static constexpr Float32 kHDRHeadroomThreshold = 1.05f;

private:
    // Identity
    FDisplayId m_displayId{ 0 };
    const char* m_name{ nullptr };   //<! Owned by SDL; valid until SDL_Quit().
    bool m_isPrimary{ false };

    // Screen geometry
    FIntPoint2D m_position{ 0, 0 };
    FIntExtent2D m_size{ 0, 0 };
    FIntPoint2D m_workAreaPosition{ 0, 0 };
    FIntExtent2D m_workAreaSize{ 0, 0 };

    // Display modes
    FDisplayMode m_currentMode{};
    TArray<FDisplayMode> m_supportedModes{};

    // Physical properties
    Float32 m_dpi{ kBaselineDPIDesktop };
    Float32 m_contentScale{ 1.0f };
    EDisplayOrientation m_orientation{ EDisplayOrientation::Unknown };

    // Capabilities
    EDisplayColorSpace m_colorSpace{ EDisplayColorSpace::sRGB };
    bool m_supportsHDR{ false };
    bool m_hdrEnabled{ false };
    Float32 m_peakLuminance{ 0.0f };
    Float32 m_minLuminance{ 0.0f };

public:
    /// @brief Constructs the display object and immediately populates all cached state via SDL3.
    /// @param displayId A valid SDL_DisplayID returned by SDL_GetDisplays().
    explicit FSDL3Display(FDisplayId displayId) noexcept;

    ~FSDL3Display() override = default;

public:
    ///
    /// @section Methods - SDL3-specific
    ///

    /// @brief Repopulates all cached state by re-querying SDL3.
    ///
    /// Must be called by the owning SDL3 window system backend whenever any of these SDL3
    /// events are received for this display:
    ///   - SDL_EVENT_DISPLAY_ORIENTATION
    ///   - SDL_EVENT_DISPLAY_ADDED / SDL_EVENT_DISPLAY_REMOVED
    ///   - SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED
    ///   - SDL_EVENT_DISPLAY_MOVED
    ///   - SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED / SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED
    ///   - SDL_EVENT_DISPLAY_HDR_STATE_CHANGED
    ///
    /// @warning After this call, all previously obtained FStringView and TArrayView references into this display object
    ///          are invalidated.
    void Refresh() noexcept;

    /// @brief Returns the underlying SDL3 display identifier.
    /// @return The SDL_DisplayID this object wraps.
    GP_NODISCARD FDisplayId GetSDLDisplayId() const noexcept;

    ///
    /// @section Identity
    ///

    GP_NODISCARD virtual SizeT GetId() const noexcept override;
    GP_NODISCARD virtual FStringView GetName() const noexcept override;
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept override;
    GP_NODISCARD virtual bool IsPrimary() const noexcept override;

    ///
    /// @section Screen Geometry
    ///

    GP_NODISCARD virtual FIntPoint2D GetPosition() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetSize() const noexcept override;
    GP_NODISCARD virtual FIntPoint2D GetWorkAreaPosition() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetWorkAreaSize() const noexcept override;

    ///
    /// @section Display Modes
    ///

    GP_NODISCARD virtual FDisplayMode GetCurrentMode() const noexcept override;
    GP_NODISCARD virtual TArrayView<const FDisplayMode> GetSupportedModes() const noexcept override;
    GP_NODISCARD virtual bool SupportsMode(const FDisplayMode& mode) const noexcept override;

    ///
    /// @section Physical Properties
    ///

    GP_NODISCARD virtual Float32 GetDPI() const noexcept override;
    // GP_NODISCARD virtual FIntExtent2D GetRawDPI() const noexcept override;
    GP_NODISCARD virtual Float32 GetContentScale() const noexcept override;
    // GP_NODISCARD virtual FIntExtent2D GetPhysicalSizeMillimeters() const noexcept override;
    GP_NODISCARD virtual EDisplayOrientation GetOrientation() const noexcept override;

    ///
    /// @section Capabilities
    ///

    GP_NODISCARD virtual EDisplayColorSpace GetColorSpace() const noexcept override;
    GP_NODISCARD virtual bool SupportsHDR() const noexcept override;
    GP_NODISCARD virtual bool IsHDREnabled() const noexcept override;
    GP_NODISCARD virtual Float32 GetPeakLuminance() const noexcept override;
    GP_NODISCARD virtual Float32 GetMinLuminance() const noexcept override;

private:
    ///
    /// @section Query Helpers
    ///

    void QueryIdentity() noexcept;
    void QueryGeometry() noexcept;
    void QueryModes() noexcept;
    void QueryPhysicalProperties() noexcept;
    void QueryCapabilities() noexcept;
};

}   // namespace GP
