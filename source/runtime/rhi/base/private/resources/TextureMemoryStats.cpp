// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/TextureMemoryStats.hpp"
#include "device/RHIGlobals.hpp"

namespace gp::rhi::resource
{

gp::Int64 TextureMemoryStats::getTotalDeviceWorkingMemory() const noexcept
{
    const RHIGlobals& globals = getRHIGlobals();
    const bool isIntegrated = globals.deviceIsIntegrated || globals.gpuInfo.deviceIsIntegrated;
    return getTotalDeviceWorkingMemory(isIntegrated);
}

}   // namespace gp::rhi::resource
