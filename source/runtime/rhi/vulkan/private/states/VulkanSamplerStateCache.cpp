// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanSamplerStateCache.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "states/VulkanSamplerState.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

VulkanSamplerStateCache::VulkanSamplerStateCache(
    VulkanDevice& device, VulkanDeferredDeletionQueue* const deferredDeletionQueue
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
{}

VulkanSamplerStateCache::~VulkanSamplerStateCache() noexcept
{
    reset();
}

pipeline::SamplerStateRef VulkanSamplerStateCache::findOrCreate(const pipeline::SamplerStateDesc& desc) noexcept
{
    VkSamplerCreateInfo createInfo;
    VulkanSamplerState::setupSamplerCreateInfo(desc, m_device, createInfo);
    const gp::UInt32 createInfoHash = VulkanSamplerState::computeCreateInfoHash(createInfo);

    // Lookup and insertion share one lock so two threads never create the same sampler twice.
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    for (const Entry& entry: m_entries)
    {
        if (entry.createInfoHash == createInfoHash)
        {
            return pipeline::SamplerStateRef(entry.sampler);
        }
    }

    gp::RefCountedPtr<VulkanSamplerState> sampler(
        new VulkanSamplerState(m_device, createInfo, m_deferredDeletionQueue)
    );
    m_entries.emplaceBack(Entry{ createInfoHash, sampler });
    return pipeline::SamplerStateRef(sampler);
}

gp::USize VulkanSamplerStateCache::getNumSamplers() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_entries.size();
}

void VulkanSamplerStateCache::reset() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_entries.clear();
}

}   // namespace gp::rhi::vulkan
