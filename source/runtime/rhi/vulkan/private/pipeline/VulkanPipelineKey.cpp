// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineKey.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/XxHash.hpp"
#include "memory/Memory.hpp"

namespace gp::rhi::vulkan
{

bool VulkanPipelineKey::operator==(const VulkanPipelineKey& rhs) const noexcept
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

VulkanPipelineKeyWriter::VulkanPipelineKeyWriter(VulkanPipelineKey& outKey) noexcept
    : m_key(outKey)
{
    m_key.m_data.clear();
    m_key.m_hash = 0u;
}

VulkanPipelineKeyWriter::~VulkanPipelineKeyWriter() noexcept
{
    if (m_key.m_data.isEmpty())
    {
        m_key.m_hash = 0u;
        return;
    }
    gp::hashing::XxHash64 hasher;
    hasher.update(m_key.m_data.data(), static_cast<gp::USize>(m_key.m_data.size()));
    m_key.m_hash = hasher.finalize();
}

void VulkanPipelineKeyWriter::writeBytes(const void* const data, const gp::USize size) noexcept
{
    if (size == 0u)
    {
        return;
    }
    m_key.m_data.append(static_cast<const gp::UInt8*>(data), size);
}

void VulkanPipelineKeyWriter::writeCount(const gp::USize count) noexcept
{
    write(static_cast<gp::Int32>(count));
}

}   // namespace gp::rhi::vulkan
