// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanContextCommon.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanSubmission.hpp"
#include "contexts/VulkanContextDesc.hpp"
#include "contexts/VulkanContextEnums.hpp"
#include "CoreMinimal.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanContextTestFixture.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanContextEnumsTest, FlushFlagsCombineAsBitmask)
{
    const VulkanFlushFlags flags = VulkanFlushFlags::WaitForSubmission | VulkanFlushFlags::WaitForCompletion;
    EXPECT_NE(flags & VulkanFlushFlags::WaitForSubmission, VulkanFlushFlags::None);
    EXPECT_NE(flags & VulkanFlushFlags::WaitForCompletion, VulkanFlushFlags::None);
    EXPECT_EQ(VulkanFlushFlags::WaitForSubmission & VulkanFlushFlags::WaitForCompletion, VulkanFlushFlags::None);
}

TEST(VulkanContextEnumsTest, PhasesAreOrderedLikeASubmission)
{
    EXPECT_LT(VulkanContextPhase::Wait, VulkanContextPhase::UpdateReservedResources);
    EXPECT_LT(VulkanContextPhase::UpdateReservedResources, VulkanContextPhase::Execute);
    EXPECT_LT(VulkanContextPhase::Execute, VulkanContextPhase::Signal);
}

class VulkanContextCommonLiveTest : public VulkanContextLiveTest
{};

TEST_F(VulkanContextCommonLiveTest, NewContextHasNoPayloads)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    EXPECT_EQ(context.getNumPayloads(), 0u);
    EXPECT_FALSE(context.hasActiveCommandBuffer());
    EXPECT_EQ(context.getCurrentPhase(), VulkanContextPhase::Wait);
    EXPECT_EQ(context.getCommandBufferType(), VulkanCommandBufferType::Primary);
    EXPECT_EQ(&context.getDevice(), m_device);
    EXPECT_EQ(&context.getQueue(), m_queue);
}

TEST_F(VulkanContextCommonLiveTest, LaterPhasesShareThePayloadEarlierPhasesOpenANewOne)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);

    VulkanPayload& execute = context.getPayload(VulkanContextPhase::Execute);
    EXPECT_EQ(context.getNumPayloads(), 1u);

    VulkanPayload& signal = context.getPayload(VulkanContextPhase::Signal);
    EXPECT_EQ(&signal, &execute);
    EXPECT_EQ(context.getNumPayloads(), 1u);

    VulkanPayload& wait = context.getPayload(VulkanContextPhase::Wait);
    EXPECT_NE(&wait, &execute);
    EXPECT_EQ(context.getNumPayloads(), 2u);
    EXPECT_EQ(context.getCurrentPhase(), VulkanContextPhase::Wait);
    EXPECT_EQ(&wait.queue, m_queue);
}

TEST_F(VulkanContextCommonLiveTest, CommandBufferIsReusedWhileRecording)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);

    VulkanCommandBuffer* const first = context.getCommandBuffer();
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(first->hasBegun());
    EXPECT_TRUE(context.hasActiveCommandBuffer());
    EXPECT_EQ(context.getCommandBuffer(), first);
}

TEST_F(VulkanContextCommonLiveTest, NewPayloadEndsThePreviousCommandBuffer)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);

    VulkanCommandBuffer* const first = context.getCommandBuffer();
    ASSERT_NE(first, nullptr);
    context.newPayload();
    EXPECT_TRUE(first->hasEnded());
    EXPECT_FALSE(context.hasActiveCommandBuffer());

    VulkanCommandBuffer* const second = context.getCommandBuffer();
    ASSERT_NE(second, nullptr);
    EXPECT_NE(second, first);
    EXPECT_EQ(context.getNumPayloads(), 2u);
}

TEST_F(VulkanContextCommonLiveTest, FinalizeHandsOverEndedPayloads)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    VulkanCommandBuffer* const commandBuffer = context.getCommandBuffer();
    ASSERT_NE(commandBuffer, nullptr);

    VulkanPlatformCommandList commandList{};
    context.finalize(commandList);

    EXPECT_EQ(context.getNumPayloads(), 0u);
    EXPECT_EQ(context.getCurrentPhase(), VulkanContextPhase::Wait);
    ASSERT_EQ(commandList.payloads.size(), 1u);
    ASSERT_EQ(commandList.payloads[0]->commandBuffers.size(), 1u);
    EXPECT_EQ(commandList.payloads[0]->commandBuffers[0], commandBuffer);
    EXPECT_TRUE(commandBuffer->hasEnded());

    discardUnsubmitted(commandList);
}

TEST_F(VulkanContextCommonLiveTest, FinalizeOfAnIdleContextProducesNothing)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    VulkanPlatformCommandList commandList{};
    context.finalize(commandList);
    EXPECT_TRUE(commandList.payloads.isEmpty());
}

