// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanStateSettings.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "maths/base/Arithmetic.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarMaxAnisotropy(
    "r.MaxAnisotropy",
    8,
    "Default anisotropic tap count of samplers that do not request one. Clamped to [1, 16] and to the device limit.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

}   // namespace

gp::Int32 getVulkanDefaultMaxAnisotropy() noexcept
{
    return g_cvarMaxAnisotropy.getValueOnAnyThread();
}

gp::Int32 computeVulkanSamplerAnisotropy(const gp::Int32 requestedAnisotropy) noexcept
{
    const gp::Int32 anisotropy = (requestedAnisotropy > 0) ? requestedAnisotropy : getVulkanDefaultMaxAnisotropy();
    return gp::math::clamp<gp::Int32>(anisotropy, kMinSamplerAnisotropy, kMaxSamplerAnisotropy);
}

}   // namespace gp::rhi::vulkan
