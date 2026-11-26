// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQueryRecording.hpp"
#include "../queries/VulkanQueryTestFixture.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanSubmission.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "queries/VulkanQueryEnums.hpp"
#include "queries/VulkanQueryPool.hpp"
#include "queries/VulkanQueryPoolManager.hpp"
#include "queries/VulkanQueryTypes.hpp"
#include "queries/VulkanRenderQuery.hpp"
#include "queries/VulkanRenderQueryRHI.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Live fixture that records queries into real command buffers.
class VulkanQueryRecordingLiveTest : public VulkanQueryLiveTest
{
protected:
    /// @brief Creates a query with a fresh completion sync point, as the top-of-pipe end does.
    /// @param[in] queryType Query type.
    /// @return New query.
    [[nodiscard]] static gp::RefCountedPtr<VulkanRenderQuery>
        createUsedQuery(const sync::RenderQueryType queryType) noexcept
    {
        gp::RefCountedPtr<VulkanRenderQuery> query(new VulkanRenderQuery(queryType));
        query->syncPoint = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "RecordingTestQuery");
        return query;
    }
};

TEST_F(VulkanQueryRecordingLiveTest, TimestampsResolveWhenThePayloadCompletes)
{
    if (!supportsTimestamps())
    {
        GTEST_SKIP() << "Graphics queues do not support timestamps; skipping timestamp test.";
    }

    const gp::RefCountedPtr<VulkanRenderQuery> start = createUsedQuery(sync::RenderQueryType::AbsoluteTime);
    const gp::RefCountedPtr<VulkanRenderQuery> end = createUsedQuery(sync::RenderQueryType::AbsoluteTime);
    end->syncPoint = start->syncPoint;

    VulkanCommandBuffer* const cmdBuffer = beginCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);

    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    payload->queryPoolManager = m_queryPoolManager;
    recordVulkanEndRenderQuery(*m_queryPoolManager, payload->queryPools, cmdBuffer->getHandle(), *start, 0u);
    recordVulkanEndRenderQuery(*m_queryPoolManager, payload->queryPools, cmdBuffer->getHandle(), *end, 0u);

    // Both timestamps share the payload's single timestamp pool.
    ASSERT_EQ(payload->queryPools.get(VulkanQueryPoolType::Timestamp).size(), 1u);
    EXPECT_TRUE(payload->queryPools.get(VulkanQueryPoolType::Occlusion).isEmpty());
    EXPECT_EQ(start->indexInPool, 0u);
    EXPECT_EQ(end->indexInPool, 1u);

    const VulkanSyncPointRef syncPoint = start->syncPoint;
    payload->syncPoints.pushBack(syncPoint);
    ASSERT_TRUE(submitAndWait(cmdBuffer, payload));

    // Completion resolved the results and returned the pool to the cache.
    EXPECT_TRUE(syncPoint->isComplete());
    EXPECT_EQ(m_queryPoolManager->getFreePoolCount(VulkanQueryPoolType::Timestamp), 1u);
    EXPECT_GE(end->result, start->result);

    gp::UInt64 startMicroseconds = 0u;
    gp::UInt64 endMicroseconds = 0u;
    const gp::Float32 period = m_device->getLimits().timestampPeriod;
    ASSERT_TRUE(getVulkanRenderQueryResult(start.get(), startMicroseconds, false, period));
    ASSERT_TRUE(getVulkanRenderQueryResult(end.get(), endMicroseconds, false, period));
    EXPECT_GE(endMicroseconds, startMicroseconds);
}

TEST_F(VulkanQueryRecordingLiveTest, OcclusionQueriesWithoutDrawsReportNoSamples)
{
    const gp::RefCountedPtr<VulkanRenderQuery> first = createUsedQuery(sync::RenderQueryType::Occlusion);
    const gp::RefCountedPtr<VulkanRenderQuery> second = createUsedQuery(sync::RenderQueryType::Occlusion);
    first->result = 77u;
    second->result = 77u;

    VulkanCommandBuffer* const cmdBuffer = beginCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);
    const VkCommandBuffer handle = cmdBuffer->getHandle();

    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    payload->queryPoolManager = m_queryPoolManager;
    beginVulkanOcclusionQueryBatch(*m_queryPoolManager, payload->queryPools, handle, 2u, 0u);
    ASSERT_EQ(payload->queryPools.get(VulkanQueryPoolType::Occlusion).size(), 1u);

    VulkanQueryPool* const pool = getCurrentVulkanOcclusionQueryPool(payload->queryPools);
    ASSERT_NE(pool, nullptr);
    EXPECT_EQ(pool, payload->queryPools.get(VulkanQueryPoolType::Occlusion)[0]);
    EXPECT_FALSE(pool->isDirty());
    EXPECT_EQ(pool->getQueryStride(), 1u);

    recordVulkanBeginRenderQuery(payload->queryPools, handle, *first);
    recordVulkanEndRenderQuery(*m_queryPoolManager, payload->queryPools, handle, *first, 0u);
    recordVulkanBeginRenderQuery(payload->queryPools, handle, *second);
    recordVulkanEndRenderQuery(*m_queryPoolManager, payload->queryPools, handle, *second, 0u);
    EXPECT_EQ(first->indexInPool, 0u);
    EXPECT_EQ(second->indexInPool, 1u);
    EXPECT_EQ(pool->getCurrentQueryCount(), 2u);

    payload->syncPoints.pushBack(first->syncPoint);
    payload->syncPoints.pushBack(second->syncPoint);
    ASSERT_TRUE(submitAndWait(cmdBuffer, payload));

    EXPECT_TRUE(first->syncPoint->isComplete());
    EXPECT_TRUE(second->syncPoint->isComplete());
    EXPECT_EQ(first->result, 0u);
    EXPECT_EQ(second->result, 0u);
    EXPECT_EQ(m_queryPoolManager->getFreePoolCount(VulkanQueryPoolType::Occlusion), 1u);

    gp::UInt64 samples = 1u;
    ASSERT_TRUE(getVulkanRenderQueryResult(first.get(), samples, false, 1.0f));
    EXPECT_EQ(samples, 0u);
}