TEST_F(VulkanContextCommonLiveTest, ContextSyncPointIsStableUntilFinalize)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);

    const VulkanSyncPointRef first = context.getContextSyncPoint();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->getType(), VulkanSyncPointType::Context);
    EXPECT_EQ(context.getContextSyncPoint().get(), first.get());

    VulkanPlatformCommandList commandList{};
    context.finalize(commandList);
    ASSERT_EQ(commandList.payloads.size(), 1u);
    ASSERT_EQ(commandList.payloads[0]->syncPoints.size(), 1u);
    EXPECT_EQ(commandList.payloads[0]->syncPoints[0].get(), first.get());

    EXPECT_NE(context.getContextSyncPoint().get(), first.get());
    discardUnsubmitted(commandList);
}

TEST_F(VulkanContextCommonLiveTest, PendingSyncPointsMoveToTheSignalPhase)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    context.addPendingSyncPoint(VulkanSyncPoint::create(VulkanSyncPointType::Context, "PendingA"));
    context.addPendingSyncPoint(VulkanSyncPoint::create(VulkanSyncPointType::Context, "PendingB"));
    context.addPendingSyncPoint(VulkanSyncPointRef{});
    EXPECT_EQ(context.getNumPendingSyncPoints(), 2u);

    context.flushPendingSyncPoints();
    EXPECT_EQ(context.getNumPendingSyncPoints(), 0u);
    EXPECT_EQ(context.getCurrentPhase(), VulkanContextPhase::Signal);
    EXPECT_EQ(context.getPayload(VulkanContextPhase::Signal).syncPoints.size(), 2u);
}

TEST_F(VulkanContextCommonLiveTest, NullSyncPointsAndSemaphoresAreIgnored)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    context.signalSyncPoint(VulkanSyncPointRef{});
    context.addSubmissionEvent(VulkanSyncPointRef{});
    context.addWaitSemaphore(VK_PIPELINE_STAGE_TRANSFER_BIT, VulkanSemaphoreRef{});
    context.addSignalSemaphore(VulkanSemaphoreRef{});
    EXPECT_EQ(context.getNumPayloads(), 0u);
}

TEST_F(VulkanContextCommonLiveTest, SubmissionEventsJoinTheCurrentPayload)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    VulkanPayload& payload = context.getPayload(VulkanContextPhase::Execute);
    context.addSubmissionEvent(VulkanSyncPoint::create(VulkanSyncPointType::Context, "Event"));
    EXPECT_EQ(payload.submissionEvents.size(), 1u);
    EXPECT_EQ(context.getNumPayloads(), 1u);
}

TEST_F(VulkanContextCommonLiveTest, FlushWithCompletionWaitsForTheGPU)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    ASSERT_NE(context.getCommandBuffer(), nullptr);

    const VulkanSyncPointRef signal = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "Signal");
    context.signalSyncPoint(signal);

    EXPECT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));
    EXPECT_TRUE(signal->isComplete());
    EXPECT_EQ(context.getNumPayloads(), 0u);
    EXPECT_GE(m_submitter->getNumSubmittedPayloads(), 1u);
    EXPECT_EQ(m_submitter->getNumKnownQueues(), 1u);
}

TEST_F(VulkanContextCommonLiveTest, FlushWithoutSubmitterFails)
{
    VulkanContextServices services = m_contextServices;
    services.submitter = nullptr;
    VulkanContextCommon context(services, VulkanCommandBufferType::Primary);
    EXPECT_FALSE(context.flushCommands(VulkanFlushFlags::None));
}

TEST_F(VulkanContextCommonLiveTest, ContextCanBeFlushedRepeatedly)
{
    VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
    for (gp::UInt32 iteration = 0u; iteration < 3u; ++iteration)
    {
        ASSERT_NE(context.getCommandBuffer(), nullptr);
        EXPECT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));
    }
}

TEST_F(VulkanContextCommonLiveTest, DestroyingARecordingContextDiscardsItsPayloads)
{
    {
        VulkanContextCommon context(m_contextServices, VulkanCommandBufferType::Primary);
        ASSERT_NE(context.getCommandBuffer(), nullptr);
        context.addPendingSyncPoint(VulkanSyncPoint::create(VulkanSyncPointType::Context, "Dropped"));
    }

    // The pool must still hand out a ready command buffer afterwards.
    VulkanContextCommon next(m_contextServices, VulkanCommandBufferType::Primary);
    EXPECT_NE(next.getCommandBuffer(), nullptr);
}

TEST_F(VulkanContextCommonLiveTest, ImmediateSubmitterIgnoresEmptyBatches)
{
    gp::Vector<VulkanPayload*> payloads{};
    m_submitter->submitPayloads(payloads);
    EXPECT_EQ(m_submitter->getNumSubmittedPayloads(), 0u);
    EXPECT_EQ(m_submitter->getNumKnownQueues(), 0u);
}

TEST_F(VulkanContextCommonLiveTest, ImmediateSubmitterWithoutQueuesReturnsCurrentCompletion)
{
    const VulkanSyncPointRef syncPoint = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "Never");
    EXPECT_FALSE(m_submitter->waitForSyncPoint(*syncPoint, 1'000'000ULL));
}

}   // namespace gp::rhi::vulkan::tests
