// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanPendingComputeState.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanSubmission.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "pendingstate/VulkanComputePipelineDescriptorState.hpp"
#include "pipeline/ComputePipelineState.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanComputePipelineState.hpp"
#include "VulkanPendingStateLiveFixture.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST_F(VulkanPendingStateLiveTest, ComputeDispatchWithoutAPipelineIsRejected)
{
    VulkanCommandBuffer* const commandBuffer = beginCommandBuffer();
    ASSERT_NE(commandBuffer, nullptr);
    {
        VulkanPendingComputeState state(*m_device, *m_context, m_defaults);

        EXPECT_EQ(state.getCurrentPipeline(), nullptr);
        EXPECT_EQ(state.getNumPipelineStates(), 0u);
        EXPECT_FALSE(state.prepareForDispatch(*commandBuffer));
    }
    EXPECT_TRUE(submitAndWait(commandBuffer, new VulkanPayload(*m_queue)));
}

TEST_F(VulkanPendingStateLiveTest, ComputeDispatchBindsThePipeline)
{
    const pipeline::ComputePipelineStateRef pipelineRef = createComputePipeline();
    ASSERT_TRUE(pipelineRef.isValid());
    const VulkanComputePipelineState& pipelineState = *getInnerState(pipelineRef);
    VulkanCommandBuffer* const commandBuffer = beginCommandBuffer();
    ASSERT_NE(commandBuffer, nullptr);
    {
        VulkanPendingComputeState state(*m_device, *m_context, m_defaults);
        state.setComputePipeline(pipelineState);

        ASSERT_EQ(state.getCurrentPipeline(), &pipelineState);
        ASSERT_NE(state.getCurrentState(), nullptr);
        EXPECT_EQ(state.getCurrentState()->getPipeline(), &pipelineState);
        EXPECT_EQ(state.getCurrentState()->getBindPoint(), VK_PIPELINE_BIND_POINT_COMPUTE);
        // The test shader has no binding: nothing to allocate, the dispatch still goes through.
        EXPECT_TRUE(state.prepareForDispatch(*commandBuffer));
        EXPECT_EQ(m_context->numAcquireCalls, 0u);
        EXPECT_TRUE(state.prepareForDispatch(*commandBuffer));
    }
    EXPECT_TRUE(submitAndWait(commandBuffer, new VulkanPayload(*m_queue)));
}

TEST_F(VulkanPendingStateLiveTest, ComputePipelineStatesAreReusedUntilDeleted)
{
    const pipeline::ComputePipelineStateRef pipelineRef = createComputePipeline();
    ASSERT_TRUE(pipelineRef.isValid());
    const VulkanComputePipelineState& pipelineState = *getInnerState(pipelineRef);
    VulkanPendingComputeState state(*m_device, *m_context, m_defaults);

    state.setComputePipeline(pipelineState);
    VulkanComputePipelineDescriptorState* const descriptorState = state.getCurrentState();
    state.setComputePipeline(pipelineState);
    EXPECT_EQ(state.getNumPipelineStates(), 1u);

    state.reset();
    EXPECT_EQ(state.getCurrentPipeline(), nullptr);
    EXPECT_EQ(state.getCurrentState(), nullptr);
    state.setComputePipeline(pipelineState);
    EXPECT_EQ(state.getCurrentState(), descriptorState);
    EXPECT_EQ(state.getNumPipelineStates(), 1u);

    state.notifyDeletedPipeline(pipelineState);
    EXPECT_EQ(state.getCurrentPipeline(), nullptr);
    EXPECT_EQ(state.getNumPipelineStates(), 0u);

    // Unknown pipelines are ignored.
    state.notifyDeletedPipeline(pipelineState);
    EXPECT_EQ(state.getNumPipelineStates(), 0u);
}

}   // namespace gp::rhi::vulkan::tests
