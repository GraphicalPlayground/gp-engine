// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanGraphicsPipelineDesc.hpp"
#include "containers/arrays/Array.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/BlendState.hpp"             // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilState.hpp"      // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/RasterizerState.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/VulkanPipelineKey.hpp"
#include "pipeline/VulkanPipelineTypes.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "states/VulkanStateRHI.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Fixture flushing the RHI deletion queue after each test.
class VulkanGraphicsPipelineDescTest : public ::testing::Test
{
protected:
    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }
};

/// @brief Returns a vertex pipeline description with distinct shader keys.
/// @return Description.
[[nodiscard]] VulkanGraphicsPipelineDesc makeDesc()
{
    VulkanGraphicsPipelineDesc desc{};
    desc.shaderKeys[static_cast<gp::USize>(shader_stage::Stage::Vertex)] = 0x1111u;
    desc.shaderKeys[static_cast<gp::USize>(shader_stage::Stage::Pixel)] = 0x2222u;
    desc.shaderKeyShared = combineVulkanShaderKeys(desc.shaderKeys);
    desc.rasterizationSamples = 1u;
    desc.topology = static_cast<gp::UInt32>(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    desc.colorAttachmentStates.emplaceBack();
    return desc;
}

}   // namespace

TEST(VulkanGraphicsPipelineDescKeyTest, CombinedShaderKeyDependsOnStageOrder)
{
    gp::Array<gp::UInt64, kNumGraphicsPipelineStages> keys{};
    keys[static_cast<gp::USize>(shader_stage::Stage::Vertex)] = 0x1111u;
    keys[static_cast<gp::USize>(shader_stage::Stage::Pixel)] = 0x2222u;
    gp::Array<gp::UInt64, kNumGraphicsPipelineStages> swapped{};
    swapped[static_cast<gp::USize>(shader_stage::Stage::Vertex)] = 0x2222u;
    swapped[static_cast<gp::USize>(shader_stage::Stage::Pixel)] = 0x1111u;

    EXPECT_EQ(combineVulkanShaderKeys(keys), combineVulkanShaderKeys(keys));
    EXPECT_NE(combineVulkanShaderKeys(keys), combineVulkanShaderKeys(swapped));
}

TEST(VulkanGraphicsPipelineDescKeyTest, EqualDescsProduceEqualKeys)
{
    const VulkanGraphicsPipelineDesc first = makeDesc();
    const VulkanGraphicsPipelineDesc second = makeDesc();

    const VulkanPipelineKey firstKey = first.createKey();
    const VulkanPipelineKey secondKey = second.createKey();

    EXPECT_TRUE(first == second);
    EXPECT_FALSE(firstKey.isEmpty());
    EXPECT_TRUE(firstKey == secondKey);
    EXPECT_EQ(firstKey.getHash(), secondKey.getHash());
}

TEST(VulkanGraphicsPipelineDescKeyTest, EveryFieldReachesTheKey)
{
    const VulkanPipelineKey baseKey = makeDesc().createKey();

    VulkanGraphicsPipelineDesc subpass = makeDesc();
    subpass.subpassIndex = 1u;
    VulkanGraphicsPipelineDesc samples = makeDesc();
    samples.rasterizationSamples = 4u;
    VulkanGraphicsPipelineDesc attachments = makeDesc();
    attachments.colorAttachmentStates.emplaceBack();
    VulkanGraphicsPipelineDesc specialization = makeDesc();
    specialization.specializationKey = 42u;

    EXPECT_FALSE(baseKey == subpass.createKey());
    EXPECT_FALSE(baseKey == samples.createKey());
    EXPECT_FALSE(baseKey == attachments.createKey());
    EXPECT_FALSE(baseKey == specialization.createKey());
    EXPECT_FALSE(subpass == makeDesc());
}

TEST(VulkanGraphicsPipelineDescKeyTest, MeshShaderIsDetectedFromItsKey)
{
    VulkanGraphicsPipelineDesc desc = makeDesc();
    EXPECT_FALSE(desc.hasMeshShader());

    desc.shaderKeys[static_cast<gp::USize>(shader_stage::Stage::Mesh)] = 5u;
    EXPECT_TRUE(desc.hasMeshShader());
}

TEST_F(VulkanGraphicsPipelineDescTest, BuildRejectsMissingFixedFunctionStates)
{
    const VkPhysicalDeviceLimits limits{};
    const VulkanRenderTargetCaps caps{};
    const pipeline::GraphicsPipelineStateInitializer initializer{};
    VulkanDescriptorSetsLayoutInfo layoutInfo{};
    VulkanGraphicsPipelineDesc desc{};

    EXPECT_FALSE(buildVulkanGraphicsPipelineDesc(limits, caps, initializer, layoutInfo, desc));
}

TEST_F(VulkanGraphicsPipelineDescTest, BuildRejectsInitializersWithoutShaders)
{
    const pipeline::BlendStateRef blend = createVulkanBlendState(pipeline::BlendStateDesc{});
    const pipeline::RasterizerStateRef rasterizer = createVulkanRasterizerState(pipeline::RasterizerStateDesc{});
    const pipeline::DepthStencilStateRef depthStencil =
        createVulkanDepthStencilState(pipeline::DepthStencilStateDesc{});

    pipeline::GraphicsPipelineStateInitializer initializer{};
    initializer.blendState = blend.get();
    initializer.rasterizerState = rasterizer.get();
    initializer.depthStencilState = depthStencil.get();

    const VkPhysicalDeviceLimits limits{};
    const VulkanRenderTargetCaps caps{};
    VulkanDescriptorSetsLayoutInfo layoutInfo{};
    VulkanGraphicsPipelineDesc desc{};

    EXPECT_FALSE(buildVulkanGraphicsPipelineDesc(limits, caps, initializer, layoutInfo, desc));
}

}   // namespace gp::rhi::vulkan::tests
