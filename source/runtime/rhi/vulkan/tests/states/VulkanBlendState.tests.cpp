// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanBlendState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "RHIDefinitions.hpp"
#include "states/VulkanStateRHI.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanBlendStateTest, OpaqueEquationDisablesBlending)
{
    const VkPipelineColorBlendAttachmentState state =
        VulkanBlendState::translateRenderTarget(pipeline::RenderTargetBlendDesc{});

    EXPECT_EQ(state.blendEnable, VK_FALSE);
    EXPECT_EQ(state.srcColorBlendFactor, VK_BLEND_FACTOR_ONE);
    EXPECT_EQ(state.dstColorBlendFactor, VK_BLEND_FACTOR_ZERO);
    EXPECT_EQ(state.colorBlendOp, VK_BLEND_OP_ADD);
    EXPECT_EQ(
        state.colorWriteMask,
        static_cast<VkColorComponentFlags>(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        )
    );
}

TEST(VulkanBlendStateTest, AlphaBlendingEnablesBlending)
{
    const pipeline::RenderTargetBlendDesc desc(
        pipeline::BlendOperation::Add,
        pipeline::BlendFactor::SourceAlpha,
        pipeline::BlendFactor::InverseSourceAlpha,
        pipeline::BlendOperation::Max,
        pipeline::BlendFactor::One,
        pipeline::BlendFactor::One,
        pipeline::ColorWriteMask::RGB
    );
    const VkPipelineColorBlendAttachmentState state = VulkanBlendState::translateRenderTarget(desc);

    EXPECT_EQ(state.blendEnable, VK_TRUE);
    EXPECT_EQ(state.srcColorBlendFactor, VK_BLEND_FACTOR_SRC_ALPHA);
    EXPECT_EQ(state.dstColorBlendFactor, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    EXPECT_EQ(state.alphaBlendOp, VK_BLEND_OP_MAX);
    EXPECT_EQ(state.srcAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
    EXPECT_EQ(state.dstAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
    EXPECT_EQ(
        state.colorWriteMask,
        static_cast<VkColorComponentFlags>(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
        )
    );
}

TEST(VulkanBlendStateTest, EveryRenderTargetIsTranslated)
{
    pipeline::BlendStateDesc desc;
    desc.useIndependentRenderTargetBlendStates = true;
    desc.renderTargets[1] = pipeline::RenderTargetBlendDesc(
        pipeline::BlendOperation::Add, pipeline::BlendFactor::One, pipeline::BlendFactor::One
    );
    desc.renderTargets[kMaxSimultaneousRenderTargets - 1u].colorWriteMask = pipeline::ColorWriteMask::None;
    const gp::RefCountedPtr<VulkanBlendState> state(new VulkanBlendState(desc));

    EXPECT_EQ(state->getAttachmentState(0u).blendEnable, VK_FALSE);
    EXPECT_EQ(state->getAttachmentState(1u).blendEnable, VK_TRUE);
    EXPECT_EQ(state->getAttachmentState(1u).dstColorBlendFactor, VK_BLEND_FACTOR_ONE);
    EXPECT_EQ(state->getAttachmentState(kMaxSimultaneousRenderTargets - 1u).colorWriteMask, 0u);
    EXPECT_EQ(state->getAttachmentStates(), &state->getAttachmentState(0u));
}

TEST(VulkanBlendStateTest, InitializerRoundTrips)
{
    pipeline::BlendStateDesc desc(pipeline::RenderTargetBlendDesc(pipeline::BlendOperation::Subtract), true);
    const pipeline::BlendStateRef state = createVulkanBlendState(desc);
    ASSERT_NE(state.get(), nullptr);

    pipeline::BlendStateDesc roundTrip;
    EXPECT_TRUE(state->getInitializer(roundTrip));
    EXPECT_TRUE(roundTrip.useAlphaToCoverage);
    EXPECT_EQ(roundTrip.renderTargets[0], desc.renderTargets[0]);
}

}   // namespace gp::rhi::vulkan::tests
