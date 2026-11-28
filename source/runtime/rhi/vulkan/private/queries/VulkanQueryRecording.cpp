// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQueryRecording.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "queries/VulkanQueryEnums.hpp"
#include "queries/VulkanQueryPool.hpp"
#include "queries/VulkanQueryPoolManager.hpp"
#include "queries/VulkanQuerySettings.hpp"
#include "queries/VulkanQueryTypes.hpp"
#include "queries/VulkanRenderQuery.hpp"
#include "sync/SyncEnums.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Converts a multiview count to the number of slots each query occupies.
/// @param[in] multiViewCount View count, or 0 outside multiview.
/// @return Query stride, at least 1.
[[nodiscard]] constexpr gp::UInt32 toQueryStride(const gp::UInt32 multiViewCount) noexcept
{
    return (multiViewCount == 0u) ? 1u : multiViewCount;
}

/// @brief Chooses the command buffer a pool reset is recorded on.
/// @param[in] pool Pool to reset.
/// @param[in] cmdBuffer Recording command buffer.
/// @return `VK_NULL_HANDLE` when the host can reset queries, otherwise `cmdBuffer`.
[[nodiscard]] VkCommandBuffer
    getResetCommandBuffer(const VulkanQueryPool& pool, const VkCommandBuffer cmdBuffer) noexcept
{
    return (pool.getDevice().getOptionalExtensions().hasEXTHostQueryReset != 0u) ? VK_NULL_HANDLE : cmdBuffer;
}

}   // namespace

void beginVulkanOcclusionQueryBatch(
    VulkanQueryPoolManager& manager,
    VulkanPayloadQueryPools& pools,
    const VkCommandBuffer cmdBuffer,
    const gp::UInt32 numQueriesInBatch,
    const gp::UInt32 multiViewCount
) noexcept
{
    const gp::UInt32 queryStride = toQueryStride(multiViewCount);
    VulkanQueryPool* const pool = manager.acquireOcclusionQueryPool(numQueriesInBatch * queryStride);
    if (pool->isDirty() || (pool->getQueryStride() != queryStride))
    {
        pool->reset(getResetCommandBuffer(*pool, cmdBuffer), queryStride);
    }
    pools.get(VulkanQueryPoolType::Occlusion).pushBack(pool);
}

VulkanQueryPool* getCurrentVulkanOcclusionQueryPool(VulkanPayloadQueryPools& pools) noexcept
{
    gp::Vector<VulkanQueryPool*>& occlusionPools = pools.get(VulkanQueryPoolType::Occlusion);
    GP_CHECKF(!occlusionPools.isEmpty(), "Occlusion queries must be issued inside an occlusion query batch.");
    GP_CHECK(!occlusionPools.back()->isFull());
    return occlusionPools.back();
}

VulkanQueryPool* getCurrentVulkanTimestampQueryPool(
    VulkanQueryPoolManager& manager,
    VulkanPayloadQueryPools& pools,
    const VkCommandBuffer cmdBuffer,
    const gp::UInt32 multiViewCount
) noexcept
{
    gp::Vector<VulkanQueryPool*>& timestampPools = pools.get(VulkanQueryPoolType::Timestamp);
    const gp::UInt32 desiredStride = toQueryStride(multiViewCount);

    if (timestampPools.isEmpty() || timestampPools.back()->isFull() ||
        (timestampPools.back()->getQueryStride() != desiredStride))
    {
        VulkanQueryPool* const pool = manager.acquireTimingQueryPool();
        if (pool->isDirty() || (pool->getQueryStride() != desiredStride))
        {
            pool->reset(getResetCommandBuffer(*pool, cmdBuffer), desiredStride);
        }
        timestampPools.pushBack(pool);
    }
    return timestampPools.back();
}

void recordVulkanBeginRenderQuery(
    VulkanPayloadQueryPools& pools, const VkCommandBuffer cmdBuffer, VulkanRenderQuery& query
) noexcept
{
    if (query.getQueryType() == sync::RenderQueryType::Occlusion)
    {
        VulkanQueryPool* const pool = getCurrentVulkanOcclusionQueryPool(pools);
        pool->reserveQuery(query);
        vkCmdBeginQuery(cmdBuffer, pool->getHandle(), query.indexInPool, getVulkanOcclusionQueryControlFlags());
        return;
    }

    GP_CHECKF(
        query.getQueryType() != sync::RenderQueryType::AbsoluteTime,
        "Timing queries should not call beginRenderQuery()."
    );
}

void recordVulkanEndRenderQuery(
    VulkanQueryPoolManager& manager,
    VulkanPayloadQueryPools& pools,
    const VkCommandBuffer cmdBuffer,
    VulkanRenderQuery& query,
    const gp::UInt32 multiViewCount
) noexcept
{
    GP_CHECK(query.syncPoint.isValid());

    if (query.getQueryType() == sync::RenderQueryType::Occlusion)
    {
        VulkanQueryPool* const pool = getCurrentVulkanOcclusionQueryPool(pools);
        vkCmdEndQuery(cmdBuffer, pool->getHandle(), query.indexInPool);
    }
    else if (query.getQueryType() == sync::RenderQueryType::AbsoluteTime)
    {
        VulkanQueryPool* const pool = getCurrentVulkanTimestampQueryPool(manager, pools, cmdBuffer, multiViewCount);
        pool->reserveQuery(query);
        vkCmdWriteTimestamp(cmdBuffer, getVulkanTimestampQueryStage(), pool->getHandle(), query.indexInPool);
    }
}

void resolveVulkanPayloadQueries(VulkanQueryPoolManager& manager, VulkanPayloadQueryPools& pools) noexcept
{
    for (gp::USize typeIndex = 0u; typeIndex < kVulkanQueryPoolTypeCount; ++typeIndex)
    {
        gp::Vector<VulkanQueryPool*>& typePools = pools.pools[typeIndex];
        for (gp::USize index = 0u; index < typePools.size(); ++index)
        {
            VulkanQueryPool* const pool = typePools[index];
            static_cast<void>(pool->resolveResults());
            manager.releaseQueryPool(pool);
        }
        typePools.clear();
    }
}

void releaseVulkanPayloadQueries(VulkanQueryPoolManager& manager, VulkanPayloadQueryPools& pools) noexcept
{
    for (gp::USize typeIndex = 0u; typeIndex < kVulkanQueryPoolTypeCount; ++typeIndex)
    {
        gp::Vector<VulkanQueryPool*>& typePools = pools.pools[typeIndex];
        for (gp::USize index = 0u; index < typePools.size(); ++index)
        {
            manager.releaseQueryPool(typePools[index]);
        }
        typePools.clear();
    }
}

}   // namespace gp::rhi::vulkan
