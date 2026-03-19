// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"

namespace GP
{

/// @brief Describes the physical orientation of a display panel.
enum class EDisplayOrientation : UInt8
{
    Landscape,          //<! The display is in landscape orientation.
    Portrait,           //<! The display is in portrait orientation.
    LandscapeFlipped,   //<! The display is in landscape orientation but flipped.
    PortraitFlipped     //<! The display is in portrait orientation but flipped.
};

/// @brief Describes the colour gamut and transfer function that a physical display is capable of rendering.
/// @note This is distinct from GP::EColorSpace (RHI swap-chain surface colour space) which specifies what the GPU
///       encodes into the framebuffer. EDisplayColorSpace instead reflects the hardware capability reported by the OS
///       display sub-system (EDID, ICC profile, etc.) and is used to select an appropriate swap-chain surface format
///       and tone-mapping strategy.
/// @note Not all platforms expose colour-space capability. Unknown should be treated as sRGB.
enum class EDisplayColorSpace : UInt8
{
    Unknown,      //<! Cannot be determined or not reported by the platform.
    sRGB,         //<! Standard Dynamic Range, ITU-R BT.709 primaries with sRGB transfer function.
    DisplayP3,    //<! Wide colour gamut DCI-P3 primaries with display-P3 transfer function.
    Rec2020,      //<! Wide colour gamut ITU-R BT.2020 primaries, SDR transfer function (gamma 2.2 / BT.1886).
    HDR10_PQ,     //<! High Dynamic Range, ITU-R BT.2020 primaries with SMPTE ST.2084 (PQ) transfer function.
    HDR10_HLG,    //<! High Dynamic Range, ITU-R BT.2020 primaries with ARIB STD-B67 (HLG) transfer function.
    DolbyVision   //<! Dolby Vision dual-layer HDR with proprietary metadata and tone mapping.
};

}   // namespace GP
