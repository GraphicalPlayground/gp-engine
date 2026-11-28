// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderPassSettings.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarInputAttachmentShaderRead(
    "r.Vulkan.InputAttachmentShaderRead",
    0,
    "Adds SHADER_READ to the access mask of deferred shading subpass dependencies (works around driver flicker).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarDynamicRendering(
    "r.Vulkan.DynamicRendering",
    0,
    "Records passes through VK_KHR_dynamic_rendering instead of render pass and framebuffer objects when supported.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

}   // namespace

bool getVulkanInputAttachmentShaderRead() noexcept
{
    return g_cvarInputAttachmentShaderRead.getValueOnAnyThread() != 0;
}

bool getVulkanAllowDynamicRendering() noexcept
{
    return g_cvarDynamicRendering.getValueOnAnyThread() != 0;
}

}   // namespace gp::rhi::vulkan
