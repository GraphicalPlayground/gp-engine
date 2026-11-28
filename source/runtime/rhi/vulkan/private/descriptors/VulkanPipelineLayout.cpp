// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanPipelineLayout.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetLayoutCache.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VulkanPipelineLayout::VulkanPipelineLayout(
    VulkanDevice& device,
    const bool isGfxLayout,
    const bool usesBindless,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_descriptorSetsLayout(device)
    , m_isGfxLayout(isGfxLayout)
    , m_usesBindless(usesBindless)
{}

VulkanPipelineLayout::~VulkanPipelineLayout() noexcept
{
    if (m_pipelineLayout == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->enqueueHandle(
            VulkanDeferredDeletionQueue::EntryType::PipelineLayout, m_pipelineLayout
        );
    }
    else if (dynamic_api::vkDestroyPipelineLayout != nullptr)
    {
        vkDestroyPipelineLayout(m_device.getHandle(), m_pipelineLayout, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_pipelineLayout = VK_NULL_HANDLE;
}

bool VulkanPipelineLayout::compile(VulkanDescriptorSetLayoutCache& cache) noexcept
{
    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Pipeline layout compiled twice.");
        return false;
    }
    if (m_usesBindless)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Bindless pipeline layouts need the bindless descriptor manager.");
        return false;
    }
    if (!m_descriptorSetsLayout.compile(cache))
    {
        return false;
    }
    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreatePipelineLayout == nullptr))
    {
        return false;
    }

    const gp::Vector<VkDescriptorSetLayout>& layoutHandles = m_descriptorSetsLayout.getHandles();

    VkPipelineLayoutCreateInfo createInfo;
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    createInfo.setLayoutCount = static_cast<gp::UInt32>(layoutHandles.size());
    createInfo.pSetLayouts = layoutHandles.data();

    const VkResult result =
        vkCreatePipelineLayout(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_pipelineLayout);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreatePipelineLayout failed with {}.", vulkanResultToString(result));
        m_pipelineLayout = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

}   // namespace gp::rhi::vulkan
