// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanRenderQueryRHI.hpp"
#include "../queries/VulkanQueryTestFakes.hpp"
#include "../queries/VulkanQueryTestFixture.hpp"
#include "commands/CommandList.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanSubmission.hpp"
#include "containers/strings/Name.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"
#include "device/DynamicRHI.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "queries/VulkanRenderQuery.hpp"
#include "queries/VulkanRenderQueryPool.hpp"
#include "sync/GPUFence.hpp"      // IWYU pragma: keep
#include "sync/RenderQuery.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "sync/VulkanGPUFence.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

gp::Int32 g_waitCallbackCount{ 0 };

/// @brief Wait callback that completes the waited sync point, standing in for the interrupt queue.
/// @param[in] syncPoint Sync point being waited on.
void signalOnWait(const VulkanSyncPoint& syncPoint) noexcept
{
    ++g_waitCallbackCount;
    const_cast<VulkanSyncPoint&>(syncPoint).signal();
}

}   // namespace

TEST(VulkanRenderQueryRHITest, CreatesQueriesAndPoolsForSupportedTypes)
{
    {
        const sync::RenderQueryRef occlusion = createVulkanRenderQuery(sync::RenderQueryType::Occlusion);
        ASSERT_TRUE(occlusion.isValid());
        EXPECT_EQ(
            static_cast<const VulkanRenderQuery*>(occlusion.get())->getQueryType(), sync::RenderQueryType::Occlusion
        );

        const sync::RenderQueryRef timestamp = createVulkanRenderQuery(sync::RenderQueryType::AbsoluteTime);
        ASSERT_TRUE(timestamp.isValid());
        EXPECT_EQ(
            static_cast<const VulkanRenderQuery*>(timestamp.get())->getQueryType(), sync::RenderQueryType::AbsoluteTime
        );

        // Unsupported types are rejected instead of creating an unusable query.
        EXPECT_FALSE(createVulkanRenderQuery(sync::RenderQueryType::Undefined).isValid());

        const sync::RenderQueryPoolRef pool = createVulkanRenderQueryPool(sync::RenderQueryType::Occlusion, 8u);
        ASSERT_TRUE(pool.isValid());
        EXPECT_EQ(
            static_cast<const VulkanRenderQueryPool*>(pool.get())->getQueryType(), sync::RenderQueryType::Occlusion
        );
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanRenderQueryRHITest, ResultIsUnavailableUntilTheQueryWasUsed)
{
    {
        const sync::RenderQueryRef queryRef = createVulkanRenderQuery(sync::RenderQueryType::Occlusion);
        ASSERT_TRUE(queryRef.isValid());

        gp::UInt64 result = 99u;
        EXPECT_FALSE(getVulkanRenderQueryResult(queryRef.get(), result, true, 1.0f));
        EXPECT_EQ(result, 0u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanRenderQueryRHITest, OcclusionResultIsReturnedOnceTheSyncPointCompletes)
{
    {
        const sync::RenderQueryRef queryRef = createVulkanRenderQuery(sync::RenderQueryType::Occlusion);
        ASSERT_TRUE(queryRef.isValid());
        VulkanRenderQuery* const query = static_cast<VulkanRenderQuery*>(queryRef.get());
        query->syncPoint = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "OcclusionTest");
        query->result = 42u;

        gp::UInt64 result = 0u;
        EXPECT_FALSE(getVulkanRenderQueryResult(queryRef.get(), result, false, 1.0f));

        query->syncPoint->signal();
        ASSERT_TRUE(getVulkanRenderQueryResult(queryRef.get(), result, false, 1.0f));
        EXPECT_EQ(result, 42u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanRenderQueryRHITest, TimestampResultIsConvertedToMicrosecondsAfterWaiting)
{
    {
        const sync::RenderQueryRef queryRef = createVulkanRenderQuery(sync::RenderQueryType::AbsoluteTime);
        ASSERT_TRUE(queryRef.isValid());
        VulkanRenderQuery* const query = static_cast<VulkanRenderQuery*>(queryRef.get());
        query->syncPoint = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "TimestampTest");
        query->result = 3'000'000u;

        g_waitCallbackCount = 0;
        VulkanSyncPoint::setWaitCallback(&signalOnWait);
        gp::UInt64 result = 0u;
        const bool hasResult = getVulkanRenderQueryResult(queryRef.get(), result, true, 1.0f);
        VulkanSyncPoint::setWaitCallback(nullptr);

        ASSERT_TRUE(hasResult);
        EXPECT_EQ(g_waitCallbackCount, 1);
        EXPECT_EQ(result, 3'000u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanRenderQueryRHITest, TopOfPipeQueriesAreRecordedForLaterReplay)
{
    {
        const sync::RenderQueryRef queryRef = createVulkanRenderQuery(sync::RenderQueryType::Occlusion);
        ASSERT_TRUE(queryRef.isValid());
        const VulkanRenderQuery* const query = static_cast<const VulkanRenderQuery*>(queryRef.get());
        FakeSyncContextProvider provider{};

        command::CommandList cmdList{};
        ASSERT_TRUE(cmdList.isTopOfPipe());

        beginVulkanRenderQueryTopOfPipe(cmdList, queryRef.get());
        EXPECT_TRUE(cmdList.hasCommands());
        EXPECT_FALSE(query->syncPoint.isValid());

        endVulkanRenderQueryTopOfPipe(cmdList, provider, queryRef.get());

        // The sync point is created when the end is recorded, but only registered on replay.
        ASSERT_TRUE(query->syncPoint.isValid());
        EXPECT_FALSE(query->syncPoint->isComplete());
        EXPECT_EQ(query->syncPoint->getType(), VulkanSyncPointType::GPUAndCPU);
        EXPECT_EQ(provider.pendingSyncPointCount, 0u);

        // Every end issues a fresh sync point.
        const VulkanSyncPoint* const firstSyncPoint = query->syncPoint.get();
        endVulkanRenderQueryTopOfPipe(cmdList, provider, queryRef.get());
        EXPECT_NE(query->syncPoint.get(), firstSyncPoint);

        cmdList.reset();
        EXPECT_FALSE(cmdList.hasCommands());
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanRenderQueryRHITest, GPUFenceWithoutEventSignalsThroughTheContext)
{
    if (getDynamicRHI() != nullptr)
    {
        GTEST_SKIP() << "Replaying a deferred command list needs no active dynamic RHI.";
    }

    {
        VulkanDevice nullDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false);
        VulkanGPUFence* const rawFence = new VulkanGPUFence(nullDevice, gp::Name("QueryTestFence"), true);
        const sync::GPUFenceRef fenceRef(rawFence);
        ASSERT_EQ(rawFence->getEvent(), VK_NULL_HANDLE);
        FakeSyncContextProvider provider{};

        command::CommandList cmdList{};
        writeVulkanGPUFenceTopOfPipe(cmdList, provider, rawFence);

        // Only the completion sync point exists without an event.
        ASSERT_TRUE(rawFence->getCompletedSyncPoint().isValid());
        EXPECT_FALSE(rawFence->getSubmittedSyncPoint().isValid());
        EXPECT_EQ(rawFence->getNumPendingWriteCommands(), 1);
        EXPECT_EQ(provider.signalSyncPointCount, 0u);

        cmdList.execute();

        EXPECT_EQ(provider.signalSyncPointCount, 1u);
        EXPECT_EQ(provider.lastSignaledSyncPoint, rawFence->getCompletedSyncPoint());
        EXPECT_EQ(provider.pendingSyncPointCount, 0u);
        EXPECT_EQ(provider.submissionEventCount, 0u);
        EXPECT_EQ(provider.activeCommandBufferRequests, 0u);
        EXPECT_EQ(rawFence->getNumPendingWriteCommands(), 0);

        rawFence->clear();
        provider.lastSignaledSyncPoint = nullptr;
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

/// @brief Live variant that records the fence event into a real command buffer.
class VulkanRenderQueryRHILiveTest : public VulkanQueryLiveTest
{};

TEST_F(VulkanRenderQueryRHILiveTest, GPUFenceWithEventIsSetByTheCommandBuffer)
{
    if (getDynamicRHI() != nullptr)
    {
        GTEST_SKIP() << "Replaying a deferred command list needs no active dynamic RHI.";
    }

    VulkanGPUFence* const rawFence = new VulkanGPUFence(*m_device, gp::Name("QueryTestEventFence"), false);
    sync::GPUFenceRef fenceRef(rawFence);
    if (rawFence->getEvent() == VK_NULL_HANDLE)
    {
        GTEST_SKIP() << "vkCreateEvent unavailable; skipping event fence test.";
    }

    VulkanCommandBuffer* const cmdBuffer = beginCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);

    FakeSyncContextProvider provider{};
    provider.activeCommandBuffer = cmdBuffer->getHandle();

    command::CommandList cmdList{};
    writeVulkanGPUFenceTopOfPipe(cmdList, provider, rawFence);
    ASSERT_TRUE(rawFence->getSubmittedSyncPoint().isValid());
    ASSERT_TRUE(rawFence->getCompletedSyncPoint().isValid());
    cmdList.execute();

    EXPECT_EQ(provider.activeCommandBufferRequests, 1u);
    EXPECT_EQ(provider.submissionEventCount, 1u);
    EXPECT_EQ(provider.lastSubmissionEvent, rawFence->getSubmittedSyncPoint());
    EXPECT_EQ(provider.pendingSyncPointCount, 1u);
    EXPECT_EQ(provider.lastPendingSyncPoint, rawFence->getCompletedSyncPoint());
    EXPECT_EQ(provider.signalSyncPointCount, 0u);
    EXPECT_EQ(rawFence->getNumPendingWriteCommands(), 0);

    // Stand in for the context: the payload signals the registered points.
    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    payload->submissionEvents.pushBack(provider.lastSubmissionEvent);
    payload->syncPoints.pushBack(provider.lastPendingSyncPoint);
    ASSERT_TRUE(submitAndWait(cmdBuffer, payload));

    EXPECT_TRUE(rawFence->getSubmittedSyncPoint()->isComplete());
    EXPECT_TRUE(rawFence->getCompletedSyncPoint()->isComplete());
    EXPECT_EQ(vkGetEventStatus(m_device->getHandle(), rawFence->getEvent()), VK_EVENT_SET);

    provider.lastSubmissionEvent = nullptr;
    provider.lastPendingSyncPoint = nullptr;
    rawFence->clear();
    fenceRef = nullptr;
}

}   // namespace gp::rhi::vulkan::tests
