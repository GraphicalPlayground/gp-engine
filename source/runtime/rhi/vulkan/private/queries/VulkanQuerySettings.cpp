// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQuerySettings.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarTimestampQueryStage(
    "r.Vulkan.TimestampQueryStage",
    0,
    "Defines which pipeline stage is used for timestamp queries.\n"
    " 0: Use VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, less precise measures but less likely to alter performance "
    "(default)\n"
    " 1: Use VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, more precise measures but may alter performance on some platforms",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarTimingQueryPoolSize(
    "r.Vulkan.TimingQueryPoolSize",
    128,
    "Amount of timing queries per pool in reusable query pools. (Default: 128)\n",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarQueryPoolDeletionDelay(
    "r.Vulkan.QueryPoolDeletionDelay",
    10,
    "Amount of frames to wait before deleting an unused query pools. (Default: 10)\n",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarQueryUseControlPrecisionBit(
    "r.Vulkan.QueryUseControlPrecisionBit",
    1,
    "Whether to use VK_QUERY_CONTROL_PRECISE_BIT when doing a render query. (Default : 1 (on), 0 (off))\n",
    gp::console::ConsoleVarFlags::ReadOnly
);

}   // namespace

VkPipelineStageFlagBits getVulkanTimestampQueryStage() noexcept
{
    return (g_cvarTimestampQueryStage.getValueOnAnyThread() != 0) ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                                                                  : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}

gp::UInt32 getVulkanTimingQueryPoolSize() noexcept
{
    const gp::Int32 size = g_cvarTimingQueryPoolSize.getValueOnAnyThread();
    return (size > 0) ? static_cast<gp::UInt32>(size) : 1u;
}

gp::Int32 getVulkanQueryPoolDeletionDelay() noexcept
{
    return g_cvarQueryPoolDeletionDelay.getValueOnAnyThread();
}

VkQueryControlFlags getVulkanOcclusionQueryControlFlags() noexcept
{
    return (g_cvarQueryUseControlPrecisionBit.getValueOnAnyThread() > 0)
               ? static_cast<VkQueryControlFlags>(VK_QUERY_CONTROL_PRECISE_BIT)
               : 0u;
}

gp::UInt64 convertVulkanTimestampToMicroseconds(const gp::UInt64 ticks, const gp::Float32 timestampPeriod) noexcept
{
    if (!(timestampPeriod > 0.0f))
    {
        return 0u;
    }

    const gp::Float64 nanosecondsPerTick = static_cast<gp::Float64>(timestampPeriod);
    const gp::Float64 ticksPerSecond =
        static_cast<gp::Float64>(static_cast<gp::UInt64>((1000.0 * 1000.0 * 1000.0) / nanosecondsPerTick));
    if (!(ticksPerSecond > 0.0))
    {
        return 0u;
    }
    return static_cast<gp::UInt64>((static_cast<gp::Float64>(ticks) / ticksPerSecond) * 1000.0 * 1000.0);
}

}   // namespace gp::rhi::vulkan
