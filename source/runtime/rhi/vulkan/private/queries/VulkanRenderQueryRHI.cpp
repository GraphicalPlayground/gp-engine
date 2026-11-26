// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanRenderQueryRHI.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/CommandPacket.hpp"
#include "commands/ICommandContext.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "queries/VulkanQuerySettings.hpp"
#include "queries/VulkanRenderQuery.hpp"
#include "queries/VulkanRenderQueryPool.hpp"
#include "sync/IVulkanSyncContextProvider.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/VulkanGPUFence.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

sync::RenderQueryRef createVulkanRenderQuery(const sync::RenderQueryType queryType) noexcept
{
    if ((queryType != sync::RenderQueryType::Occlusion) && (queryType != sync::RenderQueryType::AbsoluteTime))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Unknown render query type {}.", static_cast<gp::UInt32>(queryType));
        return sync::RenderQueryRef{};
    }

    return sync::RenderQueryRef(new VulkanRenderQuery(queryType));
}

sync::RenderQueryPoolRef
    createVulkanRenderQueryPool(const sync::RenderQueryType queryType, const gp::UInt32 numQueries) noexcept
{
    return sync::RenderQueryPoolRef(new VulkanRenderQueryPool(queryType, numQueries));
}

bool getVulkanRenderQueryResult(
    sync::RenderQuery* const renderQuery, gp::UInt64& outResult, const bool wait, const gp::Float32 timestampPeriod
) noexcept
{
    GP_CHECK(renderQuery != nullptr);
    VulkanRenderQuery* const query = static_cast<VulkanRenderQuery*>(renderQuery);

    if (!query->syncPoint.isValid())
    {
        GP_LOG_WARNING(
            LogVulkanRHI, "Attempt to get result data for a render query that was never used in a command list."
        );
        outResult = 0u;
        return false;
    }

    if (!query->syncPoint->isComplete())
    {
        if (!wait)
        {
            return false;
        }
        query->syncPoint->wait();
    }

    GP_CHECK(query->syncPoint->isComplete());
    if (query->getQueryType() == sync::RenderQueryType::Occlusion)
    {
        outResult = query->result;
        return true;
    }

    if (query->getQueryType() == sync::RenderQueryType::AbsoluteTime)
    {
        outResult = convertVulkanTimestampToMicroseconds(query->result, timestampPeriod);
        return true;
    }

    return false;
}

void beginVulkanRenderQueryTopOfPipe(command::CommandListBase& cmdList, sync::RenderQuery* const renderQuery) noexcept
{
    if (cmdList.isBottomOfPipe())
    {
        cmdList.getContext().beginRenderQuery(renderQuery);
        return;
    }
    cmdList.emplaceCommand<command::CommandBeginRenderQuery>(renderQuery);
}

void endVulkanRenderQueryTopOfPipe(
    command::CommandListBase& cmdList,
    IVulkanSyncContextProvider& syncContextProvider,
    sync::RenderQuery* const renderQuery
) noexcept
{
    GP_CHECK(renderQuery != nullptr);
    VulkanRenderQuery* const query = static_cast<VulkanRenderQuery*>(renderQuery);

    // Queries issued outside of a batch use one sync point per query.
    query->syncPoint = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "UnbatchedRenderQuerySyncPoint");
    IVulkanSyncContextProvider* const provider = &syncContextProvider;
    cmdList.enqueueLambda([provider, syncPoint = query->syncPoint](command::CommandListBase& executingCmdList) noexcept
    {
        provider->addPendingSyncPoint(executingCmdList, syncPoint);
    });

    // Record the end of the query on the context.
    if (cmdList.isBottomOfPipe())
    {
        cmdList.getContext().endRenderQuery(renderQuery);
        return;
    }
    cmdList.emplaceCommand<command::CommandEndRenderQuery>(renderQuery);
}

void writeVulkanGPUFenceTopOfPipe(
    command::CommandListBase& cmdList, IVulkanSyncContextProvider& syncContextProvider, sync::GPUFence* const fence
) noexcept
{
    GP_CHECK(fence != nullptr);
    VulkanGPUFence* const vulkanFence = static_cast<VulkanGPUFence*>(fence);

    GP_CHECKF(
        !vulkanFence->getSubmittedSyncPoint().isValid() && !vulkanFence->getCompletedSyncPoint().isValid(),
        "The fence for the current GPU node has already been issued."
    );

    VulkanSyncPointRef completedSyncPoint = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "GPUFence");
    VulkanSyncPointRef submittedSyncPoint{};
    if (vulkanFence->getEvent() != VK_NULL_HANDLE)
    {
        submittedSyncPoint = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "GPUFenceSubmitted");
    }
    vulkanFence->setSyncPoints(submittedSyncPoint, completedSyncPoint);

    static_cast<void>(vulkanFence->incrementPendingWriteCommands());
    IVulkanSyncContextProvider* const provider = &syncContextProvider;
    cmdList.enqueueLambda([provider,
                           fenceRef = gp::RefCountedPtr<VulkanGPUFence>(vulkanFence),
                           completedSyncPoint,
                           submittedSyncPoint](command::CommandListBase& executingCmdList) noexcept
    {
        const VkEvent event = fenceRef->getEvent();
        if (event != VK_NULL_HANDLE)
        {
            vkCmdSetEvent(
                provider->getActiveCommandBuffer(executingCmdList), event, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
            );
            provider->addSubmissionEvent(executingCmdList, submittedSyncPoint);
            provider->addPendingSyncPoint(executingCmdList, completedSyncPoint);
        }
        else
        {
            provider->signalSyncPoint(executingCmdList, completedSyncPoint);
        }

        static_cast<void>(fenceRef->decrementPendingWriteCommands());
    });
}

}   // namespace gp::rhi::vulkan
