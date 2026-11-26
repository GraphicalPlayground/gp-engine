// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "containers/arrays/Vector.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsEnums.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "hashing/Crc32.hpp"
#include "logging/LogMacros.hpp"
#include "memory/Memory.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "templates/NumericLimits.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "translation/VulkanShaderEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief CRC-32 of a byte range, chained from `seed`.
/// @param[in] data First byte.
/// @param[in] size Byte count.
/// @param[in] seed Previous CRC, or zero.
/// @return Chained CRC-32.
[[nodiscard]] gp::UInt32 chainCrc32(const void* const data, const gp::USize size, const gp::UInt32 seed) noexcept
{
    gp::hashing::Crc32 hasher(seed);
    hasher.update(data, size);
    return hasher.finalize();
}

/// @brief CRC-32 of one trivially copyable value, chained from `seed`.
/// @tparam InValueType Value type.
/// @param[in] value Hashed value.
/// @param[in] seed Previous CRC.
/// @return Chained CRC-32.
template <typename InValueType>
[[nodiscard]] gp::UInt32 chainCrc32Value(const InValueType& value, const gp::UInt32 seed) noexcept
{
    return chainCrc32(static_cast<const void*>(&value), sizeof(InValueType), seed);
}

/// @brief Increments a narrow counter, refusing to wrap.
/// @tparam InCounterType Unsigned counter type.
/// @param[in,out] value Counter.
/// @return False when the counter is already at its maximum.
template <typename InCounterType>
[[nodiscard]] bool incrementChecked(InCounterType& value) noexcept
{
    if (value == gp::NumericLimits<InCounterType>::max())
    {
        return false;
    }
    ++value;
    return true;
}

/// @brief One entry of the process-wide types usage registry.
struct TypesUsageEntry
{
    /// @brief CRC of the per-type counts; rejects most mismatches before the full compare.
    gp::UInt32 countsHash{ 0u };

    /// @brief Per-type counts; compared in full, since a CRC collision would size pools for the wrong shape.
    VulkanDescriptorTypeCounts counts{};

    /// @brief Dense id assigned to those counts.
    gp::UInt32 id{ 0u };
};

/// @brief Guards `getTypesUsageRegistry()`.
/// @return Registry mutex.
[[nodiscard]] gp::thread::Mutex& getTypesUsageMutex() noexcept
{
    static gp::thread::Mutex s_mutex{};
    return s_mutex;
}

/// @brief Registry mapping per-type count hashes to dense ids.
/// @return Registry; ids start at 1.
[[nodiscard]] gp::Vector<TypesUsageEntry>& getTypesUsageRegistry() noexcept
{
    static gp::Vector<TypesUsageEntry> s_registry{};
    return s_registry;
}

}   // namespace

void VulkanDescriptorSetsLayoutInfo::SetLayout::generateHash() noexcept
{
    hash = chainCrc32(
        static_cast<const void*>(layoutBindings.data()),
        static_cast<gp::USize>(layoutBindings.size()) * sizeof(VkDescriptorSetLayoutBinding),
        0u
    );
}

bool VulkanDescriptorSetsLayoutInfo::SetLayout::operator==(const SetLayout& rhs) const noexcept
{
    if ((hash != rhs.hash) || (layoutBindings.size() != rhs.layoutBindings.size()))
    {
        return false;
    }
    if (layoutBindings.isEmpty())
    {
        return true;
    }
    return gp::memory::compareMemory(
               layoutBindings.data(),
               rhs.layoutBindings.data(),
               static_cast<gp::USize>(layoutBindings.size()) * sizeof(VkDescriptorSetLayoutBinding)
           ) == 0;
}

bool VulkanDescriptorSetsLayoutInfo::StageInfo::isEmpty() const noexcept
{
    return types.isEmpty() && (packedGlobalsSize == 0u) && (numBoundUniformBuffers == 0u);
}

bool VulkanDescriptorSetsLayoutInfo::StageInfo::operator==(const StageInfo& rhs) const noexcept
{
    if ((packedGlobalsSize != rhs.packedGlobalsSize) || (numBoundUniformBuffers != rhs.numBoundUniformBuffers) ||
        (numBufferInfos != rhs.numBufferInfos) || (numImageInfos != rhs.numImageInfos) ||
        (numAccelerationStructures != rhs.numAccelerationStructures) || (types.size() != rhs.types.size()))
    {
        return false;
    }
    if (types.isEmpty())
    {
        return true;
    }
    return gp::memory::compareMemory(
               types.data(), rhs.types.data(), static_cast<gp::USize>(types.size()) * sizeof(VkDescriptorType)
           ) == 0;
}

