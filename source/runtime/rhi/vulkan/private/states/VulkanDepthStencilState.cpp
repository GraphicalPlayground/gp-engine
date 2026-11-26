// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanDepthStencilState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "translation/VulkanPipelineEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Builds the native stencil op state of one face.
/// @param[in] failOp Op applied when the stencil test fails.
/// @param[in] depthFailOp Op applied when the stencil test passes and the depth test fails.
/// @param[in] passOp Op applied when both tests pass.
/// @param[in] compare Stencil comparison.
/// @param[in] readMask Stencil compare mask.
/// @param[in] writeMask Stencil write mask.
/// @return Native face state with a zero reference (the reference is dynamic state).
[[nodiscard]] VkStencilOpState makeStencilOpState(
    const pipeline::StencilOp failOp,
    const pipeline::StencilOp depthFailOp,
    const pipeline::StencilOp passOp,
    const pipeline::CompareFunction compare,
    const gp::UInt8 readMask,
    const gp::UInt8 writeMask
) noexcept
{
    VkStencilOpState state{};
    state.failOp = stencilOpToVkStencilOp(failOp);
    state.passOp = stencilOpToVkStencilOp(passOp);
    state.depthFailOp = stencilOpToVkStencilOp(depthFailOp);
    state.compareOp = compareFunctionToVkCompareOp(compare);
    state.compareMask = readMask;
    state.writeMask = writeMask;
    state.reference = 0u;
    return state;
}

}   // namespace

VulkanDepthStencilState::VulkanDepthStencilState(const pipeline::DepthStencilStateDesc& desc) noexcept
    : m_desc(desc)
{}

VulkanDepthStencilState::~VulkanDepthStencilState() noexcept = default;

void VulkanDepthStencilState::setupCreateInfo(
    const pipeline::GraphicsPipelineStateInitializer& pipelineInitializer,
    VkPipelineDepthStencilStateCreateInfo& outCreateInfo
) const noexcept
{
    setupCreateInfo(pipelineInitializer.depthBounds, outCreateInfo);
}

void VulkanDepthStencilState::setupCreateInfo(
    const bool enableDepthBounds, VkPipelineDepthStencilStateCreateInfo& outCreateInfo
) const noexcept
{
    zeroVulkanStruct(outCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);

    outCreateInfo.depthTestEnable =
        ((m_desc.depthTest != pipeline::CompareFunction::Always) || m_desc.enableDepthWrite) ? VK_TRUE : VK_FALSE;
    outCreateInfo.depthCompareOp = compareFunctionToVkCompareOp(m_desc.depthTest);
    outCreateInfo.depthWriteEnable = m_desc.enableDepthWrite ? VK_TRUE : VK_FALSE;

    // The bounds themselves are dynamic state; the pipeline only decides whether the test runs.
    outCreateInfo.depthBoundsTestEnable = enableDepthBounds ? VK_TRUE : VK_FALSE;
    outCreateInfo.minDepthBounds = 0.0f;
    outCreateInfo.maxDepthBounds = 1.0f;

    outCreateInfo.stencilTestEnable =
        (m_desc.enableFrontFaceStencil || m_desc.enableBackFaceStencil) ? VK_TRUE : VK_FALSE;

    // Engine front face -> native back face.
    outCreateInfo.back = makeStencilOpState(
        m_desc.frontFaceStencilFailOp,
        m_desc.frontFaceDepthFailOp,
        m_desc.frontFacePassOp,
        m_desc.frontFaceStencilTest,
        m_desc.stencilReadMask,
        m_desc.stencilWriteMask
    );

    if (m_desc.enableBackFaceStencil)
    {
        // Engine back face -> native front face.
        outCreateInfo.front = makeStencilOpState(
            m_desc.backFaceStencilFailOp,
            m_desc.backFaceDepthFailOp,
            m_desc.backFacePassOp,
            m_desc.backFaceStencilTest,
            m_desc.stencilReadMask,
            m_desc.stencilWriteMask
        );
    }
    else
    {
        outCreateInfo.front = outCreateInfo.back;
    }
}

bool VulkanDepthStencilState::getInitializer(pipeline::DepthStencilStateDesc& outDesc) const noexcept
{
    outDesc = m_desc;
    return true;
}

}   // namespace gp::rhi::vulkan
