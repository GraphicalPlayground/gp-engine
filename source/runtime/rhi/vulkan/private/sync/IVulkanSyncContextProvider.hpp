// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"               // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Seam over the command list context that owns the payload being recorded.
/// @details GPU fences and render queries register their sync points on the bottom of the pipe through this
///          interface, on whichever context executes `cmdList`. The command list context implements it.
class GP_RHI_VULKAN_API IVulkanSyncContextProvider
{
public:
    /// @brief Destroys the provider.
    virtual ~IVulkanSyncContextProvider() noexcept = default;

    /// @brief Signals `syncPoint` once the payload currently being recorded completes on the GPU.
    /// @param[in,out] cmdList Executing command list.
    /// @param[in] syncPoint Sync point to signal.
    virtual void
        addPendingSyncPoint(command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint) noexcept = 0;

    /// @brief Signals `syncPoint` once the payload currently being recorded is submitted to its queue.
    /// @param[in,out] cmdList Executing command list.
    /// @param[in] syncPoint Sync point to signal.
    virtual void
        addSubmissionEvent(command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint) noexcept = 0;

    /// @brief Ends the current payload so that `syncPoint` is signaled when everything recorded so far completes.
    /// @param[in,out] cmdList Executing command list.
    /// @param[in] syncPoint Sync point to signal.
    virtual void signalSyncPoint(command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint) noexcept = 0;

    /// @brief Returns the command buffer currently recording for `cmdList`.
    /// @param[in,out] cmdList Executing command list.
    /// @return Active command buffer.
    [[nodiscard]] virtual VkCommandBuffer getActiveCommandBuffer(command::CommandListBase& cmdList) noexcept = 0;
};

}   // namespace gp::rhi::vulkan