TEST_F(VulkanQueryRecordingLiveTest, TimestampPoolChangesWhenTheViewCountChanges)
{
    VulkanCommandBuffer* const cmdBuffer = beginCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);
    const VkCommandBuffer handle = cmdBuffer->getHandle();

    VulkanPayloadQueryPools pools{};
    VulkanQueryPool* const singleView = getCurrentVulkanTimestampQueryPool(*m_queryPoolManager, pools, handle, 0u);
    ASSERT_NE(singleView, nullptr);
    EXPECT_EQ(singleView->getQueryStride(), 1u);
    EXPECT_FALSE(singleView->isDirty());

    // The same view count keeps the current pool.
    EXPECT_EQ(getCurrentVulkanTimestampQueryPool(*m_queryPoolManager, pools, handle, 1u), singleView);

    VulkanQueryPool* const multiView = getCurrentVulkanTimestampQueryPool(*m_queryPoolManager, pools, handle, 2u);
    ASSERT_NE(multiView, nullptr);
    EXPECT_NE(multiView, singleView);
    EXPECT_EQ(multiView->getQueryStride(), 2u);
    EXPECT_EQ(pools.get(VulkanQueryPoolType::Timestamp).size(), 2u);

    // Pools that were never read back go back to the cache unresolved.
    releaseVulkanPayloadQueries(*m_queryPoolManager, pools);
    EXPECT_TRUE(pools.isEmpty());
    EXPECT_EQ(m_queryPoolManager->getFreePoolCount(VulkanQueryPoolType::Timestamp), 2u);
    EXPECT_TRUE(singleView->isDirty());

    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    EXPECT_TRUE(submitAndWait(cmdBuffer, payload));
}

TEST_F(VulkanQueryRecordingLiveTest, FullTimestampPoolRollsOverToANewPool)
{
    VulkanCommandBuffer* const cmdBuffer = beginCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);
    const VkCommandBuffer handle = cmdBuffer->getHandle();

    VulkanPayloadQueryPools pools{};
    VulkanQueryPool* const firstPool = getCurrentVulkanTimestampQueryPool(*m_queryPoolManager, pools, handle, 0u);
    ASSERT_NE(firstPool, nullptr);
    gp::UInt64 sink = 0u;
    while (!firstPool->isFull())
    {
        static_cast<void>(firstPool->reserveQuery(&sink));
    }

    VulkanQueryPool* const secondPool = getCurrentVulkanTimestampQueryPool(*m_queryPoolManager, pools, handle, 0u);
    EXPECT_NE(secondPool, firstPool);
    EXPECT_EQ(pools.get(VulkanQueryPoolType::Timestamp).size(), 2u);

    releaseVulkanPayloadQueries(*m_queryPoolManager, pools);
    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    EXPECT_TRUE(submitAndWait(cmdBuffer, payload));
}

TEST_F(VulkanQueryRecordingLiveTest, MergedPayloadsCarryTheirQueryPools)
{
    VulkanPayload* const target = new VulkanPayload(*m_queue);
    VulkanPayload source(*m_queue);
    source.queryPoolManager = m_queryPoolManager;
    source.queryPools.get(VulkanQueryPoolType::Timestamp).pushBack(m_queryPoolManager->acquireTimingQueryPool());

    target->merge(source);
    EXPECT_EQ(target->queryPoolManager, m_queryPoolManager);
    EXPECT_EQ(target->queryPools.get(VulkanQueryPoolType::Timestamp).size(), 1u);
    EXPECT_TRUE(source.queryPools.isEmpty());

    // Destroying a payload that never completed hands its pools back to the cache.
    delete target;
    EXPECT_EQ(m_queryPoolManager->getFreePoolCount(VulkanQueryPoolType::Timestamp), 1u);
}

}   // namespace gp::rhi::vulkan::tests
