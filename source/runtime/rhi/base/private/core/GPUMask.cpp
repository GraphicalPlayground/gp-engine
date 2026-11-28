// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/GPUMask.hpp"

namespace gp::rhi
{

GP_RHI_API gp::UInt32 g_numExplicitGPUsForRendering = 1u;
GP_RHI_API gp::UInt32 g_virtualMGPU = 0u;

gp::UInt32 getNumExplicitGPUsForRendering() noexcept
{
    if (g_numExplicitGPUsForRendering == 0u)
    {
        return 1u;
    }
    if (g_numExplicitGPUsForRendering > GPUMask::kMaxGPUs)
    {
        return GPUMask::kMaxGPUs;
    }
    return g_numExplicitGPUsForRendering;
}

void setNumExplicitGPUsForRendering(const gp::UInt32 numGPUs) noexcept
{
    if (numGPUs == 0u)
    {
        g_numExplicitGPUsForRendering = 1u;
        return;
    }
    g_numExplicitGPUsForRendering = (numGPUs > GPUMask::kMaxGPUs) ? GPUMask::kMaxGPUs : numGPUs;
}

gp::UInt32 getVirtualMGPU() noexcept
{
    return g_virtualMGPU;
}

void setVirtualMGPU(const gp::UInt32 virtualGPUCount) noexcept
{
    g_virtualMGPU = virtualGPUCount;
    if (virtualGPUCount > 1u)
    {
        setNumExplicitGPUsForRendering(virtualGPUCount);
    }
}

}   // namespace gp::rhi
