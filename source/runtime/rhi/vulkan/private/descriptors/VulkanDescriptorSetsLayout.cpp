// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetLayoutCache.hpp"
#include "device/VulkanDevice.hpp"
#include "logging/LogMacros.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reads one per-type count.
/// @param[in] typeCounts Counts.
/// @param[in] type Descriptor type.
/// @return Count of `type`.
[[nodiscard]] gp::UInt32 countOf(const VulkanDescriptorTypeCounts& typeCounts, const VkDescriptorType type) noexcept
{
    return typeCounts[getVulkanDescriptorTypeSlot(type)];
}

/// @brief Logs and reports one broken limit.
/// @param[in] holds Result of the limit comparison.
/// @param[in] name Limit name.
/// @param[in] used Descriptors used.
/// @param[in] limit Device limit.
/// @return `holds`.
bool checkLimit(const bool holds, const char* const name, const gp::UInt32 used, const gp::UInt32 limit) noexcept
{
    if (!holds)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor layout uses {} descriptors but {} is {}.", used, name, limit);
    }
    return holds;
}

}   // namespace

VulkanDescriptorSetsLayout::VulkanDescriptorSetsLayout(VulkanDevice& device) noexcept
    : m_device(device)
{}

bool VulkanDescriptorSetsLayout::validateLimits(
    const VulkanDescriptorTypeCounts& typeCounts,
    const VkPhysicalDeviceLimits& limits,
    const GpuVendorId vendorId,
    const gp::UInt32 maxAccelerationStructures
) noexcept
{
    const gp::UInt32 samplers = countOf(typeCounts, VK_DESCRIPTOR_TYPE_SAMPLER) +
                                countOf(typeCounts, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    const gp::UInt32 uniformBuffers = countOf(typeCounts, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) +
                                      countOf(typeCounts, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    const gp::UInt32 dynamicUniformBuffers = countOf(typeCounts, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    const gp::UInt32 storageBuffers = countOf(typeCounts, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) +
                                      countOf(typeCounts, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    const gp::UInt32 dynamicStorageBuffers = countOf(typeCounts, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    const gp::UInt32 sampledImages = countOf(typeCounts, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) +
                                     countOf(typeCounts, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) +
                                     countOf(typeCounts, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
    const gp::UInt32 storageImages = countOf(typeCounts, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) +
                                     countOf(typeCounts, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
    const gp::UInt32 inputAttachments = countOf(typeCounts, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    const gp::UInt32 accelerationStructures = countOf(typeCounts, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

    bool isValid = true;
    isValid &= checkLimit(
        samplers <= limits.maxDescriptorSetSamplers,
        "maxDescriptorSetSamplers",
        samplers,
        limits.maxDescriptorSetSamplers
    );
    isValid &= checkLimit(
        uniformBuffers <= limits.maxDescriptorSetUniformBuffers,
        "maxDescriptorSetUniformBuffers",
        uniformBuffers,
        limits.maxDescriptorSetUniformBuffers
    );
    isValid &= checkLimit(
        (vendorId == GpuVendorId::Amd) || (dynamicUniformBuffers <= limits.maxDescriptorSetUniformBuffersDynamic),
        "maxDescriptorSetUniformBuffersDynamic",
        dynamicUniformBuffers,
        limits.maxDescriptorSetUniformBuffersDynamic
    );
    isValid &= checkLimit(
        storageBuffers <= limits.maxDescriptorSetStorageBuffers,
        "maxDescriptorSetStorageBuffers",
        storageBuffers,
        limits.maxDescriptorSetStorageBuffers
    );
    isValid &= checkLimit(
        dynamicStorageBuffers <= limits.maxDescriptorSetStorageBuffersDynamic,
        "maxDescriptorSetStorageBuffersDynamic",
        dynamicStorageBuffers,
        limits.maxDescriptorSetStorageBuffersDynamic
    );
    isValid &= checkLimit(
        sampledImages <= limits.maxDescriptorSetSampledImages,
        "maxDescriptorSetSampledImages",
        sampledImages,
        limits.maxDescriptorSetSampledImages
    );
    isValid &= checkLimit(
        storageImages <= limits.maxDescriptorSetStorageImages,
        "maxDescriptorSetStorageImages",
        storageImages,
        limits.maxDescriptorSetStorageImages
    );
    isValid &= checkLimit(
        inputAttachments <= limits.maxDescriptorSetInputAttachments,
        "maxDescriptorSetInputAttachments",
        inputAttachments,
        limits.maxDescriptorSetInputAttachments
    );
    isValid &= checkLimit(
        (accelerationStructures == 0u) || (accelerationStructures <= maxAccelerationStructures),
        "maxDescriptorSetAccelerationStructures",
        accelerationStructures,
        maxAccelerationStructures
    );
    return isValid;
}

bool VulkanDescriptorSetsLayout::compile(VulkanDescriptorSetLayoutCache& cache) noexcept
{
    if (m_isCompiled)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor sets layout compiled twice.");
        return false;
    }

    // Ray tracing is not enabled yet, so no acceleration structure limit is available.
    if (!validateLimits(m_layoutTypes, m_device.getLimits(), m_device.getVendorId(), 0u))
    {
        return false;
    }

    m_layoutHandles.clear();
    m_layoutHandleIds.clear();
    m_layoutHandles.reserve(m_setLayouts.size());
    m_layoutHandleIds.reserve(m_setLayouts.size());
    for (const SetLayout& setLayout: m_setLayouts)
    {
        VulkanDescriptorSetLayoutEntry entry{};
        if (!cache.findOrCreate(setLayout, entry))
        {
            m_layoutHandles.clear();
            m_layoutHandleIds.clear();
            return false;
        }
        m_layoutHandles.pushBack(entry.handle);
        m_layoutHandleIds.pushBack(entry.handleId);
    }

    if (m_typesUsageID == ~0u)
    {
        compileTypesUsageID();
    }

    zeroVulkanStruct(m_allocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    m_allocateInfo.descriptorSetCount = static_cast<gp::UInt32>(m_layoutHandles.size());
    m_allocateInfo.pSetLayouts = m_layoutHandles.data();
    m_isCompiled = true;
    return true;
}

}   // namespace gp::rhi::vulkan
