// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "display/DisplayEnums.hpp"
#include "display/DisplayTypes.hpp"
#include "event/system/DisplayEvent.hpp"

namespace gp::hal
{

/// @brief Platform-agnostic interface representing a physical or virtual display monitor.
/// @details Provides zero-cost shadow-state queries for monitor resolution, virtual desktop bounds,
///          usable work area, DPI scaling, HDR capabilities, and supported fullscreen modes.
class IDisplay
{
protected:
    /// @brief Protected copy and move operations prevent polymorphic slicing through base pointers
    ///        while allowing derived concrete classes to implement safe move semantics.
    constexpr IDisplay(const IDisplay&) noexcept = default;
    constexpr IDisplay& operator=(const IDisplay&) noexcept = default;
    constexpr IDisplay(IDisplay&&) noexcept = default;
    constexpr IDisplay& operator=(IDisplay&&) noexcept = default;

public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IDisplay() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IDisplay() noexcept = default;

public:
    /// @brief Retrieves the unique engine/backend identifier for this display.
    /// @return The 32-bit `DisplayId`, or `kInvalidDisplayId` (0) if uninitialized.
    [[nodiscard]] virtual DisplayId getId() const noexcept = 0;

    /// @brief Retrieves the native OS/backend display identifier (e.g., `SDL_DisplayID`).
    /// @return The native 32-bit display identifier.
    [[nodiscard]] virtual DisplayId getNativeId() const noexcept = 0;

    /// @brief Retrieves the human-readable UTF-8 model name of the monitor.
    /// @return StringView referencing the cached monitor name.
    [[nodiscard]] virtual StringView getName() const noexcept = 0;

    /// @brief Checks whether this display object represents a valid, connected monitor.
    /// @return True if valid and connected, false otherwise.
    [[nodiscard]] virtual bool isValid() const noexcept = 0;

    /// @brief Checks whether this monitor is currently connected to the system.
    /// @return True if connected, false if unplugged.
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    /// @brief Checks whether this display is designated by the OS as the primary desktop monitor.
    /// @return True if primary, false otherwise.
    [[nodiscard]] virtual bool isPrimary() const noexcept = 0;

    /// @brief Checks whether High Dynamic Range (HDR) output is currently enabled on this display.
    /// @return True if HDR is active, false otherwise.
    [[nodiscard]] virtual bool isHDREnabled() const noexcept = 0;

    /// @brief Retrieves the capability bitmask for this display.
    /// @return `DisplayFlags` bitmask.
    [[nodiscard]] virtual DisplayFlags getFlags() const noexcept = 0;

    /// @brief Retrieves the current physical rotation of the display panel.
    /// @return `DisplayOrientation` enumerator.
    [[nodiscard]] virtual DisplayOrientation getOrientation() const noexcept = 0;

    /// @brief Retrieves the full virtual desktop rectangle occupied by this monitor.
    /// @return `DisplayRect` containing the virtual desktop `(x, y)` origin and `(width, height)` pixel extent.
    [[nodiscard]] virtual DisplayRect getBounds() const noexcept = 0;

    /// @brief Retrieves the usable desktop work area excluding OS taskbars, docks, and menu bars.
    /// @return `DisplayRect` representing the safe placement area for windowed applications.
    [[nodiscard]] virtual DisplayRect getWorkArea() const noexcept = 0;

    /// @brief Retrieves the active display mode currently driving the monitor.
    /// @return Const reference to the current `DisplayMode`.
    [[nodiscard]] virtual const DisplayMode& getCurrentMode() const noexcept = 0;

    /// @brief Retrieves the native desktop display mode configured in the OS prior to any exclusive fullscreen switch.
    /// @return Const reference to the desktop `DisplayMode`.
    [[nodiscard]] virtual const DisplayMode& getDesktopMode() const noexcept = 0;

    /// @brief Retrieves the OS user-interface content scale factor for this display (1.0f = 100% scaling).
    /// @return Content scale multiplier.
    [[nodiscard]] virtual Float32 getContentScale() const noexcept = 0;

    /// @brief Retrieves the HDR calibration metadata (SDR white point and HDR luminance headroom).
    /// @return Const reference to the cached `DisplayHDRMetadata`.
    [[nodiscard]] virtual const DisplayHDRMetadata& getHDRMetadata() const noexcept = 0;

    /// @brief Returns the number of supported exclusive fullscreen video modes cached for this monitor.
    /// @return Number of available `DisplayMode` entries.
    [[nodiscard]] virtual gp::UInt32 getSupportedModeCount() const noexcept = 0;

    /// @brief Retrieves a supported fullscreen display mode by index.
    /// @param[in] index Zero-based mode index in the range `[0, getSupportedModeCount())`.
    /// @return Pointer to the `DisplayMode`, or `nullptr` if `index` is out of bounds.
    [[nodiscard]] virtual const DisplayMode* getSupportedMode(gp::UInt32 index) const noexcept = 0;

    /// @brief Searches the cached mode table for the closest hardware mode matching the requested resolution and
    /// refresh rate.
    /// @param[in] targetResolution Desired width and height in pixels.
    /// @param[in] targetRefreshRate Desired refresh rate in Hz (0.0f = prefer highest available refresh rate).
    /// @param[out] outMode Receives the best-matching `DisplayMode` if found.
    /// @return True if a matching mode was found, false if the mode table is empty.
    [[nodiscard]] virtual bool findClosestMode(
        const Extent2D& targetResolution, Float32 targetRefreshRate, DisplayMode& outMode
    ) const noexcept = 0;

    /// @brief Queries the underlying OS backend to refresh all cached display metrics and video modes.
    /// @note Must only be called from the main OS thread.
    virtual void refresh() noexcept = 0;

    /// @brief Updates the display's internal shadow state from a decoded HAL `DisplayEvent`.
    /// @note Called on the Game Thread when draining the lock-free `SPSCByteRingBuffer`.
    /// @param[in] ev The decoded display event packet.
    virtual void onDisplayEvent(const event::DisplayEvent& ev) noexcept = 0;
};

}   // namespace gp::hal
