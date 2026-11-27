// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanCommandContextProvider.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/IComputeContext.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "contexts/VulkanCommandListContext.hpp"
#include "contexts/VulkanContextEnums.hpp"
#include "contexts/VulkanUploadContext.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

VulkanCommandContextProvider::VulkanCommandContextProvider(VulkanUploadContextPool* const uploadPool) noexcept
    : m_uploadPool(uploadPool)
{}

VulkanCommandContextProvider::~VulkanCommandContextProvider() noexcept
{
    if ((m_activeUploadContext != nullptr) && (m_uploadPool != nullptr))
    {
        m_uploadPool->release(*m_activeUploadContext);
    }
    m_activeUploadContext = nullptr;
}

VulkanCommandListContext& VulkanCommandContextProvider::getVulkanContext(command::CommandListBase& cmdList) noexcept
{
    // Every context the Vulkan RHI hands out is a VulkanCommandListContext; wrapping layers (validation) forward
    // their lowest level context, so the downcast is always valid.
    return static_cast<VulkanCommandListContext&>(cmdList.getContext().getLowestLevelContext());
}

IVulkanTransferCommandRecorder*
    VulkanCommandContextProvider::getCommandListRecorder(command::CommandListBase& cmdList) noexcept
{
    return getVulkanContext(cmdList).getTransferRecorder();
}

IVulkanTransferCommandRecorder*
    VulkanCommandContextProvider::getUploadRecorder(command::CommandListBase& cmdList) noexcept
{
    static_cast<void>(cmdList);
    if (m_uploadPool == nullptr)
    {
        return nullptr;
    }
    if (m_activeUploadContext == nullptr)
    {
        m_activeUploadContext = &m_uploadPool->acquire();
    }
    return m_activeUploadContext->getTransferRecorder();
}

IVulkanTransferCommandRecorder*
    VulkanCommandContextProvider::getContextRecorder(command::IComputeContext* const context) noexcept
{
    if (context == nullptr)
    {
        return nullptr;
    }
    return static_cast<VulkanCommandListContext&>(context->getLowestLevelContext()).getTransferRecorder();
}

sync::PipelineStage VulkanCommandContextProvider::getTrackedPipelines(
    command::IComputeContext* const context, const resource::Resource* const resource
) noexcept
{
    if ((context == nullptr) || (resource == nullptr))
    {
        return sync::PipelineStage::None;
    }
    return context->getTrackedPipelines(static_cast<const resource::ViewableResource*>(resource));
}

void VulkanCommandContextProvider::submitAndBlockUntilGPUIdle(command::CommandListImmediate& cmdList) noexcept
{
    if (m_activeUploadContext != nullptr)
    {
        static_cast<void>(m_activeUploadContext->flushCommands(VulkanFlushFlags::WaitForCompletion));
        if (m_uploadPool != nullptr)
        {
            m_uploadPool->release(*m_activeUploadContext);
        }
        m_activeUploadContext = nullptr;
    }
    static_cast<void>(getVulkanContext(cmdList).flushCommands(VulkanFlushFlags::WaitForCompletion));
}

bool VulkanCommandContextProvider::needsExtraTransitions(const command::CommandListBase& cmdList) const noexcept
{
    static_cast<void>(cmdList);
    return true;
}

void VulkanCommandContextProvider::addPendingSyncPoint(
    command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint
) noexcept
{
    getVulkanContext(cmdList).addPendingSyncPoint(syncPoint);
}

void VulkanCommandContextProvider::addSubmissionEvent(
    command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint
) noexcept
{
    getVulkanContext(cmdList).addSubmissionEvent(syncPoint);
}

void VulkanCommandContextProvider::signalSyncPoint(
    command::CommandListBase& cmdList, const VulkanSyncPointRef& syncPoint
) noexcept
{
    getVulkanContext(cmdList).signalSyncPoint(syncPoint);
}

VkCommandBuffer VulkanCommandContextProvider::getActiveCommandBuffer(command::CommandListBase& cmdList) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getVulkanContext(cmdList).getCommandBuffer();
    return (commandBuffer != nullptr) ? commandBuffer->getHandle() : VK_NULL_HANDLE;
}

bool VulkanCommandContextProvider::finalizeUploads(VulkanPlatformCommandList& outCommandList) noexcept
{
    if (m_activeUploadContext == nullptr)
    {
        return false;
    }
    m_activeUploadContext->finalize(outCommandList);
    if (m_uploadPool != nullptr)
    {
        m_uploadPool->release(*m_activeUploadContext);
    }
    m_activeUploadContext = nullptr;
    return true;
}

}   // namespace gp::rhi::vulkan
