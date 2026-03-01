// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "HALBuild.hpp"
#include "Templates/Enums.hpp"

namespace GP::HAL
{

/// \brief Describes the physical connector type used to attach the monitor to the GPU.
enum class EMonitorConnectorType : UInt8
{
    Unknown = 0,   //<! Connector type could not be determined.
    HDMI,          //<! High-Definition Multimedia Interface.
    DisplayPort,   //<! DisplayPort (including Mini-DP).
    DVI,           //<! Digital Visual Interface.
    VGA,           //<! Video Graphics Array (analog).
    Thunderbolt,   //<! Thunderbolt / USB4 DisplayPort Alt Mode.
    USBC,          //<! Generic USB-C DisplayPort Alternate Mode.
    Embedded,      //<! Built-in / laptop panel (eDP, LVDS).
    Virtual,       //<! Software or virtual display (e.g., headless, RDP, emulator).
};

/// \brief Describes the panel technology of the display.
enum class EMonitorPanelType : UInt8
{
    Unknown = 0,   //<! Panel type could not be determined.
    IPS,           //<! In-Plane Switching — accurate color, wide viewing angle.
    TN,            //<! Twisted Nematic — fast response, narrower color gamut.
    VA,            //<! Vertical Alignment — high contrast, moderate response.
    OLED,          //<! Organic LED — perfect blacks, risk of burn-in.
    MicroLED,      //<! Micro-LED — high brightness, no burn-in.
    QLED,          //<! Quantum-dot LED backlit LCD.
    CRT,           //<! Legacy cathode-ray tube.
};

/// \brief Describes the color gamut / color space the monitor natively covers.
enum class EMonitorColorGamut : UInt8
{
    Unknown = 0,   //<! Gamut information unavailable.
    sRGB,          //<! Standard sRGB (IEC 61966-2-1), ~67 % of DCI-P3.
    AdobeRGB,      //<! Adobe RGB (1998), ~50 % larger than sRGB.
    DCIP3,         //<! DCI-P3, common in HDR consumer displays.
    BT2020,        //<! ITU-R BT.2020 / Rec. 2020, ultra-wide gamut for HDR10/HLG.
    BT2100,        //<! ITU-R BT.2100, superset of BT.2020 with PQ/HLG transfer.
    DisplayP3,     //<! Display P3 (Apple), D65 white point variant of DCI-P3.
};

/// \brief Describes the HDR transfer function / standard supported by the monitor.
enum class EMonitorHDRFormat : UInt8
{
    None = 0,      //<! No HDR support — standard dynamic range (SDR) only.
    HDR10,         //<! HDR10 static metadata, PQ (ST.2084) transfer function.
    HLG,           //<! Hybrid Log-Gamma — broadcast HDR standard.
    DolbyVision,   //<! Dolby Vision — dynamic metadata HDR.
    HDR400,        //<! VESA DisplayHDR 400 — entry-level HDR certification.
    HDR600,        //<! VESA DisplayHDR 600.
    HDR1000,       //<! VESA DisplayHDR 1000.
    HDR1400,       //<! VESA DisplayHDR 1400.
};

/// \brief Orientation of the physical display panel.
enum class EMonitorOrientation : UInt8
{
    Landscape = 0,      //<! Normal horizontal orientation.
    Portrait,           //<! Rotated 90 degrees clockwise.
    LandscapeFlipped,   //<! Rotated 180 degrees.
    PortraitFlipped,    //<! Rotated 270 degrees clockwise (90 counter-clockwise).
};

/// \brief Variable refresh rate technology supported by the monitor.
enum class EMonitorVRRType : UInt8
{
    None = 0,    //<! No variable refresh rate support.
    FreeSync,    //<! AMD FreeSync / VESA Adaptive-Sync.
    GSync,       //<! NVIDIA G-Sync (hardware module).
    GSyncComp,   //<! NVIDIA G-Sync Compatible (Adaptive-Sync certified by NVIDIA).
    HDMI_VRR,    //<! HDMI 2.1 native VRR.
};

/// \brief Bit-flag capabilities reported by the monitor driver.
enum class EMonitorCapabilityFlags : UInt32
{
    None = 0,
    HDR = 1 << 0,               //<! Any form of HDR is supported.
    VRR = 1 << 1,               //<! Variable refresh rate is supported.
    HighDPI = 1 << 2,           //<! DPI scale factor greater than 1.0.
    TouchScreen = 1 << 3,       //<! Monitor has integrated touch input.
    BuiltInSpeakers = 1 << 4,   //<! HDMI/DP audio output available.
    PrivacyScreen = 1 << 5,     //<! Electronic privacy filter present.
    LocalDimming = 1 << 6,      //<! Full-array local dimming (FALD) support.
    WideColorGamut = 1 << 7,    //<! Covers more than sRGB (e.g., DCI-P3 or wider).
    SyncWithGPU = 1 << 8,       //<! Explicit GPU sync / frame-pacing support.
    EyeTracking = 1 << 9,       //<! Integrated eye-tracking hardware.
    PrimaryDisplay = 1 << 10,   //<! This is the OS-designated primary monitor.
};

}   // namespace GP::HAL

GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::HAL::EMonitorCapabilityFlags);

GP_ENUM_TO_STRING(
    GP::HAL::EMonitorConnectorType,
    { GP::HAL::EMonitorConnectorType::Unknown, "Unknown" },
    { GP::HAL::EMonitorConnectorType::HDMI, "HDMI" },
    { GP::HAL::EMonitorConnectorType::DisplayPort, "DisplayPort" },
    { GP::HAL::EMonitorConnectorType::DVI, "DVI" },
    { GP::HAL::EMonitorConnectorType::VGA, "VGA" },
    { GP::HAL::EMonitorConnectorType::Thunderbolt, "Thunderbolt" },
    { GP::HAL::EMonitorConnectorType::USBC, "USB-C" },
    { GP::HAL::EMonitorConnectorType::Embedded, "Embedded" },
    { GP::HAL::EMonitorConnectorType::Virtual, "Virtual" }
);

GP_ENUM_TO_STRING(
    GP::HAL::EMonitorPanelType,
    { GP::HAL::EMonitorPanelType::Unknown, "Unknown" },
    { GP::HAL::EMonitorPanelType::IPS, "IPS" },
    { GP::HAL::EMonitorPanelType::TN, "TN" },
    { GP::HAL::EMonitorPanelType::VA, "VA" },
    { GP::HAL::EMonitorPanelType::OLED, "OLED" },
    { GP::HAL::EMonitorPanelType::MicroLED, "MicroLED" },
    { GP::HAL::EMonitorPanelType::QLED, "QLED" },
    { GP::HAL::EMonitorPanelType::CRT, "CRT" }
);

GP_ENUM_TO_STRING(
    GP::HAL::EMonitorColorGamut,
    { GP::HAL::EMonitorColorGamut::Unknown, "Unknown" },
    { GP::HAL::EMonitorColorGamut::sRGB, "sRGB" },
    { GP::HAL::EMonitorColorGamut::AdobeRGB, "Adobe RGB" },
    { GP::HAL::EMonitorColorGamut::DCIP3, "DCI-P3" },
    { GP::HAL::EMonitorColorGamut::BT2020, "BT.2020" },
    { GP::HAL::EMonitorColorGamut::BT2100, "BT.2100" },
    { GP::HAL::EMonitorColorGamut::DisplayP3, "Display P3" }
);

GP_ENUM_TO_STRING(
    GP::HAL::EMonitorHDRFormat,
    { GP::HAL::EMonitorHDRFormat::None, "None" },
    { GP::HAL::EMonitorHDRFormat::HDR10, "HDR10" },
    { GP::HAL::EMonitorHDRFormat::HLG, "HLG" },
    { GP::HAL::EMonitorHDRFormat::DolbyVision, "Dolby Vision" },
    { GP::HAL::EMonitorHDRFormat::HDR400, "VESA DisplayHDR 400" },
    { GP::HAL::EMonitorHDRFormat::HDR600, "VESA DisplayHDR 600" },
    { GP::HAL::EMonitorHDRFormat::HDR1000, "VESA DisplayHDR 1000" },
    { GP::HAL::EMonitorHDRFormat::HDR1400, "VESA DisplayHDR 1400" }
);

GP_ENUM_TO_STRING(
    GP::HAL::EMonitorOrientation,
    { GP::HAL::EMonitorOrientation::Landscape, "Landscape" },
    { GP::HAL::EMonitorOrientation::Portrait, "Portrait" },
    { GP::HAL::EMonitorOrientation::LandscapeFlipped, "Landscape (Flipped)" },
    { GP::HAL::EMonitorOrientation::PortraitFlipped, "Portrait (Flipped)" }
);

GP_ENUM_TO_STRING(
    GP::HAL::EMonitorVRRType,
    { GP::HAL::EMonitorVRRType::None, "None" },
    { GP::HAL::EMonitorVRRType::FreeSync, "AMD FreeSync" },
    { GP::HAL::EMonitorVRRType::GSync, "NVIDIA G-Sync" },
    { GP::HAL::EMonitorVRRType::GSyncComp, "NVIDIA G-Sync Compatible" },
    { GP::HAL::EMonitorVRRType::HDMI_VRR, "HDMI 2.1 VRR" }
);
