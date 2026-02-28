// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Events/EventsForward.hpp"
#include "HALBuild.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"
#include "Monitor/MonitorEnums.hpp"

namespace GP::HAL
{

/// \brief Forward declaration of the IMonitor interface.
class IMonitor;

}   // namespace GP::HAL

namespace GP
{

/// \brief Event definitions for HAL::IMonitor.
/// Each nested struct represents a distinct monitor event. Clients subscribe via TMulticastDelegate::Add*() and
/// retain an FDelegateHandle or FScopedDelegateHandle to control the subscription lifetime.
template <>
struct FEvents<HAL::IMonitor>
{
public:
    /// \brief Fired when the monitor is connected to the system and enumerated by the OS.
    struct Connected
    {
        HAL::IMonitor* monitor{ nullptr };   //<! The newly connected monitor.
    };

    /// \brief Fired when the monitor is physically disconnected or disabled by the OS.
    /// \warning Any pointer or handle to the monitor must be considered invalid after this event is dispatched.
    struct Disconnected
    {
        HAL::IMonitor* monitor{ nullptr };   //<! The monitor that was disconnected.
    };

    /// \brief Fired when the active video mode changes (resolution, refresh rate, or bit depth).
    struct VideoModeChanged
    {
        UInt32 previousWidth{ 0 };           //<! Previous horizontal resolution, in physical pixels.
        UInt32 previousHeight{ 0 };          //<! Previous vertical resolution, in physical pixels.
        UInt32 previousRefreshRateHz{ 0 };   //<! Previous refresh rate, in Hz.
        UInt32 newWidth{ 0 };                //<! New horizontal resolution, in physical pixels.
        UInt32 newHeight{ 0 };               //<! New vertical resolution, in physical pixels.
        UInt32 newRefreshRateHz{ 0 };        //<! New refresh rate, in Hz.
    };

    /// \brief Fired when the monitor's DPI or content scale factor changes at runtime (e.g., due to OS display
    /// settings being modified while the application is running).
    struct DPIChanged
    {
        Float32 previousDPIX{ 96.0f };   //<! Previous horizontal DPI.
        Float32 previousDPIY{ 96.0f };   //<! Previous vertical DPI.
        Float32 newDPIX{ 96.0f };        //<! New horizontal DPI.
        Float32 newDPIY{ 96.0f };        //<! New vertical DPI.
    };

    /// \brief Fired when the monitor's orientation changes (e.g., the user physically rotates the panel).
    struct OrientationChanged
    {
        HAL::EMonitorOrientation previousOrientation = HAL::EMonitorOrientation::Landscape;
        HAL::EMonitorOrientation newOrientation = HAL::EMonitorOrientation::Landscape;
    };

    /// \brief Fired when the OS-designated primary monitor changes.
    /// \note This may occur after another monitor is connected, disconnected, or promoted via OS display settings.
    struct PrimaryChanged
    {
        HAL::IMonitor* previousPrimary{ nullptr };   //<! The monitor that was previously primary.
        HAL::IMonitor* newPrimary{ nullptr };        //<! The monitor that is now primary.
    };

    /// \brief Fired when the monitor's HDR state is toggled on or off (e.g., user enables HDR in OS display settings).
    struct HDRStateChanged
    {
        bool hdrEnabled{ false };   //<! True if HDR is now active, false if reverted to SDR.
    };

    /// \brief Fired when the monitor's brightness or backlight level changes programmatically or via OS policy.
    struct BrightnessChanged
    {
        Float32 previousBrightness{ 1.0f };   //<! Previous brightness level in [0.0, 1.0].
        Float32 newBrightness{ 1.0f };        //<! New brightness level in [0.0, 1.0].
    };

    /// \brief Fired when the monitor enters a low-power / sleep state.
    struct Sleeping
    {};

    /// \brief Fired when the monitor wakes from a low-power / sleep state.
    struct WokeUp
    {};

    /// \brief Fired when the monitor's gamma ramp or color profile is modified.
    struct ColorProfileChanged
    {};
};

namespace HAL
{

/// \brief Convenience alias for the monitor event bundle.
using FMonitorEvents = FEvents<HAL::IMonitor>;

}   // namespace HAL

}   // namespace GP
