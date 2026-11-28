// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetCache.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "descriptors/VulkanDescriptorSetWriter.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "VulkanConfiguration.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanCachedDescriptorPool::VulkanCachedDescriptorPool(
    VulkanDevice& device, const gp::UInt32 maxDescriptorSets, const VulkanDescriptorPoolSizeRatios& poolSizeRatios
) noexcept
    : m_pool(device, maxDescriptorSets, poolSizeRatios)
    , m_setCapacity(static_cast<gp::UInt32>(static_cast<gp::Float32>(maxDescriptorSets) * kMaxAllocRatio))
{}

gp::UInt32 VulkanCachedDescriptorPool::getPoolSizeStatistic(const VkDescriptorType type) const noexcept
{
    const gp::USize slot = getVulkanDescriptorTypeSlot(type);
    return (slot < kVulkanNumCoreDescriptorTypes) ? m_poolSizesStatistic[slot] : 0u;
}

void VulkanCachedDescriptorPool::reset() noexcept
{
    m_pool.reset();
    m_setsCache.clear();
    m_setCache.clear();
    m_poolSizesStatistic = gp::Array<gp::UInt32, kVulkanNumCoreDescriptorTypes>{};
}

bool VulkanCachedDescriptorPool::canGC(const gp::UInt64 currentFrame, const gp::UInt32 numFramesToWait) const noexcept
{
    return (currentFrame > m_recentFrame) &&
           ((currentFrame - m_recentFrame) > static_cast<gp::UInt64>(numFramesToWait));
}

gp::Float32 VulkanCachedDescriptorPool::calcAllocRatio() const noexcept
{
    const gp::UInt32 maxSets = m_pool.getMaxDescriptorSets();
    gp::Float32 allocRatio =
        (maxSets != 0u) ? (static_cast<gp::Float32>(m_setCache.size()) / static_cast<gp::Float32>(maxSets)) : 0.0f;
    if (allocRatio < kMinAllocRatio)
    {
        GP_LOG_WARNING(
            LogVulkanRHI,
            "Descriptor set cache pool allocation ratio is too low: {}. Using {}.",
            static_cast<gp::Float64>(allocRatio),
            static_cast<gp::Float64>(kMinAllocRatio)
        );
        allocRatio = kMinAllocRatio;
    }
    return allocRatio;
}

void VulkanCachedDescriptorPool::calcPoolSizesRatio(VulkanDescriptorPoolSizeRatios& outPoolSizeRatios) const noexcept
{
    const gp::USize numSets = (m_setCache.size() > 0u) ? static_cast<gp::USize>(m_setCache.size()) : 1u;
    for (gp::USize index = 0u; index < kVulkanNumCoreDescriptorTypes; ++index)
    {
        outPoolSizeRatios[index] =
            static_cast<gp::Float32>(m_poolSizesStatistic[index]) / static_cast<gp::Float32>(numSets);
    }
}

bool VulkanCachedDescriptorPool::findDescriptorSets(
    const VulkanDescriptorSetKey& setsKey, const gp::UInt64 currentFrame, VulkanCachedDescriptorSets& outSets
) noexcept
{
    for (gp::USize index = 0u; index < static_cast<gp::USize>(m_setsCache.size()); ++index)
    {
        const SetsEntry& entry = m_setsCache[index];
        if (entry.key == setsKey)
        {
            outSets = entry.sets;
            m_recentFrame = currentFrame;
            return true;
        }
    }
    return false;
}

