// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanPendingTextureLocks.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::vulkan
{

void VulkanPendingTextureLockRegistry::add(
    const VulkanTexture* const texture, const VulkanPendingTextureLock& pendingLock
) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    GP_CHECKF(
        findLocked(texture, pendingLock.mipIndex, pendingLock.arrayLayer) == m_entries.size(),
        "Texture subresource already has a pending lock."
    );
    m_entries.emplaceBack(Entry{ texture, pendingLock });
}

bool VulkanPendingTextureLockRegistry::take(
    const VulkanTexture* const texture,
    const gp::UInt32 mipIndex,
    const gp::UInt32 arrayLayer,
    VulkanPendingTextureLock& outPendingLock
) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    const gp::USize index = findLocked(texture, mipIndex, arrayLayer);
    if (index == m_entries.size())
    {
        return false;
    }

    outPendingLock = m_entries[index].pendingLock;
    m_entries.removeAtSwap(index);
    return true;
}

bool VulkanPendingTextureLockRegistry::contains(
    const VulkanTexture* const texture, const gp::UInt32 mipIndex, const gp::UInt32 arrayLayer
) const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return findLocked(texture, mipIndex, arrayLayer) != m_entries.size();
}

gp::USize VulkanPendingTextureLockRegistry::size() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_entries.size();
}

gp::USize VulkanPendingTextureLockRegistry::findLocked(
    const VulkanTexture* const texture, const gp::UInt32 mipIndex, const gp::UInt32 arrayLayer
) const noexcept
{
    for (gp::USize index = 0u; index < m_entries.size(); ++index)
    {
        const Entry& entry = m_entries[index];
        if (entry.texture == texture && entry.pendingLock.mipIndex == mipIndex &&
            entry.pendingLock.arrayLayer == arrayLayer)
        {
            return index;
        }
    }
    return m_entries.size();
}

VulkanPendingTextureLockRegistry& getVulkanPendingTextureLocks() noexcept
{
    static VulkanPendingTextureLockRegistry s_registry{};
    return s_registry;
}

}   // namespace gp::rhi::vulkan
