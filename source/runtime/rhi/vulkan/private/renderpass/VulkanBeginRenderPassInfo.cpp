// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanBeginRenderPassInfo.hpp"
#include "commands/RenderPassInfo.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "maths/color/LinearColor.hpp"
#include "memory/Memory.hpp"
#include "renderpass/VulkanFramebuffer.hpp"
#include "renderpass/VulkanRenderPass.hpp"
#include "resources/Texture.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

void VulkanBeginRenderPassInfo::build(
    const command::RenderPassInfo& rpInfo, const VulkanRenderPass& renderPass, const VulkanFramebuffer& framebuffer
) noexcept
{
    gp::memory::zeroMemory(clearValues.data(), sizeof(VkClearValue) * clearValues.size());

    const VulkanRenderTargetLayout& layout = renderPass.getLayout();
    const gp::UInt32 numUsedClearValues = renderPass.getNumUsedClearValues();
    if (numUsedClearValues > 0u)
    {
        const gp::UInt32 numColors = layout.getNumColorAttachments();
        for (gp::UInt32 index = 0u; index < numColors; ++index)
        {
            const resource::Texture* const texture = rpInfo.colorRenderTargets[index].renderTarget;
            const gp::UInt32 attachment = layout.getColorAttachmentReferences()[index].attachment;
            if ((texture == nullptr) || (attachment >= numUsedClearValues))
            {
                continue;
            }

            const gp::math::LinearColor color =
                texture->hasClearValue() ? texture->getClearColor() : gp::math::LinearColor(0.0f, 0.0f, 0.0f, 1.0f);
            VkClearValue& value = clearValues[attachment];
            value.color.float32[0] = color.r;
            value.color.float32[1] = color.g;
            value.color.float32[2] = color.b;
            value.color.float32[3] = color.a;
        }

        const resource::Texture* const depthTexture = rpInfo.depthStencilRenderTarget.depthStencilTarget;
        const VkAttachmentReference* const depthReference = layout.getDepthAttachmentReference();
        if ((depthTexture != nullptr) && (depthReference != nullptr) &&
            (depthReference->attachment < numUsedClearValues))
        {
            gp::Float32 depth = 0.0f;
            gp::UInt32 stencil = 0u;
            depthTexture->getDepthStencilClearValue(depth, stencil);
            VkClearValue& value = clearValues[depthReference->attachment];
            value.depthStencil.depth = depth;
            value.depthStencil.stencil = stencil;
        }
    }

    zeroVulkanStruct(beginInfo, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    beginInfo.renderPass = renderPass.getHandle();
    beginInfo.framebuffer = framebuffer.getHandle();
    beginInfo.renderArea = framebuffer.getRenderArea();
    beginInfo.clearValueCount = numUsedClearValues;
    beginInfo.pClearValues = (numUsedClearValues > 0u) ? clearValues.data() : nullptr;
}

}   // namespace gp::rhi::vulkan
