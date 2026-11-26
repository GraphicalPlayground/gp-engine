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

}   // namespace gp::rhi::vulkan
