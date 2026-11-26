// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQueryPoolManager.hpp"
#include "../queries/VulkanQueryTestFixture.hpp"
#include "CoreMinimal.hpp"
#include "queries/VulkanQueryEnums.hpp"
#include "queries/VulkanQueryPool.hpp"
#include "queries/VulkanQuerySettings.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Live fixture for the query pool cache.
class VulkanQueryPoolManagerLiveTest : public VulkanQueryLiveTest
{};

TEST_F(VulkanQueryPoolManagerLiveTest, OcclusionPoolsRoundUpAndAreRecycled)
{
    VulkanQueryPoolManager& manager = *m_queryPoolManager;
    EXPECT_EQ(&manager.getDevice(), m_device);
    EXPECT_EQ(manager.getOcclusionQueryPoolSize(), 0u);

    VulkanQueryPool* const pool = manager.acquireOcclusionQueryPool(10u);
    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->isValid());
    EXPECT_EQ(pool->getPoolType(), VulkanQueryPoolType::Occlusion);
    EXPECT_EQ(pool->getMaxQueries(), VulkanQueryPoolManager::kOcclusionPoolSizeGranularity);
    EXPECT_EQ(manager.getOcclusionQueryPoolSize(), VulkanQueryPoolManager::kOcclusionPoolSizeGranularity);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 0u);

    manager.releaseQueryPool(pool);
    EXPECT_TRUE(pool->isDirty());
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 1u);

    // A request that fits the shared size reuses the idle pool.
    VulkanQueryPool* const reused = manager.acquireOcclusionQueryPool(100u);
    EXPECT_EQ(reused, pool);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 0u);
    manager.releaseQueryPool(reused);
}

TEST_F(VulkanQueryPoolManagerLiveTest, GrowingTheOcclusionSizeDropsSmallerPools)
{
    VulkanQueryPoolManager& manager = *m_queryPoolManager;
    VulkanQueryPool* const small = manager.acquireOcclusionQueryPool(1u);
    ASSERT_NE(small, nullptr);
    manager.releaseQueryPool(small);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 1u);

    VulkanQueryPool* const large = manager.acquireOcclusionQueryPool(300u);
    ASSERT_NE(large, nullptr);
    EXPECT_EQ(manager.getOcclusionQueryPoolSize(), 2u * VulkanQueryPoolManager::kOcclusionPoolSizeGranularity);
    EXPECT_EQ(large->getMaxQueries(), 2u * VulkanQueryPoolManager::kOcclusionPoolSizeGranularity);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 0u);

    // Later small requests keep using the grown size.
    manager.releaseQueryPool(large);
    VulkanQueryPool* const again = manager.acquireOcclusionQueryPool(0u);
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again->getMaxQueries(), 2u * VulkanQueryPoolManager::kOcclusionPoolSizeGranularity);
    manager.releaseQueryPool(again);
}

TEST_F(VulkanQueryPoolManagerLiveTest, ZeroSizedOcclusionRequestStillCreatesAPool)
{
    VulkanQueryPool* const pool = m_queryPoolManager->acquireOcclusionQueryPool(0u);
    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->isValid());
    EXPECT_EQ(pool->getMaxQueries(), 1u);
    m_queryPoolManager->releaseQueryPool(pool);
}

TEST_F(VulkanQueryPoolManagerLiveTest, TimingPoolsUseTheConfiguredSize)
{
    VulkanQueryPoolManager& manager = *m_queryPoolManager;
    VulkanQueryPool* const pool = manager.acquireTimingQueryPool();
    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->isValid());
    EXPECT_EQ(pool->getPoolType(), VulkanQueryPoolType::Timestamp);
    EXPECT_EQ(pool->getMaxQueries(), getVulkanTimingQueryPoolSize());

    manager.releaseQueryPool(pool);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Timestamp), 1u);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 0u);
    EXPECT_EQ(manager.acquireTimingQueryPool(), pool);
    manager.releaseQueryPool(pool);

    // Releasing nothing is a no-op.
    manager.releaseQueryPool(nullptr);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Timestamp), 1u);
}

TEST_F(VulkanQueryPoolManagerLiveTest, StalePoolsAreDeletedAfterTheDeletionDelay)
{
    VulkanQueryPoolManager& manager = *m_queryPoolManager;
    manager.releaseQueryPool(manager.acquireTimingQueryPool());
    manager.releaseQueryPool(manager.acquireOcclusionQueryPool(4u));

    const gp::Int32 deletionDelay = getVulkanQueryPoolDeletionDelay();
    for (gp::Int32 frame = 0; frame < deletionDelay; ++frame)
    {
        manager.removeStaleQueryPools();
    }
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Timestamp), 1u);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 1u);

    manager.removeStaleQueryPools();
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Timestamp), 0u);
    EXPECT_EQ(manager.getFreePoolCount(VulkanQueryPoolType::Occlusion), 0u);
}

}   // namespace gp::rhi::vulkan::tests
