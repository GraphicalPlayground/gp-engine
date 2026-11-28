// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanDepthStencilState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "states/VulkanStateRHI.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanDepthStencilStateTest, DefaultStateTestsAndWritesDepth)
{
    const gp::RefCountedPtr<VulkanDepthStencilState> state(
        new VulkanDepthStencilState(pipeline::DepthStencilStateDesc{})
    );
    VkPipelineDepthStencilStateCreateInfo createInfo{};
    state->setupCreateInfo(false, createInfo);

    EXPECT_EQ(createInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
    EXPECT_EQ(createInfo.depthTestEnable, VK_TRUE);
    EXPECT_EQ(createInfo.depthWriteEnable, VK_TRUE);
    EXPECT_EQ(createInfo.depthCompareOp, VK_COMPARE_OP_LESS_OR_EQUAL);
    EXPECT_EQ(createInfo.stencilTestEnable, VK_FALSE);
    EXPECT_EQ(createInfo.depthBoundsTestEnable, VK_FALSE);
    EXPECT_FLOAT_EQ(createInfo.minDepthBounds, 0.0f);
    EXPECT_FLOAT_EQ(createInfo.maxDepthBounds, 1.0f);
}

TEST(VulkanDepthStencilStateTest, AlwaysWithoutWriteDisablesDepthTest)
{
    const pipeline::DepthStencilStateDesc desc(false, pipeline::CompareFunction::Always);
    const gp::RefCountedPtr<VulkanDepthStencilState> state(new VulkanDepthStencilState(desc));
    VkPipelineDepthStencilStateCreateInfo createInfo{};
    state->setupCreateInfo(false, createInfo);

    EXPECT_EQ(createInfo.depthTestEnable, VK_FALSE);
    EXPECT_EQ(createInfo.depthWriteEnable, VK_FALSE);
}

TEST(VulkanDepthStencilStateTest, DepthBoundsComeFromThePipeline)
{
    const gp::RefCountedPtr<VulkanDepthStencilState> state(
        new VulkanDepthStencilState(pipeline::DepthStencilStateDesc{})
    );
    pipeline::GraphicsPipelineStateInitializer initializer;
    initializer.depthBounds = true;
    VkPipelineDepthStencilStateCreateInfo createInfo{};
    state->setupCreateInfo(initializer, createInfo);

    EXPECT_EQ(createInfo.depthBoundsTestEnable, VK_TRUE);
}

TEST(VulkanDepthStencilStateTest, FrontStencilIsMirroredWithoutBackFaceStencil)
{
    const pipeline::DepthStencilStateDesc desc(
        true,
        pipeline::CompareFunction::Greater,
        true,
        pipeline::CompareFunction::Equal,
        pipeline::StencilOp::Zero,
        pipeline::StencilOp::Invert,
        pipeline::StencilOp::Replace,
        false,
        pipeline::CompareFunction::Always,
        pipeline::StencilOp::Keep,
        pipeline::StencilOp::Keep,
        pipeline::StencilOp::Keep,
        0x0Fu,
        0xF0u
    );
    const gp::RefCountedPtr<VulkanDepthStencilState> state(new VulkanDepthStencilState(desc));
    VkPipelineDepthStencilStateCreateInfo createInfo{};
    state->setupCreateInfo(false, createInfo);

    EXPECT_EQ(createInfo.stencilTestEnable, VK_TRUE);
    EXPECT_EQ(createInfo.depthCompareOp, VK_COMPARE_OP_GREATER);
    EXPECT_EQ(createInfo.back.compareOp, VK_COMPARE_OP_EQUAL);
    EXPECT_EQ(createInfo.back.failOp, VK_STENCIL_OP_ZERO);
    EXPECT_EQ(createInfo.back.depthFailOp, VK_STENCIL_OP_INVERT);
    EXPECT_EQ(createInfo.back.passOp, VK_STENCIL_OP_REPLACE);
    EXPECT_EQ(createInfo.back.compareMask, 0x0Fu);
    EXPECT_EQ(createInfo.back.writeMask, 0xF0u);
    EXPECT_EQ(createInfo.back.reference, 0u);
    EXPECT_EQ(createInfo.front.compareOp, createInfo.back.compareOp);
    EXPECT_EQ(createInfo.front.passOp, createInfo.back.passOp);
}

TEST(VulkanDepthStencilStateTest, BackFaceStencilFillsTheOtherFace)
{
    const pipeline::DepthStencilStateDesc desc(
        true,
        pipeline::CompareFunction::LessEqual,
        true,
        pipeline::CompareFunction::Always,
        pipeline::StencilOp::Keep,
        pipeline::StencilOp::Keep,
        pipeline::StencilOp::Increment,
        true,
        pipeline::CompareFunction::NotEqual,
        pipeline::StencilOp::Keep,
        pipeline::StencilOp::Keep,
        pipeline::StencilOp::Decrement
    );
    const gp::RefCountedPtr<VulkanDepthStencilState> state(new VulkanDepthStencilState(desc));
    VkPipelineDepthStencilStateCreateInfo createInfo{};
    state->setupCreateInfo(false, createInfo);

    EXPECT_EQ(createInfo.back.passOp, VK_STENCIL_OP_INCREMENT_AND_WRAP);
    EXPECT_EQ(createInfo.front.passOp, VK_STENCIL_OP_DECREMENT_AND_WRAP);
    EXPECT_EQ(createInfo.front.compareOp, VK_COMPARE_OP_NOT_EQUAL);
}

TEST(VulkanDepthStencilStateTest, InitializerRoundTrips)
{
    const pipeline::DepthStencilStateDesc desc(false, pipeline::CompareFunction::Greater);
    const pipeline::DepthStencilStateRef state = createVulkanDepthStencilState(desc);
    ASSERT_NE(state.get(), nullptr);

    pipeline::DepthStencilStateDesc roundTrip;
    EXPECT_TRUE(state->getInitializer(roundTrip));
    EXPECT_EQ(roundTrip, desc);
}

}   // namespace gp::rhi::vulkan::tests
