// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::vulkan
{

void VulkanDescriptorPoolsManager::init(VulkanDevice& device) noexcept
{
    m_device = &device;
}

VulkanDescriptorPoolSetContainer&
    VulkanDescriptorPoolsManager::acquirePoolSetContainer(const gp::UInt64 currentFrame) noexcept
{
    GP_CHECK(m_device != nullptr);

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    for (gp::UniquePtr<VulkanDescriptorPoolSetContainer>& poolSet: m_poolSets)
    {
        if (poolSet->isUnused())
        {
            poolSet->setUsed(true, currentFrame);
            return *poolSet;
        }
    }

    m_poolSets.pushBack(gp::makeUnique<VulkanDescriptorPoolSetContainer>(*m_device, currentFrame));
    return *m_poolSets.back();
}

void VulkanDescriptorPoolsManager::releasePoolSet(VulkanDescriptorPoolSetContainer& poolSet) noexcept
{
    // Only the owning context touches a used container, so resetting it needs no lock; the used flag is read by
    // `acquirePoolSetContainer()` and `gc()` under the lock, so it is flipped under the lock too.
    poolSet.reset();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    poolSet.setUsed(false, poolSet.getLastFrameUsed());
}

bool VulkanDescriptorPoolsManager::gc(const gp::UInt64 currentFrame) noexcept
{
    return gc(currentFrame, getVulkanNumFramesToWaitForDescriptorDelete());
}

bool VulkanDescriptorPoolsManager::gc(const gp::UInt64 currentFrame, const gp::UInt32 framesToWait) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    // Containers are created at the back, so the back is the most likely to be surplus after a load spike.
    for (gp::USize index = static_cast<gp::USize>(m_poolSets.size()); index > 0u; --index)
    {
        VulkanDescriptorPoolSetContainer& poolSet = *m_poolSets[index - 1u];
        const gp::UInt64 lastFrameUsed = poolSet.getLastFrameUsed();
        const bool isStale = (currentFrame > lastFrameUsed) && ((currentFrame - lastFrameUsed) > framesToWait);
        if (poolSet.isUnused() && isStale)
        {
            m_poolSets.removeAtSwap(index - 1u);
            return true;
        }
    }
    return false;
}

gp::USize VulkanDescriptorPoolsManager::getNumPoolSetContainers() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return static_cast<gp::USize>(m_poolSets.size());
}

}   // namespace gp::rhi::vulkan
