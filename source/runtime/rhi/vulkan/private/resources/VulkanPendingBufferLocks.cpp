// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanPendingBufferLocks.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "resources/VulkanBufferTypes.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::vulkan
{

void VulkanPendingBufferLockRegistry::add(
    const VulkanBuffer* const buffer, const VulkanPendingBufferLock& pendingLock
) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    GP_CHECKF(findLocked(buffer) == m_entries.size(), "Buffer already has a pending lock.");
    m_entries.emplaceBack(Entry{ buffer, pendingLock });
}

bool VulkanPendingBufferLockRegistry::take(
    const VulkanBuffer* const buffer, VulkanPendingBufferLock& outPendingLock
) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    const gp::USize index = findLocked(buffer);
    if (index == m_entries.size())
    {
        return false;
    }

    outPendingLock = m_entries[index].pendingLock;
    m_entries.removeAtSwap(index);
    return true;
}

bool VulkanPendingBufferLockRegistry::contains(const VulkanBuffer* const buffer) const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return findLocked(buffer) != m_entries.size();
}

gp::USize VulkanPendingBufferLockRegistry::size() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_entries.size();
}

gp::USize VulkanPendingBufferLockRegistry::findLocked(const VulkanBuffer* const buffer) const noexcept
{
    for (gp::USize index = 0u; index < m_entries.size(); ++index)
    {
        if (m_entries[index].buffer == buffer)
        {
            return index;
        }
    }
    return m_entries.size();
}

VulkanPendingBufferLockRegistry& getVulkanPendingBufferLocks() noexcept
{
    static VulkanPendingBufferLockRegistry s_registry{};
    return s_registry;
}

}   // namespace gp::rhi::vulkan

