// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetWriter.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "states/VulkanSamplerState.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Number of bindings tracked by one validation mask word.
constexpr gp::UInt32 kBitsPerMaskWord = 32u;

/// @brief Assigns `source` to `destination`.
/// @tparam InT Copyable, equality-comparable type.
/// @param[in,out] destination Value to update.
/// @param[in] source New value.
/// @return True when the value changed.
template <typename InT>
bool copyAndReturnNotEqual(InT& destination, const InT source) noexcept
{
    const bool hasChanged = !(destination == source);
    destination = source;
    return hasChanged;
}

/// @brief Returns whether a layout is valid for a sampled image read.
/// @param[in] layout Image layout.
/// @return True for every read-only, shared present or general layout.
bool isSampledImageLayout(const VkImageLayout layout) noexcept
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_GENERAL:
        return true;
    default:
        return false;
    }
}

}   // namespace

gp::UInt32 VulkanDescriptorSetWriter::setupDescriptorWrites(
    const gp::ConstVectorView<VkDescriptorType> types,
    VulkanHashableDescriptorInfo* const hashableDescriptorInfos,
    VkWriteDescriptorSet* writeDescriptors,
    VkDescriptorImageInfo* imageInfos,
    VkDescriptorBufferInfo* bufferInfos,
    gp::UInt8* const bindingToDynamicOffsetMap,
    VkWriteDescriptorSetAccelerationStructureKHR* accelerationStructureWrites,
    VkAccelerationStructureKHR* accelerationStructures,
    const VulkanSamplerState& defaultSampler,
    const VulkanTextureView& defaultImageView
) noexcept
{
    m_hashableDescriptorInfos = hashableDescriptorInfos;
    m_writeDescriptors = writeDescriptors;
    m_numWrites = static_cast<gp::UInt32>(types.size());
    m_bindingToDynamicOffsetMap = bindingToDynamicOffsetMap;
    m_isKeyDirty = true;
    m_hasVolatileResources = false;

    initWrittenMasks(m_numWrites);

    gp::UInt32 dynamicOffsetIndex = 0u;
    for (gp::UInt32 index = 0u; index < m_numWrites; ++index)
    {
        const VkDescriptorType type = types[index];
        VkWriteDescriptorSet& write = writeDescriptors[index];
        zeroVulkanStruct(write, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        write.dstBinding = index;
        write.descriptorCount = 1u;
        write.descriptorType = type;

        switch (type)
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            GP_CHECK(bindingToDynamicOffsetMap != nullptr);
            bindingToDynamicOffsetMap[index] = static_cast<gp::UInt8>(dynamicOffsetIndex);
            ++dynamicOffsetIndex;
            write.pBufferInfo = bufferInfos++;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            write.pBufferInfo = bufferInfos++;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            // A sampler binding starts valid: the default sampler is a usable resource, unlike a default image.
            setWrittenBase(index);
            [[fallthrough]];
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            // `Texture.Load()` compiles to an image fetch that some drivers still route through a sampler, so every
            // image binding starts with a valid default sampler.
            if (hashableDescriptorInfos != nullptr)
            {
                hashableDescriptorInfos[index].image.samplerId = defaultSampler.getSamplerId();
                hashableDescriptorInfos[index].image.imageViewId = defaultImageView.viewId;
                hashableDescriptorInfos[index].image.imageLayout = static_cast<gp::UInt32>(VK_IMAGE_LAYOUT_GENERAL);
            }
            imageInfos->sampler = defaultSampler.getHandle();
            imageInfos->imageView = defaultImageView.view;
            imageInfos->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            write.pImageInfo = imageInfos++;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            // Texel buffer writes point straight at the caller's `VkBufferView`.
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            zeroVulkanStruct(
                *accelerationStructureWrites, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
            );
            *accelerationStructures = VK_NULL_HANDLE;
            accelerationStructureWrites->accelerationStructureCount = 1u;
            accelerationStructureWrites->pAccelerationStructures = accelerationStructures++;
            write.pNext = accelerationStructureWrites++;
            break;
        default:
            GP_CHECKF(false, "Unsupported descriptor type {}.", static_cast<gp::UInt32>(type));
            break;
        }
    }

    reset();
    return dynamicOffsetIndex;
}

