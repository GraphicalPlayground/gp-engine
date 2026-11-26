// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderModule.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VulkanShaderModule::VulkanShaderModule(
    VulkanDevice& device,
    const gp::ConstVectorView<gp::UInt32> spirv,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
{
    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreateShaderModule == nullptr) || spirv.isEmpty())
    {
        return;
    }

    VkShaderModuleCreateInfo createInfo;
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    createInfo.codeSize = static_cast<gp::USize>(spirv.size()) * sizeof(gp::UInt32);
    createInfo.pCode = spirv.data();

    const VkResult result = vkCreateShaderModule(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_module);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateShaderModule failed with {}.", vulkanResultToString(result));
        m_module = VK_NULL_HANDLE;
    }
}

VulkanShaderModule::~VulkanShaderModule() noexcept
{
    if (m_module == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->enqueueHandle(VulkanDeferredDeletionQueue::EntryType::ShaderModule, m_module);
    }
    else if (dynamic_api::vkDestroyShaderModule != nullptr)
    {
        vkDestroyShaderModule(m_device.getHandle(), m_module, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_module = VK_NULL_HANDLE;
}

}   // namespace gp::rhi::vulkan
