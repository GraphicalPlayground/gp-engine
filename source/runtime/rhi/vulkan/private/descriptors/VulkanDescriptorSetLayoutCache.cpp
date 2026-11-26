// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetLayoutCache.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanDescriptorSetLayoutCache::VulkanDescriptorSetLayoutCache(
    VulkanDevice& device, VulkanDeferredDeletionQueue* const deferredDeletionQueue
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
{}

VulkanDescriptorSetLayoutCache::~VulkanDescriptorSetLayoutCache() noexcept
{
    for (Entry& entry: m_entries)
    {
        if (entry.layout.handle == VK_NULL_HANDLE)
        {
            continue;
        }
        if (m_deferredDeletionQueue != nullptr)
        {
            m_deferredDeletionQueue->enqueueHandle(
                VulkanDeferredDeletionQueue::EntryType::DescriptorSetLayout, entry.layout.handle
            );
        }
        else if (dynamic_api::vkDestroyDescriptorSetLayout != nullptr)
        {
            vkDestroyDescriptorSetLayout(m_device.getHandle(), entry.layout.handle, GP_VULKAN_CPU_ALLOCATOR);
        }
        entry.layout.handle = VK_NULL_HANDLE;
    }
    m_entries.clear();
}

bool VulkanDescriptorSetLayoutCache::findOrCreate(
    const VulkanDescriptorSetsLayoutInfo::SetLayout& setLayout, VulkanDescriptorSetLayoutEntry& outEntry
) noexcept
{
    outEntry = VulkanDescriptorSetLayoutEntry{};

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    // Lower bound on the hash, then a linear walk over the (rare) entries sharing it.
    gp::USize first = 0u;
    gp::USize count = static_cast<gp::USize>(m_entries.size());
    while (count > 0u)
    {
        const gp::USize half = count / 2u;
        const gp::USize middle = first + half;
        if (m_entries[middle].setLayout.hash < setLayout.hash)
        {
            first = middle + 1u;
            count -= half + 1u;
        }
        else
        {
            count = half;
        }
    }
    for (gp::USize index = first;
         (index < static_cast<gp::USize>(m_entries.size())) && (m_entries[index].setLayout.hash == setLayout.hash);
         ++index)
    {
        if (m_entries[index].setLayout == setLayout)
        {
            outEntry = m_entries[index].layout;
            return true;
        }
    }

    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreateDescriptorSetLayout == nullptr))
    {
        return false;
    }

    VkDescriptorSetLayoutCreateInfo createInfo;
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    createInfo.bindingCount = static_cast<gp::UInt32>(setLayout.layoutBindings.size());
    createInfo.pBindings = setLayout.layoutBindings.data();

    VkDescriptorSetLayout handle = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateDescriptorSetLayout(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &handle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateDescriptorSetLayout failed with {}.", vulkanResultToString(result));
        return false;
    }

    Entry entry{};
    entry.setLayout = setLayout;
    entry.layout.handle = handle;
    entry.layout.handleId = ++m_lastHandleId;
    outEntry = entry.layout;
    m_entries.insertAt(first, std::move(entry));
    return true;
}

gp::USize VulkanDescriptorSetLayoutCache::getNumLayouts() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return static_cast<gp::USize>(m_entries.size());
}

}   // namespace gp::rhi::vulkan
