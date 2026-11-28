// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanMemoryStatsTranslation.hpp"
#include "CoreMinimal.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "profiling/RHIStats.hpp"

namespace gp::rhi::vulkan
{

profiling::RHIMemoryStats makeVulkanRHIMemoryStats(
    const VulkanMemoryStats& stats, const gp::UInt64 deviceLocalTotal, const gp::UInt64 hostTotal
) noexcept
{
    profiling::RHIMemoryStats result{};
    result.dedicatedVideoMemory = deviceLocalTotal;
    result.dedicatedSystemMemory = 0u;
    result.sharedSystemMemory = hostTotal;
    result.totalGraphicsMemory = deviceLocalTotal + hostTotal;
    result.demotedVideoMemory = stats.demotedLocal;
    result.budgetVideoMemory = stats.budgetLocal;
    result.usedVideoMemory = stats.usedLocal;
    result.availableVideoMemory = stats.availableLocal;
    return result;
}

}   // namespace gp::rhi::vulkan
