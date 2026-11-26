// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderFactory.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/Crc32.hpp"
#include "logging/LogMacros.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Stage frequencies of the graphics pipeline, indexed by `shader_stage::Stage`.
constexpr shader::ShaderFrequency kGraphicsStageFrequencies[VulkanShaderFactory::kNumGraphicsStages] = {
    shader::ShaderFrequency::Vertex,
    shader::ShaderFrequency::Pixel,
    shader::ShaderFrequency::Geometry,
    shader::ShaderFrequency::Mesh,
    shader::ShaderFrequency::Amplification
};

/// @brief Returns the first entry whose key is not less than `key`.
/// @tparam InEntry Entry type with a `key` member.
/// @param[in] entries Entries sorted by key.
/// @param[in] key Searched key.
/// @return Insertion index in `[0, entries.size()]`.
template <typename InEntry>
[[nodiscard]] gp::USize lowerBound(const gp::Vector<InEntry>& entries, const gp::UInt64 key) noexcept
{
    gp::USize first = 0u;
    gp::USize count = static_cast<gp::USize>(entries.size());
    while (count > 0u)
    {
        const gp::USize half = count / 2u;
        const gp::USize middle = first + half;
        if (entries[middle].key < key)
        {
            first = middle + 1u;
            count -= half + 1u;
        }
        else
        {
            count = half;
        }
    }
    return first;
}

}   // namespace

VulkanShaderFactory::~VulkanShaderFactory() noexcept
{
    gp::USize remaining = 0u;
    for (const gp::Vector<Entry>& entries: m_shaders)
    {
        remaining += static_cast<gp::USize>(entries.size());
    }
    if (remaining != 0u)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shader factory destroyed with {} shaders still registered.", remaining);
    }
}

gp::UInt64 VulkanShaderFactory::computeShaderKey(const gp::ConstVectorView<gp::UInt8> code) noexcept
{
    gp::hashing::Crc32 hasher;
    hasher.update(static_cast<const void*>(code.data()), static_cast<gp::USize>(code.size()));
    const gp::UInt32 crc = hasher.finalize();
    return (static_cast<gp::UInt64>(code.size()) & 0xFFFFFFFFull) | (static_cast<gp::UInt64>(crc) << 32u);
}

void VulkanShaderFactory::lookupGfxShaders(
    const gp::Array<gp::UInt64, kNumGraphicsStages>& shaderKeys,
    gp::Array<VulkanShader*, kNumGraphicsStages>& outShaders
) const noexcept
{
    gp::thread::SharedLock<gp::thread::SharedMutex> readLock(m_mutex);
    for (gp::USize stage = 0u; stage < kNumGraphicsStages; ++stage)
    {
        outShaders[stage] = nullptr;
        if (shaderKeys[stage] == 0u)
        {
            continue;
        }
        const Entry* const entry = findLocked(kGraphicsStageFrequencies[stage], shaderKeys[stage]);
        if (entry != nullptr)
        {
            outShaders[stage] = entry->vulkanShader;
        }
    }
}

void VulkanShaderFactory::onDeleteShader(const VulkanShader& shader) noexcept
{
    const gp::USize frequencyIndex = static_cast<gp::USize>(shader.getVulkanFrequency());
    gp::thread::UniqueLock<gp::thread::SharedMutex> writeLock(m_mutex);
    gp::Vector<Entry>& entries = m_shaders[frequencyIndex];
    const gp::USize index = lowerBound(entries, shader.getShaderKey());
    if ((index < static_cast<gp::USize>(entries.size())) && (entries[index].vulkanShader == &shader))
    {
        entries.removeAt(index);
    }
}

gp::USize VulkanShaderFactory::getNumShaders(const shader::ShaderFrequency frequency) const noexcept
{
    gp::thread::SharedLock<gp::thread::SharedMutex> readLock(m_mutex);
    return static_cast<gp::USize>(m_shaders[static_cast<gp::USize>(frequency)].size());
}

const VulkanShaderFactory::Entry*
    VulkanShaderFactory::findLocked(const shader::ShaderFrequency frequency, const gp::UInt64 shaderKey) const noexcept
{
    const gp::Vector<Entry>& entries = m_shaders[static_cast<gp::USize>(frequency)];
    const gp::USize index = lowerBound(entries, shaderKey);
    if ((index < static_cast<gp::USize>(entries.size())) && (entries[index].key == shaderKey))
    {
        return &entries[index];
    }
    return nullptr;
}

void VulkanShaderFactory::insertLocked(
    const shader::ShaderFrequency frequency,
    const gp::UInt64 shaderKey,
    VulkanShader& vulkanShader,
    shader::Shader& rhiShader
) noexcept
{
    gp::Vector<Entry>& entries = m_shaders[static_cast<gp::USize>(frequency)];
    const gp::USize index = lowerBound(entries, shaderKey);
    if ((index < static_cast<gp::USize>(entries.size())) && (entries[index].key == shaderKey))
    {
        // The previous shader with this key is being destroyed; its destructor will no longer match the entry.
        entries[index].vulkanShader = &vulkanShader;
        entries[index].rhiShader = &rhiShader;
        return;
    }
    entries.insertAt(index, Entry{ shaderKey, &vulkanShader, &rhiShader });
}

}   // namespace gp::rhi::vulkan
