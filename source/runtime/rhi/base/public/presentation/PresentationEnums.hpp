// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::presentation
{

/// @brief Packed color gamut and electro-optical transfer function (EOTF) for swapchain output.
enum class ColorSpaceAndEOTF : gp::UInt8
{
    Unknown = 0x00u,

    ColorSpaceRec709 = 1u,
    ColorSpaceRec2020 = 2u,
    ColorSpaceDCIP3 = 3u,
    ColorSpaceMask = 0x0Fu,

    EOTFLinear = 1u << 4,
    EOTFsRGB = 2u << 4,
    EOTFPQ = 3u << 4,
    EOTFMask = 0xF0u,

    Rec709sRGB = 1u | (2u << 4),
    Rec709Linear = 1u | (1u << 4),

    Rec2020PQ = 2u | (3u << 4),
    Rec2020Linear = 2u | (1u << 4),

    DCIP3PQ = 3u | (3u << 4),
    DCIP3Linear = 3u | (1u << 4)
};

}   // namespace gp::rhi::presentation

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::presentation::ColorSpaceAndEOTF);