bool VulkanDescriptorSetWriter::writeUniformBuffer(
    const gp::UInt32 descriptorIndex,
    const VkBuffer buffer,
    const gp::UInt32 handleId,
    const VkDeviceSize offset,
    const VkDeviceSize range
) noexcept
{
    return writeBuffer(descriptorIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer, handleId, offset, range, 0u);
}

bool VulkanDescriptorSetWriter::writeDynamicUniformBuffer(
    const gp::UInt32 descriptorIndex,
    const VkBuffer buffer,
    const gp::UInt32 handleId,
    const VkDeviceSize offset,
    const VkDeviceSize range,
    const gp::UInt32 dynamicOffset
) noexcept
{
    return writeBuffer(
        descriptorIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, buffer, handleId, offset, range, dynamicOffset
    );
}

bool VulkanDescriptorSetWriter::writeSampler(
    const gp::UInt32 descriptorIndex, const VulkanSamplerState& sampler
) noexcept
{
    GP_CHECK(descriptorIndex < m_numWrites);
    VkWriteDescriptorSet& write = m_writeDescriptors[descriptorIndex];
    GP_CHECK(
        (write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
        (write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    );
    VkDescriptorImageInfo* const imageInfo = const_cast<VkDescriptorImageInfo*>(write.pImageInfo);
    GP_CHECK(imageInfo != nullptr);
    setWritten(descriptorIndex);

    if (m_hashableDescriptorInfos == nullptr)
    {
        return copyAndReturnNotEqual(imageInfo->sampler, sampler.getHandle());
    }

    VulkanHashableDescriptorInfo& hashableInfo = m_hashableDescriptorInfos[descriptorIndex];
    GP_CHECK(sampler.getSamplerId() > 0u);
    bool hasChanged = false;
    if (hashableInfo.image.samplerId != sampler.getSamplerId())
    {
        hashableInfo.image.samplerId = sampler.getSamplerId();
        imageInfo->sampler = sampler.getHandle();
        hasChanged = true;
    }
    m_isKeyDirty = m_isKeyDirty || hasChanged;
    return hasChanged;
}

bool VulkanDescriptorSetWriter::writeImage(
    const gp::UInt32 descriptorIndex, const VulkanTextureView& textureView, const VkImageLayout layout
) noexcept
{
    return writeTextureView(descriptorIndex, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureView, layout);
}

bool VulkanDescriptorSetWriter::writeInputAttachment(
    const gp::UInt32 descriptorIndex, const VulkanTextureView& textureView, const VkImageLayout layout
) noexcept
{
    return writeTextureView(descriptorIndex, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, textureView, layout);
}

bool VulkanDescriptorSetWriter::writeStorageImage(
    const gp::UInt32 descriptorIndex, const VulkanTextureView& textureView, const VkImageLayout layout
) noexcept
{
    return writeTextureView(descriptorIndex, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, textureView, layout);
}

bool VulkanDescriptorSetWriter::writeStorageTexelBuffer(
    const gp::UInt32 descriptorIndex, const VulkanTypedBufferView& view
) noexcept
{
    return writeBufferView(descriptorIndex, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, view);
}

bool VulkanDescriptorSetWriter::writeUniformTexelBuffer(
    const gp::UInt32 descriptorIndex, const VulkanTypedBufferView& view
) noexcept
{
    return writeBufferView(descriptorIndex, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, view);
}

bool VulkanDescriptorSetWriter::writeStorageBuffer(
    const gp::UInt32 descriptorIndex, const VulkanStructuredBufferView& view
) noexcept
{
    return writeBuffer(
        descriptorIndex, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, view.buffer, view.handleId, view.offset, view.size, 0u
    );
}

bool VulkanDescriptorSetWriter::writeAccelerationStructure(
    const gp::UInt32 descriptorIndex, const VkAccelerationStructureKHR accelerationStructure
) noexcept
{
    GP_CHECKF(
        m_hashableDescriptorInfos == nullptr, "Acceleration structures are not supported by the descriptor set cache."
    );
    GP_CHECK(descriptorIndex < m_numWrites);
    const VkWriteDescriptorSet& write = m_writeDescriptors[descriptorIndex];
    GP_CHECK(write.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    setWritten(descriptorIndex);

    // The extension struct may not be first in the chain once other extensions are added, so walk it.
    const VkWriteDescriptorSetAccelerationStructureKHR* foundWrite = nullptr;
    const VkBaseInStructure* cursor = static_cast<const VkBaseInStructure*>(write.pNext);
    while (cursor != nullptr)
    {
        if (cursor->sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR)
        {
            foundWrite = reinterpret_cast<const VkWriteDescriptorSetAccelerationStructureKHR*>(cursor);
            break;
        }
        cursor = cursor->pNext;
    }

    GP_CHECKF(
        foundWrite != nullptr, "Acceleration structure write has no extension struct; see setupDescriptorWrites()."
    );
    if (foundWrite == nullptr)
    {
        return false;
    }
    GP_CHECK(foundWrite->accelerationStructureCount == 1u);

    VkAccelerationStructureKHR& destination =
        *const_cast<VkAccelerationStructureKHR*>(foundWrite->pAccelerationStructures);
    return copyAndReturnNotEqual(destination, accelerationStructure);
}

void VulkanDescriptorSetWriter::setDescriptorSet(const VkDescriptorSet descriptorSet) noexcept
{
    for (gp::UInt32 index = 0u; index < m_numWrites; ++index)
    {
        m_writeDescriptors[index].dstSet = descriptorSet;
    }
}

const VulkanDescriptorSetKey& VulkanDescriptorSetWriter::getKey() const noexcept
{
    GP_CHECKF(m_hashableDescriptorInfos != nullptr, "Descriptor set keys need the descriptor set cache path.");
    if (m_isKeyDirty)
    {
        // The trailing entry is the layout identity: equal resources under different layouts are different sets.
        m_key.generateFromData(
            m_hashableDescriptorInfos, sizeof(VulkanHashableDescriptorInfo) * (static_cast<gp::USize>(m_numWrites) + 1u)
        );
        m_isKeyDirty = false;
    }
    return m_key;
}

void VulkanDescriptorSetWriter::reset() noexcept
{
    m_hasVolatileResources = false;
#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
    m_writtenMask = m_baseWrittenMask;
#endif
}

bool VulkanDescriptorSetWriter::checkAllWritten() const noexcept
{
#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
    bool isComplete = true;
    for (gp::UInt32 index = 0u; index < m_numWrites; ++index)
    {
        const gp::UInt32 word = index / kBitsPerMaskWord;
        const gp::UInt32 bit = 1u << (index % kBitsPerMaskWord);
        if (((m_writtenMask[word] | m_excludedMask[word]) & bit) == 0u)
        {
            GP_LOG_WARNING(
                LogVulkanRHI,
                "Descriptor write {}/{} (type {}) was not written; the driver may crash on this draw.",
                index,
                m_numWrites,
                static_cast<gp::UInt32>(m_writeDescriptors[index].descriptorType)
            );
            isComplete = false;
        }
    }
    return isComplete;
#else
    return true;
#endif
}

void VulkanDescriptorSetWriter::setExcludedResources(
    const gp::ConstVectorView<gp::UInt32> excludedDescriptorIndices
) noexcept
{
#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
    for (gp::USize index = 0u; index < static_cast<gp::USize>(excludedDescriptorIndices.size()); ++index)
    {
        const gp::UInt32 descriptorIndex = excludedDescriptorIndices[index];
        GP_CHECK(descriptorIndex < m_numWrites);
        m_excludedMask[descriptorIndex / kBitsPerMaskWord] |= 1u << (descriptorIndex % kBitsPerMaskWord);
    }
#else
    static_cast<void>(excludedDescriptorIndices);
#endif
}

bool VulkanDescriptorSetWriter::writeBuffer(
    const gp::UInt32 descriptorIndex,
    const VkDescriptorType expectedType,
    const VkBuffer buffer,
    const gp::UInt32 handleId,
    const VkDeviceSize offset,
    const VkDeviceSize range,
    const gp::UInt32 dynamicOffset
) noexcept
{
    GP_CHECK(descriptorIndex < m_numWrites);
    const VkWriteDescriptorSet& write = m_writeDescriptors[descriptorIndex];
    if (expectedType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    {
        GP_CHECKF(
            (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) ||
                (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC),
            "Descriptor type mismatch at index {}: wrote {}, binding expects {}.",
            descriptorIndex,
            static_cast<gp::UInt32>(expectedType),
            static_cast<gp::UInt32>(write.descriptorType)
        );
    }
    else
    {
        GP_CHECKF(
            write.descriptorType == expectedType,
            "Descriptor type mismatch at index {}: wrote {}, binding expects {}.",
            descriptorIndex,
            static_cast<gp::UInt32>(expectedType),
            static_cast<gp::UInt32>(write.descriptorType)
        );
    }
    VkDescriptorBufferInfo* const bufferInfo = const_cast<VkDescriptorBufferInfo*>(write.pBufferInfo);
    GP_CHECK(bufferInfo != nullptr);
    setWritten(descriptorIndex);

    bool hasChanged = false;
    if (m_hashableDescriptorInfos != nullptr)
    {
        // Ids, not handles: a handle value can be recycled by the driver for a different buffer.
        VulkanHashableDescriptorInfo& hashableInfo = m_hashableDescriptorInfos[descriptorIndex];
        GP_CHECK(handleId > 0u);
        if (hashableInfo.buffer.id != handleId)
        {
            hashableInfo.buffer.id = handleId;
            bufferInfo->buffer = buffer;
            hasChanged = true;
        }
        if (hashableInfo.buffer.offset != static_cast<gp::UInt32>(offset))
        {
            hashableInfo.buffer.offset = static_cast<gp::UInt32>(offset);
            bufferInfo->offset = offset;
            hasChanged = true;
        }
        if (hashableInfo.buffer.range != static_cast<gp::UInt32>(range))
        {
            hashableInfo.buffer.range = static_cast<gp::UInt32>(range);
            bufferInfo->range = range;
            hasChanged = true;
        }
        m_isKeyDirty = m_isKeyDirty || hasChanged;
    }
    else
    {
        hasChanged = copyAndReturnNotEqual(bufferInfo->buffer, buffer);
        hasChanged = copyAndReturnNotEqual(bufferInfo->offset, offset) || hasChanged;
        hasChanged = copyAndReturnNotEqual(bufferInfo->range, range) || hasChanged;
    }

    if (expectedType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
    {
        GP_CHECK((m_bindingToDynamicOffsetMap != nullptr) && (m_dynamicOffsets != nullptr));
        m_dynamicOffsets[m_bindingToDynamicOffsetMap[descriptorIndex]] = dynamicOffset;
    }
    return hasChanged;
}

bool VulkanDescriptorSetWriter::writeTextureView(
    const gp::UInt32 descriptorIndex,
    const VkDescriptorType expectedType,
    const VulkanTextureView& textureView,
    const VkImageLayout layout
) noexcept
{
    GP_CHECK(descriptorIndex < m_numWrites);
    const VkWriteDescriptorSet& write = m_writeDescriptors[descriptorIndex];
    if (expectedType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
    {
        GP_CHECKF(
            (write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
                (write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
            "Descriptor type mismatch at index {}: wrote {}, binding expects {}.",
            descriptorIndex,
            static_cast<gp::UInt32>(expectedType),
            static_cast<gp::UInt32>(write.descriptorType)
        );
        GP_ENSURE(isSampledImageLayout(layout));
    }
    else
    {
        GP_CHECKF(
            write.descriptorType == expectedType,
            "Descriptor type mismatch at index {}: wrote {}, binding expects {}.",
            descriptorIndex,
            static_cast<gp::UInt32>(expectedType),
            static_cast<gp::UInt32>(write.descriptorType)
        );
    }
    VkDescriptorImageInfo* const imageInfo = const_cast<VkDescriptorImageInfo*>(write.pImageInfo);
    GP_CHECK(imageInfo != nullptr);
    setWritten(descriptorIndex);

    bool hasChanged = false;
    if (m_hashableDescriptorInfos != nullptr)
    {
        VulkanHashableDescriptorInfo& hashableInfo = m_hashableDescriptorInfos[descriptorIndex];
        GP_CHECK(textureView.viewId > 0u);
        if (hashableInfo.image.imageViewId != textureView.viewId)
        {
            hashableInfo.image.imageViewId = textureView.viewId;
            imageInfo->imageView = textureView.view;
            hasChanged = true;
        }
        if (hashableInfo.image.imageLayout != static_cast<gp::UInt32>(layout))
        {
            hashableInfo.image.imageLayout = static_cast<gp::UInt32>(layout);
            imageInfo->imageLayout = layout;
            hasChanged = true;
        }
        m_isKeyDirty = m_isKeyDirty || hasChanged;
    }
    else
    {
        hasChanged = copyAndReturnNotEqual(imageInfo->imageView, textureView.view);
        hasChanged = copyAndReturnNotEqual(imageInfo->imageLayout, layout) || hasChanged;
    }
    return hasChanged;
}

bool VulkanDescriptorSetWriter::writeBufferView(
    const gp::UInt32 descriptorIndex, const VkDescriptorType expectedType, const VulkanTypedBufferView& view
) noexcept
{
    GP_CHECK(descriptorIndex < m_numWrites);
    VkWriteDescriptorSet& write = m_writeDescriptors[descriptorIndex];
    GP_CHECKF(
        write.descriptorType == expectedType,
        "Descriptor type mismatch at index {}: wrote {}, binding expects {}.",
        descriptorIndex,
        static_cast<gp::UInt32>(expectedType),
        static_cast<gp::UInt32>(write.descriptorType)
    );
    setWritten(descriptorIndex);
    write.pTexelBufferView = &view.view;

    if (m_hashableDescriptorInfos == nullptr)
    {
        return true;
    }

    // A view without an id (a transient view) cannot be told apart from the next one: the set must not be cached.
    if (view.viewId == 0u)
    {
        m_hasVolatileResources = true;
        return true;
    }

    VulkanHashableDescriptorInfo& hashableInfo = m_hashableDescriptorInfos[descriptorIndex];
    bool hasChanged = false;
    if (hashableInfo.bufferView.id != view.viewId)
    {
        hashableInfo.bufferView.id = view.viewId;
        hasChanged = true;
    }
    m_isKeyDirty = m_isKeyDirty || hasChanged;
    return hasChanged;
}

void VulkanDescriptorSetWriter::setWritten(const gp::UInt32 descriptorIndex) noexcept
{
#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
    m_writtenMask[descriptorIndex / kBitsPerMaskWord] |= 1u << (descriptorIndex % kBitsPerMaskWord);
#else
    static_cast<void>(descriptorIndex);
#endif
}

void VulkanDescriptorSetWriter::setWrittenBase(const gp::UInt32 descriptorIndex) noexcept
{
#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
    m_baseWrittenMask[descriptorIndex / kBitsPerMaskWord] |= 1u << (descriptorIndex % kBitsPerMaskWord);
#else
    static_cast<void>(descriptorIndex);
#endif
}

void VulkanDescriptorSetWriter::initWrittenMasks(const gp::UInt32 numDescriptorWrites) noexcept
{
#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
    const gp::USize numWords = static_cast<gp::USize>((numDescriptorWrites + kBitsPerMaskWord - 1u) / kBitsPerMaskWord);
    m_writtenMask.clear();
    m_writtenMask.resize(numWords, 0u);
    m_baseWrittenMask.clear();
    m_baseWrittenMask.resize(numWords, 0u);
    m_excludedMask.clear();
    m_excludedMask.resize(numWords, 0u);
#else
    static_cast<void>(numDescriptorWrites);
#endif
}

}   // namespace gp::rhi::vulkan
