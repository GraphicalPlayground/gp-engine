// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderPass.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "renderpass/VulkanRenderPassBuilder.hpp"
#include "renderpass/VulkanRenderPassSettings.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VkRenderPass createVulkanRenderPass(
    const VulkanDevice& device, const VulkanRenderTargetCaps& caps, const VulkanRenderTargetLayout& layout
) noexcept
{
    if (device.getHandle() == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    const bool inputAttachmentShaderRead = getVulkanInputAttachmentShaderRead();
    if (caps.hasKHRRenderPass2 && (dynamic_api::vkCreateRenderPass2KHR != nullptr))
    {
        VulkanRenderPassBuilder<VulkanRenderPass2Traits> builder(caps, inputAttachmentShaderRead);
        builder.build(layout);
        result =
            vkCreateRenderPass2KHR(device.getHandle(), &builder.getCreateInfo(), GP_VULKAN_CPU_ALLOCATOR, &renderPass);
    }
    else if (dynamic_api::vkCreateRenderPass != nullptr)
    {
        VulkanRenderPassBuilder<VulkanRenderPass1Traits> builder(caps, inputAttachmentShaderRead);
        builder.build(layout);
        result = vkCreateRenderPass(device.getHandle(), &builder.getCreateInfo(), GP_VULKAN_CPU_ALLOCATOR, &renderPass);
    }
    else
    {
        return VK_NULL_HANDLE;
    }

    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Render pass creation failed with {}.", vulkanResultToString(result));
        return VK_NULL_HANDLE;
    }
    return renderPass;
}

VulkanRenderPass::VulkanRenderPass(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const VulkanRenderTargetCaps& caps,
    const VulkanRenderTargetLayout& layout
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_layout(layout)
    , m_renderPass(createVulkanRenderPass(device, caps, layout))
    , m_numUsedClearValues(layout.getNumUsedClearValues())
{}

VulkanRenderPass::~VulkanRenderPass() noexcept
{
    if (m_renderPass == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->enqueueHandle(VulkanDeferredDeletionQueue::EntryType::RenderPass, m_renderPass);
    }
    else if (dynamic_api::vkDestroyRenderPass != nullptr)
    {
        vkDestroyRenderPass(m_device.getHandle(), m_renderPass, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_renderPass = VK_NULL_HANDLE;
}

}   // namespace gp::rhi::vulkan
