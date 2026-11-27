// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanCommonPipelineState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VulkanCommonPipelineState::VulkanCommonPipelineState(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    VulkanPipelineStateCacheManager* const owner
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_owner(owner)
{}

VulkanCommonPipelineState::~VulkanCommonPipelineState() noexcept
{
    destroyPipeline(false);
}

void VulkanCommonPipelineState::destroyPipeline(const bool immediate) noexcept
{
    if (m_pipeline == VK_NULL_HANDLE)
    {
        return;
    }

    if (!immediate && (m_deferredDeletionQueue != nullptr))
    {
        m_deferredDeletionQueue->enqueueHandle(VulkanDeferredDeletionQueue::EntryType::Pipeline, m_pipeline);
    }
    else if ((dynamic_api::vkDestroyPipeline != nullptr) && (m_device.getHandle() != VK_NULL_HANDLE))
    {
        vkDestroyPipeline(m_device.getHandle(), m_pipeline, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_pipeline = VK_NULL_HANDLE;
}

}   // namespace gp::rhi::vulkan
