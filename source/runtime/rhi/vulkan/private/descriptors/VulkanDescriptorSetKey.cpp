// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetKey.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/Crc32.hpp"
#include "memory/Memory.hpp"

namespace gp::rhi::vulkan
{

void VulkanDescriptorSetKey::generateFromData(const void* const data, const gp::USize size) noexcept
{
    m_data.clear();
    m_hash = 0u;
    if ((data == nullptr) || (size == 0u))
    {
        return;
    }

    m_data.append(static_cast<const gp::UInt8*>(data), size);
    gp::hashing::Crc32 hasher;
    hasher.update(data, size);
    m_hash = hasher.finalize();
}

bool VulkanDescriptorSetKey::operator==(const VulkanDescriptorSetKey& rhs) const noexcept
{
    if ((m_hash != rhs.m_hash) || (m_data.size() != rhs.m_data.size()))
    {
        return false;
    }
    if (m_data.isEmpty())
    {
        return true;
    }
    return gp::memory::compareMemory(m_data.data(), rhs.m_data.data(), static_cast<gp::USize>(m_data.size())) == 0;
}

}   // namespace gp::rhi::vulkan
