// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIGlobals.hpp"

namespace gp::rhi
{

namespace
{

constexpr gp::math::Vector2<gp::Float32> kDefaultMSAASampleOffsets[kDefaultMSAASampleOffsetCount] = {
    // MSAA x1
    gp::math::Vector2<gp::Float32>(+0.0f / 8.0f, +0.0f / 8.0f),

    // MSAA x2
    gp::math::Vector2<gp::Float32>(+4.0f / 8.0f, +4.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-4.0f / 8.0f, -4.0f / 8.0f),

    // MSAA x4
    gp::math::Vector2<gp::Float32>(-2.0f / 8.0f, -6.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+6.0f / 8.0f, -2.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-6.0f / 8.0f, +2.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+2.0f / 8.0f, +6.0f / 8.0f),

    // MSAA x8
    gp::math::Vector2<gp::Float32>(+1.0f / 8.0f, -3.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-1.0f / 8.0f, +3.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+5.0f / 8.0f, +1.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-3.0f / 8.0f, -5.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-5.0f / 8.0f, +5.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-7.0f / 8.0f, -1.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+3.0f / 8.0f, +7.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+7.0f / 8.0f, -7.0f / 8.0f),

    // MSAA x16
    gp::math::Vector2<gp::Float32>(+1.0f / 8.0f, +1.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-1.0f / 8.0f, -3.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-3.0f / 8.0f, +2.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+4.0f / 8.0f, -1.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-5.0f / 8.0f, -2.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+2.0f / 8.0f, +5.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+5.0f / 8.0f, +3.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+3.0f / 8.0f, -5.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-2.0f / 8.0f, +6.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+0.0f / 8.0f, -7.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-4.0f / 8.0f, -6.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-6.0f / 8.0f, +4.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-8.0f / 8.0f, +0.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+7.0f / 8.0f, -4.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(+6.0f / 8.0f, +7.0f / 8.0f),
    gp::math::Vector2<gp::Float32>(-7.0f / 8.0f, -8.0f / 8.0f),
};

}   // namespace

GP_RHI_API gp::Float32 g_minClipZ = 0.0f;
GP_RHI_API gp::Float32 g_projectionSignY = 1.0f;

RHIGlobals::RHIGlobals() noexcept
{
    for (gp::USize i = 0u; i < kDefaultMSAASampleOffsetCount; ++i)
    {
        defaultMSAASampleOffsets[i] = kDefaultMSAASampleOffsets[i];
    }
}

void RHIGlobals::reset() noexcept
{
    *this = RHIGlobals{};
}

RHIGlobals& getRHIGlobals() noexcept
{
    static RHIGlobals s_globals{};
    return s_globals;
}

}   // namespace gp::rhi