bool VulkanCachedDescriptorPool::createDescriptorSets(
    const VulkanDescriptorSetKey& setsKey,
    const VulkanDescriptorSetsLayout& layout,
    VulkanDescriptorSetWriter* const writers,
    const gp::UInt32 numWriters,
    const gp::UInt64 currentFrame,
    VulkanCachedDescriptorSets& outSets
) noexcept
{
    GP_CHECK(numWriters <= static_cast<gp::UInt32>(kVulkanMaxNumDescriptorStages));
    const gp::Vector<VkDescriptorSetLayout>& handles = layout.getHandles();

    VulkanCachedDescriptorSets newSets{};
    newSets.numSets = numWriters;
    for (gp::UInt32 setIndex = 0u; setIndex < numWriters; ++setIndex)
    {
        VulkanDescriptorSetWriter& writer = writers[setIndex];
        if (writer.getNumWrites() == 0u)
        {
            newSets.sets[setIndex] = VK_NULL_HANDLE;
            continue;
        }

        // 1. Another binding may already have created this exact set.
        const VulkanDescriptorSetKey& setKey = writer.getKey();
        bool isFound = false;
        for (gp::USize index = 0u; index < static_cast<gp::USize>(m_setCache.size()); ++index)
        {
            if (m_setCache[index].key == setKey)
            {
                newSets.sets[setIndex] = m_setCache[index].set;
                isFound = true;
                break;
            }
        }
        if (isFound)
        {
            continue;
        }

        // 2. New content: allocate, then write it once. Its content never changes afterwards.
        if ((static_cast<gp::USize>(setIndex) >= static_cast<gp::USize>(handles.size())) ||
            (static_cast<gp::USize>(m_setCache.size()) >= static_cast<gp::USize>(m_setCapacity)) ||
            !m_pool.allocateDescriptorSet(handles[setIndex], newSets.sets[setIndex]))
        {
            return false;
        }

        SetEntry& setEntry = m_setCache.emplaceBack();
        setEntry.key = setKey;
        setEntry.set = newSets.sets[setIndex];

        writer.setDescriptorSet(newSets.sets[setIndex]);
#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
        static_cast<void>(writer.checkAllWritten());
#endif

        for (gp::USize slot = 0u; slot < kVulkanNumCoreDescriptorTypes; ++slot)
        {
            m_poolSizesStatistic[slot] += layout.getTypesUsed(getVulkanDescriptorTypeForSlot(slot));
        }

        if ((dynamic_api::vkUpdateDescriptorSets != nullptr) && (m_pool.getDevice().getHandle() != VK_NULL_HANDLE))
        {
            vkUpdateDescriptorSets(
                m_pool.getDevice().getHandle(), writer.getNumWrites(), writer.getWriteDescriptors(), 0u, nullptr
            );
        }
    }

    SetsEntry& setsEntry = m_setsCache.emplaceBack();
    setsEntry.key = setsKey;
    setsEntry.sets = newSets;

    outSets = newSets;
    m_recentFrame = currentFrame;
    return true;
}

VulkanDescriptorSetCache::VulkanDescriptorSetCache(VulkanDevice& device) noexcept
    : m_device(device)
{
    // The probe pool only serves to measure the allocation ratio of real content; later pools are sized from it.
    m_cachedPools.pushBack(
        gp::makeUnique<VulkanCachedDescriptorPool>(
            m_device, kVulkanDescriptorSetCacheProbePoolSize, kVulkanDefaultDescriptorPoolSizeRatios
        )
    );
}

bool VulkanDescriptorSetCache::getDescriptorSets(
    const VulkanDescriptorSetKey& setsKey,
    const VulkanDescriptorSetsLayout& layout,
    VulkanDescriptorSetWriter* const writers,
    const gp::UInt32 numWriters,
    const gp::UInt64 currentFrame,
    VkDescriptorSet* const outSets
) noexcept
{
    GP_CHECK(!m_cachedPools.isEmpty());
    VulkanCachedDescriptorSets sets{};

    const gp::USize maxLookups = static_cast<gp::USize>(getVulkanDescriptorSetCacheMaxPoolLookups());
    bool isFound = false;
    for (gp::USize index = 0u; (index < maxLookups) && (index < static_cast<gp::USize>(m_cachedPools.size())); ++index)
    {
        if (m_cachedPools[index]->findDescriptorSets(setsKey, currentFrame, sets))
        {
            isFound = true;
            break;
        }
    }

    if (!isFound)
    {
        bool isFirstAttempt = true;
        while (!m_cachedPools[0]->createDescriptorSets(setsKey, layout, writers, numWriters, currentFrame, sets))
        {
            if (!isFirstAttempt)
            {
                GP_LOG_ERROR(LogVulkanRHI, "Failed to create descriptor sets from a freshly created cached pool.");
                return false;
            }
            isFirstAttempt = false;
            updateAllocRatio();
            addCachedPool();
        }
    }

    for (gp::UInt32 index = 0u; index < numWriters; ++index)
    {
        outSets[index] = (index < sets.numSets) ? sets.sets[index] : VK_NULL_HANDLE;
    }
    return true;
}

