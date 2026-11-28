// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanRasterizerState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "states/VulkanStateRHI.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanRasterizerStateTest, ResetCreateInfoUsesClockwiseFrontAndUnitLineWidth)
{
    VkPipelineRasterizationStateCreateInfo createInfo{};
    createInfo.polygonMode = VK_POLYGON_MODE_LINE;
    VulkanRasterizerState::resetCreateInfo(createInfo);

    EXPECT_EQ(createInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    EXPECT_EQ(createInfo.pNext, nullptr);
    EXPECT_EQ(createInfo.frontFace, VK_FRONT_FACE_CLOCKWISE);
    EXPECT_FLOAT_EQ(createInfo.lineWidth, 1.0f);
    EXPECT_EQ(createInfo.polygonMode, VK_POLYGON_MODE_FILL);
    EXPECT_EQ(createInfo.depthBiasEnable, VK_FALSE);
}

TEST(VulkanRasterizerStateTest, TranslatesFillAndCullModes)
{
    const pipeline::RasterizerStateDesc desc(
        pipeline::RasterizerFillMode::Wireframe, pipeline::RasterizerCullMode::CCW, false
    );
    const gp::RefCountedPtr<VulkanRasterizerState> state(new VulkanRasterizerState(desc));

    const VkPipelineRasterizationStateCreateInfo& createInfo = state->getCreateInfo();
    EXPECT_EQ(createInfo.polygonMode, VK_POLYGON_MODE_LINE);
    EXPECT_EQ(createInfo.cullMode, static_cast<VkCullModeFlags>(VK_CULL_MODE_BACK_BIT));
    EXPECT_EQ(createInfo.depthClampEnable, VK_FALSE);
    EXPECT_EQ(createInfo.depthBiasEnable, VK_FALSE);
}

TEST(VulkanRasterizerStateTest, ClockwiseCullingCullsFrontFaces)
{
    const pipeline::RasterizerStateDesc desc(
        pipeline::RasterizerFillMode::Solid, pipeline::RasterizerCullMode::CW, false
    );
    const gp::RefCountedPtr<VulkanRasterizerState> state(new VulkanRasterizerState(desc));

    EXPECT_EQ(state->getCreateInfo().polygonMode, VK_POLYGON_MODE_FILL);
    EXPECT_EQ(state->getCreateInfo().cullMode, static_cast<VkCullModeFlags>(VK_CULL_MODE_FRONT_BIT));
}

TEST(VulkanRasterizerStateTest, DepthBiasAndClampAreForwarded)
{
    const pipeline::RasterizerStateDesc desc(
        pipeline::RasterizerFillMode::Solid,
        pipeline::RasterizerCullMode::None,
        0.25f,
        2.0f,
        pipeline::RasterizerDepthClipMode::DepthClamp,
        true
    );
    const gp::RefCountedPtr<VulkanRasterizerState> state(new VulkanRasterizerState(desc));

    const VkPipelineRasterizationStateCreateInfo& createInfo = state->getCreateInfo();
    EXPECT_EQ(createInfo.depthBiasEnable, VK_TRUE);
    EXPECT_FLOAT_EQ(createInfo.depthBiasConstantFactor, 0.25f);
    EXPECT_FLOAT_EQ(createInfo.depthBiasSlopeFactor, 2.0f);
    EXPECT_EQ(createInfo.depthClampEnable, VK_TRUE);
    EXPECT_EQ(createInfo.cullMode, static_cast<VkCullModeFlags>(VK_CULL_MODE_NONE));
}

TEST(VulkanRasterizerStateTest, InitializerRoundTrips)
{
    const pipeline::RasterizerStateDesc desc(
        pipeline::RasterizerFillMode::Solid,
        pipeline::RasterizerCullMode::CCW,
        0.5f,
        1.0f,
        pipeline::RasterizerDepthClipMode::DepthClip,
        true
    );
    const pipeline::RasterizerStateRef state = createVulkanRasterizerState(desc);
    ASSERT_NE(state.get(), nullptr);

    pipeline::RasterizerStateDesc roundTrip;
    EXPECT_TRUE(state->getInitializer(roundTrip));
    EXPECT_EQ(roundTrip, desc);
}

}   // namespace gp::rhi::vulkan::tests
