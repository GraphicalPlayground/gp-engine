// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                  // IWYU pragma: keep
#include "queries/VulkanQueryForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"             // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Opens an occlusion query batch: appends a clean occlusion pool sized for the batch to `pools`.
/// @details The pool is reset on the host when `VK_EXT_host_query_reset` is available, otherwise on `cmdBuffer`,
///          which must then be outside a render pass.
/// @param[in,out] manager Pool cache.
/// @param[in,out] pools Pools of the payload being recorded.
/// @param[in] cmdBuffer Command buffer recording the batch.
/// @param[in] numQueriesInBatch Number of occlusion queries the batch issues.
/// @param[in] multiViewCount View count of the render pass, or 0 outside multiview.
GP_RHI_VULKAN_API void beginVulkanOcclusionQueryBatch(
    VulkanQueryPoolManager& manager,
    VulkanPayloadQueryPools& pools,
    VkCommandBuffer cmdBuffer,
    gp::UInt32 numQueriesInBatch,
    gp::UInt32 multiViewCount
) noexcept;

/// @brief Returns the occlusion pool of the open batch.
/// @param[in] pools Pools of the payload being recorded; a batch must be open.
/// @return Last occlusion pool.
[[nodiscard]] GP_RHI_VULKAN_API VulkanQueryPool*
    getCurrentVulkanOcclusionQueryPool(VulkanPayloadQueryPools& pools) noexcept;

/// @brief Returns a timestamp pool with a free query of the wanted stride, acquiring a new one when needed.
/// @details Mixing multiview and single-view timestamps in one pool would complicate the readback, so a stride change
///          switches to a new pool.
/// @param[in,out] manager Pool cache.
/// @param[in,out] pools Pools of the payload being recorded.
/// @param[in] cmdBuffer Command buffer used to reset a new pool when host query reset is unavailable.
/// @param[in] multiViewCount View count of the active render pass, or 0 outside multiview.
/// @return Timestamp pool that is not full.
[[nodiscard]] GP_RHI_VULKAN_API VulkanQueryPool* getCurrentVulkanTimestampQueryPool(
    VulkanQueryPoolManager& manager,
    VulkanPayloadQueryPools& pools,
    VkCommandBuffer cmdBuffer,
    gp::UInt32 multiViewCount
) noexcept;

/// @brief Records the start of a render query.
/// @details Occlusion queries reserve a slot in the open batch and record `vkCmdBeginQuery`. Timestamps record
///          nothing at begin.
/// @param[in,out] pools Pools of the payload being recorded.
/// @param[in] cmdBuffer Command buffer recording the query.
/// @param[in,out] query Query to begin.
GP_RHI_VULKAN_API void recordVulkanBeginRenderQuery(
    VulkanPayloadQueryPools& pools, VkCommandBuffer cmdBuffer, VulkanRenderQuery& query
) noexcept;

/// @brief Records the end of a render query.
/// @details Occlusion queries record `vkCmdEndQuery` on the slot reserved at begin. Timestamps reserve a slot and
///          record `vkCmdWriteTimestamp` at the `r.Vulkan.TimestampQueryStage` stage.
/// @param[in,out] manager Pool cache.
/// @param[in,out] pools Pools of the payload being recorded.
/// @param[in] cmdBuffer Command buffer recording the query.
/// @param[in,out] query Query to end; its sync point must be set.
/// @param[in] multiViewCount View count of the active render pass, or 0 outside multiview.
GP_RHI_VULKAN_API void recordVulkanEndRenderQuery(
    VulkanQueryPoolManager& manager,
    VulkanPayloadQueryPools& pools,
    VkCommandBuffer cmdBuffer,
    VulkanRenderQuery& query,
    gp::UInt32 multiViewCount
) noexcept;

/// @brief Reads back every query of a completed payload and gives the pools back to `manager`.
/// @param[in,out] manager Pool cache.
/// @param[in,out] pools Pools of the completed payload; left empty.
GP_RHI_VULKAN_API void
    resolveVulkanPayloadQueries(VulkanQueryPoolManager& manager, VulkanPayloadQueryPools& pools) noexcept;

/// @brief Gives the pools of a payload that never completed back to `manager` without reading them.
/// @param[in,out] manager Pool cache.
/// @param[in,out] pools Pools to release; left empty.
GP_RHI_VULKAN_API void
    releaseVulkanPayloadQueries(VulkanQueryPoolManager& manager, VulkanPayloadQueryPools& pools) noexcept;

}   // namespace gp::rhi::vulkan