void VulkanDescriptorSetCache::gc(const gp::UInt64 currentFrame) noexcept
{
    const gp::USize maxLookups = static_cast<gp::USize>(getVulkanDescriptorSetCacheMaxPoolLookups());
    const gp::UInt32 numFramesToWait = getVulkanNumFramesToWaitForDescriptorDelete();

    // A loop for safety against memory pressure; normally at most one pool ages out per call.
    while ((static_cast<gp::USize>(m_cachedPools.size()) > maxLookups) &&
           m_cachedPools.back()->canGC(currentFrame, numFramesToWait))
    {
        if (m_freePool.get() != nullptr)
        {
            GP_LOG_WARNING(
                LogVulkanRHI,
                "Descriptor set cache free pool is not empty; r.Vulkan.DSetCacheTargetSetsPerPool may be too small."
            );
        }
        m_freePool = std::move(m_cachedPools.back());
        m_cachedPools.popBack();
    }
}

void VulkanDescriptorSetCache::updateAllocRatio() noexcept
{
    const gp::Float32 filterParam = (m_poolAllocRatio > 0.0f) ? 2.0f : 0.0f;
    m_poolAllocRatio = ((m_poolAllocRatio * filterParam) + m_cachedPools[0]->calcAllocRatio()) / (filterParam + 1.0f);
}

void VulkanDescriptorSetCache::addCachedPool() noexcept
{
    GP_CHECK(m_poolAllocRatio > 0.0f);
    const gp::Float32 targetSetsPerPool = static_cast<gp::Float32>(getVulkanDescriptorSetCacheTargetSetsPerPool());
    gp::UInt32 maxDescriptorSets = static_cast<gp::UInt32>((targetSetsPerPool / m_poolAllocRatio) + 0.5f);
    if (maxDescriptorSets == 0u)
    {
        maxDescriptorSets = 1u;
    }

    if (m_freePool.get() != nullptr)
    {
        // Reuse the recycled pool when its capacity is close enough to the one wanted; allow it to be a little
        // smaller and quite a bit larger, since too large only wastes a little memory.
        constexpr gp::Float32 kMinErrorTolerance = -0.10f;
        constexpr gp::Float32 kMaxErrorTolerance = 0.50f;
        const gp::Float32 wanted = static_cast<gp::Float32>(maxDescriptorSets);
        const gp::Float32 error = (static_cast<gp::Float32>(m_freePool->getMaxDescriptorSets()) - wanted) / wanted;
        if ((error >= kMinErrorTolerance) && (error <= kMaxErrorTolerance))
        {
            m_freePool->reset();
            m_cachedPools.insert(m_cachedPools.cbegin(), std::move(m_freePool));
            return;
        }

        GP_LOG_INFO(
            LogVulkanRHI,
            "Descriptor set cache free pool size off by {} (tolerance [{}, {}]); creating a new pool.",
            static_cast<gp::Float64>(error),
            static_cast<gp::Float64>(kMinErrorTolerance),
            static_cast<gp::Float64>(kMaxErrorTolerance)
        );
        m_freePool.reset();
    }

    // Size the descriptor counts of the new pool from what the current pool actually held.
    VulkanDescriptorPoolSizeRatios poolSizeRatios{};
    m_cachedPools[0]->calcPoolSizesRatio(poolSizeRatios);
    m_cachedPools.insert(
        m_cachedPools.cbegin(), gp::makeUnique<VulkanCachedDescriptorPool>(m_device, maxDescriptorSets, poolSizeRatios)
    );
}

}   // namespace gp::rhi::vulkan
