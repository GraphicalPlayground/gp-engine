// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanRenderQueryPool.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "queries/VulkanRenderQuery.hpp"
#include "sync/RenderQuery.hpp"                // IWYU pragma: keep
#include "sync/RenderQueryPool.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanRenderQueryPoolTest, AllocatesQueriesOfThePoolType)
{
    {
        VulkanRenderQueryPool* const rawPool = new VulkanRenderQueryPool(sync::RenderQueryType::AbsoluteTime, 4u);
        const sync::RenderQueryPoolRef poolRef(rawPool);
        EXPECT_EQ(rawPool->getQueryType(), sync::RenderQueryType::AbsoluteTime);
        EXPECT_EQ(rawPool->getAllocatedQueryCount(), 0u);
        EXPECT_EQ(rawPool->getFreeQueryCount(), 0u);

        sync::PooledRenderQuery pooled = rawPool->allocateQuery();
        ASSERT_TRUE(pooled.isValid());
        const VulkanRenderQuery* const query = static_cast<const VulkanRenderQuery*>(pooled.getQuery());
        EXPECT_EQ(query->getQueryType(), sync::RenderQueryType::AbsoluteTime);
        EXPECT_EQ(rawPool->getAllocatedQueryCount(), 1u);

        pooled.releaseQuery();
        EXPECT_FALSE(pooled.isValid());
        EXPECT_EQ(rawPool->getFreeQueryCount(), 1u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanRenderQueryPoolTest, RecyclesReleasedQueries)
{
    {
        VulkanRenderQueryPool* const rawPool = new VulkanRenderQueryPool(sync::RenderQueryType::Occlusion);
        const sync::RenderQueryPoolRef poolRef(rawPool);

        sync::PooledRenderQuery first = rawPool->allocateQuery();
        sync::PooledRenderQuery second = rawPool->allocateQuery();
        ASSERT_TRUE(first.isValid());
        ASSERT_TRUE(second.isValid());
        EXPECT_NE(first.getQuery(), second.getQuery());
        EXPECT_EQ(rawPool->getAllocatedQueryCount(), 2u);
        EXPECT_EQ(rawPool->getFreeQueryCount(), 0u);

        const sync::RenderQuery* const firstQuery = first.getQuery();
        first.releaseQuery();
        EXPECT_EQ(rawPool->getFreeQueryCount(), 1u);

        // The idle query is handed out again instead of creating a third one.
        sync::PooledRenderQuery recycled = rawPool->allocateQuery();
        EXPECT_EQ(recycled.getQuery(), firstQuery);
        EXPECT_EQ(rawPool->getAllocatedQueryCount(), 2u);
        EXPECT_EQ(rawPool->getFreeQueryCount(), 0u);

        // Moving a pooled query keeps a single owner.
        sync::PooledRenderQuery moved(std::move(recycled));
        EXPECT_FALSE(recycled.isValid());
        EXPECT_EQ(moved.getQuery(), firstQuery);

        moved.releaseQuery();
        second.releaseQuery();
        EXPECT_EQ(rawPool->getFreeQueryCount(), 2u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

}   // namespace gp::rhi::vulkan::tests
