// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIGlobals.hpp"

namespace gp::rhi
{

GP_RHI_API gp::Float32 g_minClipZ = 0.0f;
GP_RHI_API gp::Float32 g_projectionSignY = 1.0f;

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