bool VulkanDescriptorSetsLayoutInfo::finalizeBindings(
    const VkPipelineBindPoint bindPoint,
    const VkPhysicalDeviceLimits& limits,
    const VulkanUniformBufferGatherInfo& gatherInfo,
    const bool usesBindless,
    const VulkanDynamicUniformBufferMode dynamicMode
) noexcept
{
    const bool isCompute = (bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE);
    if (!isCompute && (bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor layouts only support graphics and compute bind points.");
        return false;
    }
    if ((m_typesUsageID != ~0u) || !m_setLayouts.isEmpty())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor layout bindings were already finalized.");
        return false;
    }

    // One binding struct is reused for every descriptor; zeroing it keeps padding deterministic for hashing.
    VkDescriptorSetLayoutBinding binding{};
    binding.descriptorCount = 1u;

    const bool convertAllToDynamic = !usesBindless && (dynamicMode == VulkanDynamicUniformBufferMode::All);
    const bool convertPackedToDynamic =
        !usesBindless && (convertAllToDynamic || (dynamicMode == VulkanDynamicUniformBufferMode::PackedGlobals));
    const gp::UInt32 maxDynamicUniformBuffers = limits.maxDescriptorSetUniformBuffersDynamic;
    const gp::USize dynamicSlot = getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    const gp::USize numStages = static_cast<gp::USize>(getNumStagesForBindPoint(bindPoint));

    for (gp::USize stage = 0u; stage < numStages; ++stage)
    {
        const VulkanShaderHeader* const header = gatherInfo.codeHeaders[stage];
        if (header == nullptr)
        {
            continue;
        }

        StageInfo& stageInfo = m_stageInfos[stage];
        binding.stageFlags =
            isCompute ? static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT)
                      : static_cast<VkShaderStageFlags>(gfxStageToVkStageBit(static_cast<shader_stage::Stage>(stage)));
        stageInfo.packedGlobalsSize = header->packedGlobalsSize;
        stageInfo.numBoundUniformBuffers = header->numBoundUniformBuffers;

        const gp::USize numBindings = static_cast<gp::USize>(header->bindings.size());
        for (gp::USize bindingIndex = 0u; bindingIndex < numBindings; ++bindingIndex)
        {
            const VkDescriptorType descriptorType =
                static_cast<VkDescriptorType>(header->bindings[bindingIndex].descriptorType);
            const bool isUniformBuffer = (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            const bool isPackedGlobals = isUniformBuffer && (header->packedGlobalsSize != 0u) && (bindingIndex == 0u);

            VkDescriptorType finalType = descriptorType;
            if (isPackedGlobals)
            {
                finalType = convertPackedToDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                                   : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
            else if (isUniformBuffer)
            {
                if (bindingIndex >= static_cast<gp::USize>(header->numBoundUniformBuffers))
                {
                    // Resource-only uniform buffer: its members are bound individually, the buffer itself is not.
                    continue;
                }
                if (convertAllToDynamic && (m_layoutTypes[dynamicSlot] < maxDynamicUniformBuffers))
                {
                    finalType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                }
            }

            // The set binding index must equal the shader binding index; skipped uniform buffers must come last.
            if (static_cast<gp::USize>(stageInfo.types.size()) != bindingIndex)
            {
                GP_LOG_ERROR(
                    LogVulkanRHI, "Stage {} binding {} does not follow the emitted binding order.", stage, bindingIndex
                );
                return false;
            }
            stageInfo.types.pushBack(finalType);

            binding.binding = static_cast<gp::UInt32>(bindingIndex);
            binding.descriptorType = finalType;
            if (!addDescriptor(stage, binding))
            {
                return false;
            }
        }
    }

    compileTypesUsageID();
    generateHash(bindPoint);
    return true;
}

bool VulkanDescriptorSetsLayoutInfo::finalizeBindings(
    const VkPipelineBindPoint bindPoint,
    const VkPhysicalDeviceLimits& limits,
    const VulkanUniformBufferGatherInfo& gatherInfo,
    const bool usesBindless
) noexcept
{
    return finalizeBindings(bindPoint, limits, gatherInfo, usesBindless, getVulkanDynamicUniformBufferMode());
}

bool VulkanDescriptorSetsLayoutInfo::addDescriptor(
    const gp::USize setIndex, const VkDescriptorSetLayoutBinding& binding
) noexcept
{
    const gp::USize slot = getVulkanDescriptorTypeSlot(binding.descriptorType);
    if ((slot == kVulkanInvalidDescriptorTypeSlot) || (setIndex >= kVulkanMaxNumDescriptorStages))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Unsupported descriptor type {}.", static_cast<gp::Int32>(binding.descriptorType));
        return false;
    }

    StageInfo& stageInfo = m_stageInfos[setIndex];
    if ((binding.binding >= static_cast<gp::UInt32>(stageInfo.types.size())) ||
        (stageInfo.types[binding.binding] != binding.descriptorType))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor binding {} does not match its stage type.", binding.binding);
        return false;
    }

    bool counted = true;
    switch (binding.descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        counted = incrementChecked(stageInfo.numImageInfos);
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        counted = incrementChecked(stageInfo.numBufferInfos);
        break;
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        counted = incrementChecked(stageInfo.numAccelerationStructures);
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        // Texel buffers point at the view handle directly; they need no info struct.
        break;
    default:
        GP_LOG_ERROR(
            LogVulkanRHI,
            "Descriptor type {} is not supported in layouts.",
            static_cast<gp::Int32>(binding.descriptorType)
        );
        return false;
    }
    if (!counted)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor info counter of stage {} overflowed.", setIndex);
        return false;
    }

    ++m_layoutTypes[slot];
    if (setIndex >= static_cast<gp::USize>(m_setLayouts.size()))
    {
        m_setLayouts.resize(setIndex + 1u);
    }
    m_setLayouts[setIndex].layoutBindings.pushBack(binding);
    return true;
}

