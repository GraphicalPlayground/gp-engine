// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanRasterizerState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "translation/VulkanPipelineEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

VulkanRasterizerState::VulkanRasterizerState(const pipeline::RasterizerStateDesc& desc) noexcept
    : m_createInfo()
    , m_desc(desc)
{
    resetCreateInfo(m_createInfo);

    // Solid and wireframe are never mixed inside one bound shader state, so the fill mode is not a pipeline key on
    // its own: it only travels inside this create info.
    m_createInfo.polygonMode = rasterizerFillModeToVkPolygonMode(desc.fillMode);
    m_createInfo.cullMode = rasterizerCullModeToVkCullMode(desc.cullMode);

    m_createInfo.depthClampEnable =
        (desc.depthClipMode == pipeline::RasterizerDepthClipMode::DepthClamp) ? VK_TRUE : VK_FALSE;
    m_createInfo.depthBiasEnable = (desc.depthBias != 0.0f) ? VK_TRUE : VK_FALSE;
    m_createInfo.depthBiasSlopeFactor = desc.slopeScaleDepthBias;
    m_createInfo.depthBiasConstantFactor = desc.depthBias;
}

VulkanRasterizerState::~VulkanRasterizerState() noexcept = default;

void VulkanRasterizerState::resetCreateInfo(VkPipelineRasterizationStateCreateInfo& outCreateInfo) noexcept
{
    zeroVulkanStruct(outCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    outCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    outCreateInfo.lineWidth = 1.0f;
}

bool VulkanRasterizerState::getInitializer(pipeline::RasterizerStateDesc& outDesc) const noexcept
{
    outDesc = m_desc;
    return true;
}

}   // namespace gp::rhi::vulkan
