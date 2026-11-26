// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQueryEnums.hpp"
#include "CoreMinimal.hpp"
#include "queries/VulkanQueryTypes.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

static_assert(kVulkanQueryPoolTypeCount == 4u, "Query pool type count must match the enumerators before Count.");
static_assert(toVkQueryType(VulkanQueryPoolType::Occlusion) == VK_QUERY_TYPE_OCCLUSION);
static_assert(toVkQueryType(VulkanQueryPoolType::Timestamp) == VK_QUERY_TYPE_TIMESTAMP);
static_assert(toVulkanQueryPoolType(sync::RenderQueryType::Occlusion) == VulkanQueryPoolType::Occlusion);
static_assert(toVulkanQueryPoolType(sync::RenderQueryType::AbsoluteTime) == VulkanQueryPoolType::Timestamp);
static_assert(sizeof(VulkanQueryResultSlot) == 16u, "Readback slots must match the 64-bit availability stride.");

TEST(VulkanQueryEnumsTest, PoolTypesMapToVulkanQueryTypes)
{
    EXPECT_EQ(toVkQueryType(VulkanQueryPoolType::Occlusion), VK_QUERY_TYPE_OCCLUSION);
    EXPECT_EQ(toVkQueryType(VulkanQueryPoolType::PipelineStats), VK_QUERY_TYPE_PIPELINE_STATISTICS);
    EXPECT_EQ(toVkQueryType(VulkanQueryPoolType::Timestamp), VK_QUERY_TYPE_TIMESTAMP);
    EXPECT_EQ(
        toVkQueryType(VulkanQueryPoolType::ASCompactedSize), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR
    );
    EXPECT_EQ(toVkQueryType(VulkanQueryPoolType::Count), VK_QUERY_TYPE_MAX_ENUM);
}

TEST(VulkanQueryEnumsTest, RenderQueryTypesMapToPoolTypes)
{
    EXPECT_EQ(toVulkanQueryPoolType(sync::RenderQueryType::Occlusion), VulkanQueryPoolType::Occlusion);
    EXPECT_EQ(toVulkanQueryPoolType(sync::RenderQueryType::AbsoluteTime), VulkanQueryPoolType::Timestamp);
    EXPECT_EQ(toVulkanQueryPoolType(sync::RenderQueryType::Undefined), VulkanQueryPoolType::Count);
}

TEST(VulkanQueryTypesTest, PayloadQueryPoolsTrackPoolsPerType)
{
    VulkanPayloadQueryPools pools{};
    EXPECT_TRUE(pools.isEmpty());

    // The lists only hold pointers, so distinct addresses are enough to track ordering.
    gp::UInt64 storage[2]{};
    VulkanQueryPool* const firstPool = reinterpret_cast<VulkanQueryPool*>(&storage[0]);
    VulkanQueryPool* const secondPool = reinterpret_cast<VulkanQueryPool*>(&storage[1]);

    pools.get(VulkanQueryPoolType::Occlusion).pushBack(firstPool);
    EXPECT_FALSE(pools.isEmpty());
    EXPECT_EQ(pools.get(VulkanQueryPoolType::Occlusion).size(), 1u);
    EXPECT_TRUE(pools.get(VulkanQueryPoolType::Timestamp).isEmpty());

    VulkanPayloadQueryPools other{};
    other.get(VulkanQueryPoolType::Timestamp).pushBack(secondPool);
    other.get(VulkanQueryPoolType::Occlusion).pushBack(secondPool);
    pools.append(other);

    ASSERT_EQ(pools.get(VulkanQueryPoolType::Occlusion).size(), 2u);
    EXPECT_EQ(pools.get(VulkanQueryPoolType::Occlusion)[0], firstPool);
    EXPECT_EQ(pools.get(VulkanQueryPoolType::Occlusion)[1], secondPool);
    ASSERT_EQ(pools.get(VulkanQueryPoolType::Timestamp).size(), 1u);
    EXPECT_EQ(pools.get(VulkanQueryPoolType::Timestamp)[0], secondPool);

    // append() moves the entries, leaving the source empty.
    EXPECT_TRUE(other.isEmpty());
}

}   // namespace gp::rhi::vulkan::tests
