// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "HALBuild.hpp"
#include "Math/Utils/Linear.hpp"

namespace GP::HAL
{

/// \brief Physical dimensions of the monitor panel.
struct FMonitorPhysicalSize
{
public:
    UInt32 widthMM{ 0 };    //<! Physical width of the display area in millimetres.
    UInt32 heightMM{ 0 };   //<! Physical height of the display area in millimetres.

public:
    /// \brief Computes the diagonal size in inches.
    /// \return Diagonal length of the display in inches, or 0.0f if dimensions are not available.
    GP_NODISCARD GP_INLINE Float32 GetDiagonalInches() const noexcept
    {
        if (widthMM == 0u || heightMM == 0u) { return 0.0f; }
        const Float32 diagMM =
            Math::Sqrt(static_cast<Float32>(widthMM * widthMM) + static_cast<Float32>(heightMM * heightMM));
        return diagMM / 25.4f;
    }
};

/// \brief A single color calibration point used in the monitor's ICC / color profile representation.
/// \note These values reflect the chromaticity coordinates (CIE xy) of each primary and the white point.
struct FMonitorColorProfile
{
    Float32 redX{ 0.0f };     //<! CIE x chromaticity of the red primary.
    Float32 redY{ 0.0f };     //<! CIE y chromaticity of the red primary.
    Float32 greenX{ 0.0f };   //<! CIE x chromaticity of the green primary.
    Float32 greenY{ 0.0f };   //<! CIE y chromaticity of the green primary.
    Float32 blueX{ 0.0f };    //<! CIE x chromaticity of the blue primary.
    Float32 blueY{ 0.0f };    //<! CIE y chromaticity of the blue primary.
    Float32 whiteX{ 0.0f };   //<! CIE x chromaticity of the white point (D65 = 0.3127).
    Float32 whiteY{ 0.0f };   //<! CIE y chromaticity of the white point (D65 = 0.3290).
    Float32 gamma{ 2.2f };    //<! Display gamma exponent (1/EOTF exponent for SDR content).
};

/// \brief HDR luminance metadata reported by the monitor's EDID / DisplayID.
/// Values may be zero if the monitor does not support HDR or the driver cannot query them.
struct FMonitorHDRMetadata
{
    Float32 maxLuminance{ 0.0f };               //<! Peak luminance in nits (cd/m²) the panel can sustain.
    Float32 maxFrameAverageLuminance{ 0.0f };   //<! Maximum average luminance across a full frame, in nits.
    Float32 minLuminance{ 0.0f };               //<! Minimum achievable black luminance, in nits.
};

}   // namespace GP::HAL
