// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSettings.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsEnums.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarDynamicGlobalUBs(
    "r.Vulkan.DynamicGlobalUBs",
    2,
    "2 to treat every uniform buffer as dynamic (default), 1 to treat only packed globals as dynamic, 0 for none.",
    gp::console::ConsoleVarFlags::ReadOnly | gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarNumFramesToWaitForDescriptorDelete(
    "r.Vulkan.NumFramesToWaitForDescriptorDelete",
    2,
    "Number of frames an unused descriptor pool set waits before it is destroyed.",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarDescriptorSetCache(
    "r.Vulkan.DescriptorSetCache",
    0,
    "1 to deduplicate descriptor sets by content and reuse them across draws, 0 to rewrite per-frame sets (default).",
    gp::console::ConsoleVarFlags::ReadOnly | gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarDSetCacheTargetSetsPerPool(
    "r.Vulkan.DSetCacheTargetSetsPerPool",
    4096,
    "Target number of descriptor set allocations per single cached pool.",
    gp::console::ConsoleVarFlags::ReadOnly | gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarDSetCacheMaxPoolLookups(
    "r.Vulkan.DSetCacheMaxPoolLookups",
    2,
    "Maximum number of cached pools searched before a new descriptor set is created.",
    gp::console::ConsoleVarFlags::ReadOnly | gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAlwaysWriteDS(
    "r.Vulkan.AlwaysWriteDS",
    0,
    "1 to rewrite descriptor sets on every draw even when no binding changed (debugging aid).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

}   // namespace

VulkanDynamicUniformBufferMode getVulkanDynamicUniformBufferMode() noexcept
{
    const gp::Int32 value = g_cvarDynamicGlobalUBs.getValueOnAnyThread();
    if (value <= 0)
    {
        return VulkanDynamicUniformBufferMode::None;
    }
    if (value == 1)
    {
        return VulkanDynamicUniformBufferMode::PackedGlobals;
    }
    return VulkanDynamicUniformBufferMode::All;
}

gp::UInt32 getVulkanNumFramesToWaitForDescriptorDelete() noexcept
{
    const gp::Int32 value = g_cvarNumFramesToWaitForDescriptorDelete.getValueOnAnyThread();
    return (value > 0) ? static_cast<gp::UInt32>(value) : 0u;
}

bool useVulkanDescriptorCache() noexcept
{
    return g_cvarDescriptorSetCache.getValueOnAnyThread() != 0;
}

gp::UInt32 getVulkanDescriptorSetCacheTargetSetsPerPool() noexcept
{
    const gp::Int32 value = g_cvarDSetCacheTargetSetsPerPool.getValueOnAnyThread();
    return (value > 0) ? static_cast<gp::UInt32>(value) : 1u;
}

gp::UInt32 getVulkanDescriptorSetCacheMaxPoolLookups() noexcept
{
    const gp::Int32 value = g_cvarDSetCacheMaxPoolLookups.getValueOnAnyThread();
    return (value > 0) ? static_cast<gp::UInt32>(value) : 1u;
}

bool shouldVulkanAlwaysWriteDescriptors() noexcept
{
#if GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT
    return g_cvarAlwaysWriteDS.getValueOnAnyThread() != 0;
#else
    return false;
#endif
}

}   // namespace gp::rhi::vulkan