void VulkanDescriptorSetsLayoutInfo::generateHash(const VkPipelineBindPoint bindPoint) noexcept
{
    const gp::UInt32 layoutCount = static_cast<gp::UInt32>(m_setLayouts.size());
    m_hash = chainCrc32Value(m_typesUsageID, layoutCount);
    for (SetLayout& setLayout: m_setLayouts)
    {
        setLayout.generateHash();
        m_hash = chainCrc32Value(setLayout.hash, m_hash);
    }

    const gp::USize numStages = static_cast<gp::USize>(getNumStagesForBindPoint(bindPoint));
    for (gp::USize stage = 0u; stage < numStages; ++stage)
    {
        const StageInfo& stageInfo = m_stageInfos[stage];
        m_hash = chainCrc32Value(stageInfo.packedGlobalsSize, m_hash);
        m_hash = chainCrc32Value(stageInfo.numBoundUniformBuffers, m_hash);
        m_hash = chainCrc32Value(stageInfo.numImageInfos, m_hash);
        m_hash = chainCrc32Value(stageInfo.numBufferInfos, m_hash);
        m_hash = chainCrc32Value(stageInfo.numAccelerationStructures, m_hash);
        m_hash = chainCrc32(
            static_cast<const void*>(stageInfo.types.data()),
            static_cast<gp::USize>(stageInfo.types.size()) * sizeof(VkDescriptorType),
            m_hash
        );
    }

    // Graphics and compute pipelines can declare identical bindings; the bind point keeps their hashes apart.
    m_bindPoint = bindPoint;
    m_hash = chainCrc32Value(m_bindPoint, m_hash);
}

gp::UInt32 VulkanDescriptorSetsLayoutInfo::getTypesUsed(const VkDescriptorType type) const noexcept
{
    const gp::USize slot = getVulkanDescriptorTypeSlot(type);
    return (slot == kVulkanInvalidDescriptorTypeSlot) ? 0u : m_layoutTypes[slot];
}

bool VulkanDescriptorSetsLayoutInfo::operator==(const VulkanDescriptorSetsLayoutInfo& rhs) const noexcept
{
    if ((m_hash != rhs.m_hash) || (m_bindPoint != rhs.m_bindPoint) ||
        (m_setLayouts.size() != rhs.m_setLayouts.size()) || (m_typesUsageID != rhs.m_typesUsageID))
    {
        return false;
    }
    for (gp::USize index = 0u; index < static_cast<gp::USize>(m_setLayouts.size()); ++index)
    {
        if (!(m_setLayouts[index] == rhs.m_setLayouts[index]))
        {
            return false;
        }
    }
    for (gp::USize stage = 0u; stage < kVulkanMaxNumDescriptorStages; ++stage)
    {
        if (!(m_stageInfos[stage] == rhs.m_stageInfos[stage]))
        {
            return false;
        }
    }
    return true;
}

void VulkanDescriptorSetsLayoutInfo::compileTypesUsageID() noexcept
{
    gp::UInt32 countsHash = 0u;
    for (const gp::UInt32 count: m_layoutTypes)
    {
        countsHash = chainCrc32Value(count, countsHash);
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(getTypesUsageMutex());
    gp::Vector<TypesUsageEntry>& registry = getTypesUsageRegistry();
    for (const TypesUsageEntry& entry: registry)
    {
        if ((entry.countsHash == countsHash) && (entry.counts == m_layoutTypes))
        {
            m_typesUsageID = entry.id;
            return;
        }
    }

    m_typesUsageID = static_cast<gp::UInt32>(registry.size()) + 1u;
    registry.pushBack(TypesUsageEntry{ countsHash, m_layoutTypes, m_typesUsageID });
}

}   // namespace gp::rhi::vulkan
