// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"      // IWYU pragma: keep
#include "CoreMinimal.hpp"                  // IWYU pragma: keep
#include "queries/VulkanQueryForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Creates a render query.
/// @param[in] queryType `Occlusion` or `AbsoluteTime`.
/// @return New query, or null for an unsupported type.
[[nodiscard]] GP_RHI_VULKAN_API sync::RenderQueryRef createVulkanRenderQuery(sync::RenderQueryType queryType) noexcept;

/// @brief Creates a render query pool.
/// @param[in] queryType `Occlusion` or `AbsoluteTime`.
/// @param[in] numQueries Capacity to reserve for idle queries; 0 for none.
/// @return New pool.
[[nodiscard]] GP_RHI_VULKAN_API sync::RenderQueryPoolRef
    createVulkanRenderQueryPool(sync::RenderQueryType queryType, gp::UInt32 numQueries = 0u) noexcept;

/// @brief Reads the result of a render query.
/// @details Occlusion queries return the number of samples that passed. Timestamps are converted to microseconds.
/// @param[in] renderQuery Query created by `createVulkanRenderQuery()` and ended at least once.
/// @param[out] outResult Receives the result; set to 0 when the query was never used.
/// @param[in] wait True to block until the result is available.
/// @param[in] timestampPeriod Nanoseconds per GPU tick (`VkPhysicalDeviceLimits::timestampPeriod`).
/// @return True when `outResult` holds the result.
[[nodiscard]] GP_RHI_VULKAN_API bool getVulkanRenderQueryResult(
    sync::RenderQuery* renderQuery, gp::UInt64& outResult, bool wait, gp::Float32 timestampPeriod
) noexcept;

/// @brief Begins a render query from the top of the pipe.
/// @details Runs on the graphics context now when the command list is at the bottom of the pipe, otherwise enqueues
///          the command.
/// @param[in,out] cmdList Command list that orders the query.
/// @param[in] renderQuery Query to begin.
GP_RHI_VULKAN_API void
    beginVulkanRenderQueryTopOfPipe(command::CommandListBase& cmdList, sync::RenderQuery* renderQuery) noexcept;

/// @brief Ends a render query from the top of the pipe.
/// @details Gives the query a new sync point, registers it on the executing context so it is signaled once the
///          payload completes, then forwards the end command like `beginVulkanRenderQueryTopOfPipe()`.
/// @param[in,out] cmdList Command list that orders the query.
/// @param[in] syncContextProvider Context seam that registers the sync point; must outlive the command list.
/// @param[in] renderQuery Query to end.
GP_RHI_VULKAN_API void endVulkanRenderQueryTopOfPipe(
    command::CommandListBase& cmdList, IVulkanSyncContextProvider& syncContextProvider, sync::RenderQuery* renderQuery
) noexcept;

/// @brief Writes a GPU fence from the top of the pipe.
/// @details Gives the fence a completion sync point (plus a submission sync point when it uses an event) and
///          enqueues the write. With an event, the command buffer sets it and both sync points are registered on the
///          payload; otherwise the current payload is ended so the completion sync point covers all prior work.
/// @param[in,out] cmdList Command list that orders the write.
/// @param[in] syncContextProvider Context seam that registers the sync points; must outlive the command list.
/// @param[in] fence `VulkanGPUFence` that was not issued since its last clear.
GP_RHI_VULKAN_API void writeVulkanGPUFenceTopOfPipe(
    command::CommandListBase& cmdList, IVulkanSyncContextProvider& syncContextProvider, sync::GPUFence* fence
) noexcept;

}   // namespace gp::rhi::vulkan
