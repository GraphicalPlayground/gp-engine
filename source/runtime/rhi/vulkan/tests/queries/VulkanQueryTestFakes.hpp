// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "sync/IVulkanSyncContextProvider.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan::tests
{

/// @brief Records every sync point registered through the sync context seam.
class FakeSyncContextProvider final : public IVulkanSyncContextProvider
{
public:
    /// @copydoc IVulkanSyncContextProvider::addPendingSyncPoint()
    virtual void addPendingSyncPoint(command::CommandListBase&, const VulkanSyncPointRef& syncPoint) noexcept override
    {
        ++pendingSyncPointCount;
        lastPendingSyncPoint = syncPoint;
    }

    /// @copydoc IVulkanSyncContextProvider::addSubmissionEvent()
    virtual void addSubmissionEvent(command::CommandListBase&, const VulkanSyncPointRef& syncPoint) noexcept override
    {
        ++submissionEventCount;
        lastSubmissionEvent = syncPoint;
    }

    /// @copydoc IVulkanSyncContextProvider::signalSyncPoint()
    virtual void signalSyncPoint(command::CommandListBase&, const VulkanSyncPointRef& syncPoint) noexcept override
    {
        ++signalSyncPointCount;
        lastSignaledSyncPoint = syncPoint;
    }

    /// @copydoc IVulkanSyncContextProvider::getActiveCommandBuffer()
    [[nodiscard]] virtual VkCommandBuffer getActiveCommandBuffer(command::CommandListBase&) noexcept override
    {
        ++activeCommandBufferRequests;
        return activeCommandBuffer;
    }

    gp::UInt32 pendingSyncPointCount{ 0u };
    gp::UInt32 submissionEventCount{ 0u };
    gp::UInt32 signalSyncPointCount{ 0u };
    gp::UInt32 activeCommandBufferRequests{ 0u };
    VulkanSyncPointRef lastPendingSyncPoint{};
    VulkanSyncPointRef lastSubmissionEvent{};
    VulkanSyncPointRef lastSignaledSyncPoint{};
    VkCommandBuffer activeCommandBuffer{ VK_NULL_HANDLE };
};

}   // namespace gp::rhi::vulkan::tests
