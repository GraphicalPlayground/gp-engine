// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanRenderQuery.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "queries/VulkanQueryEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanRenderQueryTest, OcclusionQueryStartsUnreserved)
{
    {
        VulkanRenderQuery* const rawQuery = new VulkanRenderQuery(sync::RenderQueryType::Occlusion);
        const sync::RenderQueryRef queryRef(rawQuery);

        EXPECT_EQ(rawQuery->getQueryType(), sync::RenderQueryType::Occlusion);
        EXPECT_EQ(rawQuery->getQueryPoolType(), VulkanQueryPoolType::Occlusion);
        EXPECT_EQ(rawQuery->indexInPool, VulkanRenderQuery::kInvalidIndexInPool);
        EXPECT_EQ(rawQuery->result, 0u);
        EXPECT_FALSE(rawQuery->syncPoint.isValid());
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanRenderQueryTest, AbsoluteTimeQueryUsesTimestampPools)
{
    {
        VulkanRenderQuery* const rawQuery = new VulkanRenderQuery(sync::RenderQueryType::AbsoluteTime);
        const sync::RenderQueryRef queryRef(rawQuery);

        EXPECT_EQ(rawQuery->getQueryType(), sync::RenderQueryType::AbsoluteTime);
        EXPECT_EQ(rawQuery->getQueryPoolType(), VulkanQueryPoolType::Timestamp);
        EXPECT_EQ(rawQuery->getQueryPoolType(), toVulkanQueryPoolType(rawQuery->getQueryType()));
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

}   // namespace gp::rhi::vulkan::tests
