// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineSettings.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarEnablePipelineLRU(
    "r.Vulkan.EnablePipelineLRUCache",
    0,
    "Tracks graphics pipelines in an LRU and evicts the least recently used ones past the size or count budget.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarPipelineLRUSize(
    "r.Vulkan.PipelineLRUSize",
    512 * 1024 * 1024,
    "Byte budget of live graphics pipelines when the pipeline LRU is enabled.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarPipelineLRUCapacity(
    "r.Vulkan.PipelineLRUCapacity",
    8192,
    "Largest number of live graphics pipelines when the pipeline LRU is enabled.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarEvictAfterUnusedFrames(
    "r.Vulkan.PSOLRUEvictAfterUnusedFrames",
    0,
    "Evicts graphics pipelines unused for this many frames (0 disables aging).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarReleaseShaderModules(
    "r.Vulkan.ReleaseShaderModuleWhenEvictingPSO",
    0,
    "Drops the cached shader modules of a graphics pipeline when the LRU evicts it.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarEvictPrecachedImmediately(
    "r.Vulkan.PipelineLRUCacheEvictImmediately",
    0,
    "With the pipeline LRU enabled, precache requests record the pipeline description without compiling it.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Reads a non-negative integer console variable as an unsigned value.
/// @param[in] value Console variable value.
/// @return `value`, or 0 when negative.
[[nodiscard]] gp::UInt32 toUnsigned(const gp::Int32 value) noexcept
{
    return (value > 0) ? static_cast<gp::UInt32>(value) : 0u;
}

}   // namespace

VulkanPipelineLRUSettings getVulkanPipelineLRUSettings() noexcept
{
    VulkanPipelineLRUSettings settings;
    settings.isEnabled = g_cvarEnablePipelineLRU.getValueOnAnyThread() != 0;
    settings.maxSizeInBytes = toUnsigned(g_cvarPipelineLRUSize.getValueOnAnyThread());
    settings.capacity = toUnsigned(g_cvarPipelineLRUCapacity.getValueOnAnyThread());
    settings.evictAfterUnusedFrames = toUnsigned(g_cvarEvictAfterUnusedFrames.getValueOnAnyThread());
    settings.releaseShaderModulesOnEvict = g_cvarReleaseShaderModules.getValueOnAnyThread() != 0;
    settings.evictPrecachedImmediately = g_cvarEvictPrecachedImmediately.getValueOnAnyThread() != 0;
    return settings;
}

}   // namespace gp::rhi::vulkan
