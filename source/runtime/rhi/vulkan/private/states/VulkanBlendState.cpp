// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanBlendState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "translation/VulkanPipelineEnums.hpp"

namespace gp::rhi::vulkan
{

VulkanBlendState::VulkanBlendState(const pipeline::BlendStateDesc& desc) noexcept
    : m_desc(desc)
{
    for (gp::UInt32 index = 0u; index < kMaxSimultaneousRenderTargets; ++index)
    {
        m_attachmentStates[index] = translateRenderTarget(desc.renderTargets[index]);
    }
}

VulkanBlendState::~VulkanBlendState() noexcept = default;

VkPipelineColorBlendAttachmentState
    VulkanBlendState::translateRenderTarget(const pipeline::RenderTargetBlendDesc& desc) noexcept
{
    VkPipelineColorBlendAttachmentState state{};

    state.colorBlendOp = blendOperationToVkBlendOp(desc.colorBlendOp);
    state.alphaBlendOp = blendOperationToVkBlendOp(desc.alphaBlendOp);
    state.dstColorBlendFactor = blendFactorToVkBlendFactor(desc.colorDestBlend);
    state.dstAlphaBlendFactor = blendFactorToVkBlendFactor(desc.alphaDestBlend);
    state.srcColorBlendFactor = blendFactorToVkBlendFactor(desc.colorSrcBlend);
    state.srcAlphaBlendFactor = blendFactorToVkBlendFactor(desc.alphaSrcBlend);

    // `src * One + dst * Zero` writes the source unchanged, so the blend unit is switched off for that equation.
    const bool isIdentity =
        (desc.colorBlendOp == pipeline::BlendOperation::Add) && (desc.colorDestBlend == pipeline::BlendFactor::Zero) &&
        (desc.colorSrcBlend == pipeline::BlendFactor::One) && (desc.alphaBlendOp == pipeline::BlendOperation::Add) &&
        (desc.alphaDestBlend == pipeline::BlendFactor::Zero) && (desc.alphaSrcBlend == pipeline::BlendFactor::One);
    state.blendEnable = isIdentity ? VK_FALSE : VK_TRUE;

    state.colorWriteMask = colorWriteMaskToVkColorComponentFlags(desc.colorWriteMask);
    return state;
}

bool VulkanBlendState::getInitializer(pipeline::BlendStateDesc& outDesc) const noexcept
{
    outDesc = m_desc;
    return true;
}

const VkPipelineColorBlendAttachmentState&
    VulkanBlendState::getAttachmentState(const gp::UInt32 renderTargetIndex) const noexcept
{
    GP_CHECK(renderTargetIndex < kMaxSimultaneousRenderTargets);
    return m_attachmentStates[renderTargetIndex];
}

}   // namespace gp::rhi::vulkan
