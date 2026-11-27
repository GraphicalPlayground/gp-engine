// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanCommonPipelineDescriptorState.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetCache.hpp"
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/Memory.hpp"
#include "pendingstate/IVulkanPendingStateContext.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "views/VulkanView.hpp"
#include "views/VulkanViewEnums.hpp"
#include "views/VulkanViewTypes.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Marks a binding index without a dynamic offset in `bindingToDynamicOffsetMap`.
constexpr gp::UInt8 kNoDynamicOffset = 255u;

}   // namespace

VulkanCommonPipelineDescriptorState::VulkanCommonPipelineDescriptorState(
    VulkanDevice& device,
    const VulkanDescriptorSetsLayout& layout,
    const VkPipelineLayout pipelineLayout,
    const VulkanUniformBufferGatherInfo& codeHeaders,
    const VulkanDescriptorDefaults& defaults,
    const bool useDescriptorCache
) noexcept
    : m_device(device)
    , m_layout(layout)
    , m_pipelineLayout(pipelineLayout)
    , m_bindPoint(layout.getBindPoint())
    , m_codeHeaders(codeHeaders)
    , m_defaults(defaults)
    , m_useDescriptorCache(useDescriptorCache)
{
    GP_CHECKF(
        (m_defaults.defaultSampler != nullptr) && (m_defaults.defaultImageView != nullptr),
        "Descriptor states need a default sampler and a default image view."
    );
    m_numSets = static_cast<gp::UInt32>(m_layout.getLayouts().size());
    GP_CHECK(m_numSets <= static_cast<gp::UInt32>(kVulkanMaxNumDescriptorStages));

    createDescriptorWriteInfos();
    writeGlobalSamplers();

    for (gp::UInt32 set = 0u; set < m_numSets; ++set)
    {
        const VulkanDescriptorSetsLayoutInfo::StageInfo& stageInfo = m_layout.getStageInfo(set);
        if ((stageInfo.packedGlobalsSize != 0u) && !stageInfo.types.isEmpty())
        {
            m_packedUniformBuffers[set].initialize(stageInfo.packedGlobalsSize);
        }
    }
}

void VulkanCommonPipelineDescriptorState::createDescriptorWriteInfos() noexcept
{
    gp::USize numWrites = 0u;
    gp::USize numImageInfos = 0u;
    gp::USize numBufferInfos = 0u;
    gp::USize numAccelerationStructures = 0u;
    for (gp::UInt32 set = 0u; set < m_numSets; ++set)
    {
        const VulkanDescriptorSetsLayoutInfo::StageInfo& stageInfo = m_layout.getStageInfo(set);
        GP_CHECKF(stageInfo.types.size() < kNoDynamicOffset, "Set {} has too many bindings.", set);
        numWrites += static_cast<gp::USize>(stageInfo.types.size());
        numImageInfos += stageInfo.numImageInfos;
        numBufferInfos += stageInfo.numBufferInfos;
        numAccelerationStructures += stageInfo.numAccelerationStructures;
        if (!stageInfo.types.isEmpty())
        {
            m_usedSetsMask |= 1u << set;
        }
    }

    // Every array is sized once here: the writers keep raw pointers into them.
    m_writeContainer.descriptorWrites.resize(numWrites, VkWriteDescriptorSet{});
    m_writeContainer.descriptorImageInfo.resize(numImageInfos, VkDescriptorImageInfo{});
    m_writeContainer.descriptorBufferInfo.resize(numBufferInfos, VkDescriptorBufferInfo{});
    m_writeContainer.accelerationStructures.resize(
        numAccelerationStructures, VkAccelerationStructureKHR{ VK_NULL_HANDLE }
    );
    m_writeContainer.accelerationStructureWrites.resize(
        numAccelerationStructures, VkWriteDescriptorSetAccelerationStructureKHR{}
    );
    m_writeContainer.bindingToDynamicOffsetMap.resize(numWrites, kNoDynamicOffset);
    if (m_useDescriptorCache)
    {
        // One entry per binding plus one per set for the layout identity.
        m_writeContainer.hashableDescriptorInfo.resize(
            numWrites + static_cast<gp::USize>(m_numSets), VulkanHashableDescriptorInfo{}
        );
    }

    m_writers.resize(static_cast<gp::USize>(m_numSets));
    m_descriptorSetHandles.resize(static_cast<gp::USize>(m_numSets), VkDescriptorSet{ VK_NULL_HANDLE });

    VkWriteDescriptorSet* currentWrite = m_writeContainer.descriptorWrites.data();
    VkDescriptorImageInfo* currentImageInfo = m_writeContainer.descriptorImageInfo.data();
    VkDescriptorBufferInfo* currentBufferInfo = m_writeContainer.descriptorBufferInfo.data();
    VkAccelerationStructureKHR* currentAccelerationStructure = m_writeContainer.accelerationStructures.data();
    VkWriteDescriptorSetAccelerationStructureKHR* currentAccelerationStructureWrite =
        m_writeContainer.accelerationStructureWrites.data();
    gp::UInt8* currentDynamicOffsetMap = m_writeContainer.bindingToDynamicOffsetMap.data();
    VulkanHashableDescriptorInfo* currentHashableInfo =
        m_useDescriptorCache ? m_writeContainer.hashableDescriptorInfo.data() : nullptr;

    const gp::Vector<gp::UInt32>& layoutHandleIds = m_layout.getHandleIds();
    gp::UInt32 totalNumDynamicOffsets = 0u;
    for (gp::UInt32 set = 0u; set < m_numSets; ++set)
    {
        const VulkanDescriptorSetsLayoutInfo::StageInfo& stageInfo = m_layout.getStageInfo(set);
        const gp::USize numSetWrites = static_cast<gp::USize>(stageInfo.types.size());

        const gp::UInt32 numDynamicOffsets = m_writers[set].setupDescriptorWrites(
            gp::ConstVectorView<VkDescriptorType>(stageInfo.types),
            currentHashableInfo,
            currentWrite,
            currentImageInfo,
            currentBufferInfo,
            currentDynamicOffsetMap,
            currentAccelerationStructureWrite,
            currentAccelerationStructure,
            *m_defaults.defaultSampler,
            *m_defaults.defaultImageView
        );
        m_dynamicOffsetsStart[set] = totalNumDynamicOffsets;
        m_numDynamicOffsets[set] = numDynamicOffsets;
        totalNumDynamicOffsets += numDynamicOffsets;

        if (currentHashableInfo != nullptr)
        {
            // The trailing entry ties the key to the set layout: equal resources under another layout are another
            // set. An uncompiled layout has no ids yet and hashes as layout 0.
            currentHashableInfo += numSetWrites;
            currentHashableInfo->layout.max0 = ~0u;
            currentHashableInfo->layout.max1 = ~0u;
            currentHashableInfo->layout.layoutId =
                (set < static_cast<gp::UInt32>(layoutHandleIds.size())) ? layoutHandleIds[set] : 0u;
            ++currentHashableInfo;
        }

        currentWrite += numSetWrites;
        currentImageInfo += stageInfo.numImageInfos;
        currentBufferInfo += stageInfo.numBufferInfos;
        currentAccelerationStructure += stageInfo.numAccelerationStructures;
        currentAccelerationStructureWrite += stageInfo.numAccelerationStructures;
        currentDynamicOffsetMap += numSetWrites;
    }

    m_dynamicOffsets.resize(static_cast<gp::USize>(totalNumDynamicOffsets), 0u);
    for (gp::UInt32 set = 0u; set < m_numSets; ++set)
    {
        m_writers[set].setDynamicOffsets(m_dynamicOffsets.data() + m_dynamicOffsetsStart[set]);
    }
}

void VulkanCommonPipelineDescriptorState::writeGlobalSamplers() noexcept
{
    for (gp::UInt32 set = 0u; set < m_numSets; ++set)
    {
        const VulkanShaderHeader* const header = m_codeHeaders.codeHeaders[set];
        if (header == nullptr)
        {
            continue;
        }
        for (const VulkanShaderGlobalSamplerInfo& samplerInfo: header->globalSamplerInfos)
        {
            const gp::USize samplerIndex = static_cast<gp::USize>(samplerInfo.type);
            if (samplerIndex >= kVulkanNumGlobalSamplers)
            {
                continue;
            }
            const VulkanSamplerState* const sampler = m_defaults.globalSamplers[samplerIndex];
            if (sampler != nullptr)
            {
                (void)m_writers[set].writeSampler(samplerInfo.bindingIndex, *sampler);
            }
        }
    }
}

VulkanDescriptorSetWriter& VulkanCommonPipelineDescriptorState::getStageWriter(const shader_stage::Stage stage) noexcept
{
    const gp::UInt32 set = static_cast<gp::UInt32>(stage);
    GP_CHECKF(set < m_numSets, "Stage {} has no descriptor set in this layout.", set);
    return m_writers[set];
}

VkDescriptorType VulkanCommonPipelineDescriptorState::getBindingType(
    const shader_stage::Stage stage, const gp::UInt32 bindingIndex
) const noexcept
{
    const VulkanDescriptorSetsLayoutInfo::StageInfo& stageInfo = m_layout.getStageInfo(static_cast<gp::USize>(stage));
    GP_CHECKF(
        bindingIndex < static_cast<gp::UInt32>(stageInfo.types.size()),
        "Binding {} is outside the {} bindings of stage {}.",
        bindingIndex,
        stageInfo.types.size(),
        static_cast<gp::Int32>(stage)
    );
    return stageInfo.types[bindingIndex];
}

bool VulkanCommonPipelineDescriptorState::accumulateDirty(const bool hasChanged) noexcept
{
    m_isResourcesDirty = m_isResourcesDirty || hasChanged;
    return hasChanged;
}

bool VulkanCommonPipelineDescriptorState::setUniformBuffer(
    const shader_stage::Stage stage,
    const gp::UInt32 bindingIndex,
    const VkBuffer buffer,
    const gp::UInt32 handleId,
    const gp::UInt32 offset,
    const gp::UInt32 range
) noexcept
{
    VulkanDescriptorSetWriter& writer = getStageWriter(stage);
    if (getBindingType(stage, bindingIndex) == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
    {
        // The descriptor points at the buffer start; the offset travels with the bind command.
        return accumulateDirty(writer.writeDynamicUniformBuffer(bindingIndex, buffer, handleId, 0u, range, offset));
    }
    return accumulateDirty(writer.writeUniformBuffer(bindingIndex, buffer, handleId, offset, range));
}

bool VulkanCommonPipelineDescriptorState::setSampler(
    const shader_stage::Stage stage, const gp::UInt32 bindingIndex, const VulkanSamplerState& sampler
) noexcept
{
    return accumulateDirty(getStageWriter(stage).writeSampler(bindingIndex, sampler));
}

bool VulkanCommonPipelineDescriptorState::setTextureView(
    const shader_stage::Stage stage,
    const gp::UInt32 bindingIndex,
    const VulkanTextureView& textureView,
    const VkImageLayout layout
) noexcept
{
    VulkanDescriptorSetWriter& writer = getStageWriter(stage);
    switch (getBindingType(stage, bindingIndex))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return accumulateDirty(writer.writeImage(bindingIndex, textureView, layout));
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return accumulateDirty(writer.writeStorageImage(bindingIndex, textureView, layout));
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return accumulateDirty(writer.writeInputAttachment(bindingIndex, textureView, layout));
    default:
        GP_CHECKF(false, "Binding {} does not hold an image.", bindingIndex);
        break;
    }
    return false;
}

bool VulkanCommonPipelineDescriptorState::writeTypedBufferView(
    const shader_stage::Stage stage, const gp::UInt32 bindingIndex, const VulkanTypedBufferView& view
) noexcept
{
    VulkanDescriptorSetWriter& writer = getStageWriter(stage);
    if (getBindingType(stage, bindingIndex) == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    {
        return accumulateDirty(writer.writeStorageTexelBuffer(bindingIndex, view));
    }
    return accumulateDirty(writer.writeUniformTexelBuffer(bindingIndex, view));
}

bool VulkanCommonPipelineDescriptorState::setShaderResourceView(
    const shader_stage::Stage stage, const gp::UInt32 bindingIndex, VulkanView& view, const VkImageLayout imageLayout
) noexcept
{
    switch (view.getViewKind())
    {
    case VulkanViewKind::Texture:
        return setTextureView(stage, bindingIndex, view.getTextureView(), imageLayout);
    case VulkanViewKind::TypedBuffer:
        return writeTypedBufferView(stage, bindingIndex, view.getOrCreateTypedBufferView());
    case VulkanViewKind::StructuredBuffer:
        return accumulateDirty(getStageWriter(stage).writeStorageBuffer(bindingIndex, view.getStructuredBufferView()));
    case VulkanViewKind::AccelerationStructure:
        return setAccelerationStructure(stage, bindingIndex, view.getAccelerationStructureView().handle);
    case VulkanViewKind::Null:
    default:
        GP_LOG_WARNING(LogVulkanRHI, "Ignoring a null shader resource view at binding {}.", bindingIndex);
        break;
    }
    return false;
}

bool VulkanCommonPipelineDescriptorState::setUnorderedAccessView(
    const shader_stage::Stage stage, const gp::UInt32 bindingIndex, VulkanView& view
) noexcept
{
    switch (view.getViewKind())
    {
    case VulkanViewKind::Texture:
        // Storage images are always accessed in the general layout.
        return accumulateDirty(
            getStageWriter(stage).writeStorageImage(bindingIndex, view.getTextureView(), VK_IMAGE_LAYOUT_GENERAL)
        );
    case VulkanViewKind::TypedBuffer:
        return writeTypedBufferView(stage, bindingIndex, view.getOrCreateTypedBufferView());
    case VulkanViewKind::StructuredBuffer:
        return accumulateDirty(getStageWriter(stage).writeStorageBuffer(bindingIndex, view.getStructuredBufferView()));
    case VulkanViewKind::AccelerationStructure:
    case VulkanViewKind::Null:
    default:
        GP_LOG_WARNING(LogVulkanRHI, "Ignoring an unsupported unordered access view at binding {}.", bindingIndex);
        break;
    }
    return false;
}

bool VulkanCommonPipelineDescriptorState::setAccelerationStructure(
    const shader_stage::Stage stage,
    const gp::UInt32 bindingIndex,
    const VkAccelerationStructureKHR accelerationStructure
) noexcept
{
    return accumulateDirty(getStageWriter(stage).writeAccelerationStructure(bindingIndex, accelerationStructure));
}

bool VulkanCommonPipelineDescriptorState::setInputAttachment(
    const shader_stage::Stage stage,
    const gp::UInt32 bindingIndex,
    const VulkanTextureView& textureView,
    const VkImageLayout layout
) noexcept
{
    return accumulateDirty(getStageWriter(stage).writeInputAttachment(bindingIndex, textureView, layout));
}

void VulkanCommonPipelineDescriptorState::setPackedGlobalParameter(
    const shader_stage::Stage stage, const gp::UInt32 byteOffset, const gp::UInt32 numBytes, const void* const data
) noexcept
{
    const gp::USize set = static_cast<gp::USize>(stage);
    GP_CHECK(set < static_cast<gp::USize>(m_numSets));
    m_packedUniformBuffers[set].setParameter(byteOffset, numBytes, data);
}

void VulkanCommonPipelineDescriptorState::reset() noexcept
{
    for (VulkanPackedUniformBufferState& packedUniformBuffer: m_packedUniformBuffers)
    {
        packedUniformBuffer.reset();
    }
    m_isResourcesDirty = true;
}

const VulkanDescriptorSetWriter&
    VulkanCommonPipelineDescriptorState::getWriter(const gp::UInt32 setIndex) const noexcept
{
    GP_CHECK(setIndex < m_numSets);
    return m_writers[setIndex];
}

const VulkanPackedUniformBufferState&
    VulkanCommonPipelineDescriptorState::getPackedUniformBuffer(const shader_stage::Stage stage) const noexcept
{
    const gp::USize index = static_cast<gp::USize>(stage);
    GP_CHECK(index < kVulkanMaxNumDescriptorStages);
    return m_packedUniformBuffers[index];
}

bool VulkanCommonPipelineDescriptorState::hasVolatileResources() const noexcept
{
    for (const VulkanDescriptorSetWriter& writer: m_writers)
    {
        if (writer.hasVolatileResources())
        {
            return true;
        }
    }
    return false;
}

bool VulkanCommonPipelineDescriptorState::uploadPackedUniformBuffers(IVulkanPendingStateContext& context) noexcept
{
    for (gp::UInt32 set = 0u; set < m_numSets; ++set)
    {
        VulkanPackedUniformBufferState& packedUniformBuffer = m_packedUniformBuffers[set];
        if (!packedUniformBuffer.isDirty())
        {
            continue;
        }

        const gp::UInt32 size = packedUniformBuffer.getSize();
        VulkanUniformUpload upload{};
        if (!context.allocateUniformMemory(size, upload) || (upload.mappedData == nullptr))
        {
            GP_LOG_ERROR(LogVulkanRHI, "Out of transient memory for a {}-byte packed uniform buffer.", size);
            return false;
        }
        gp::memory::copyMemory(upload.mappedData, packedUniformBuffer.getData(), static_cast<gp::USize>(size));

        // Binding 0 of a stage with packed globals is always the packed buffer.
        VulkanDescriptorSetWriter& writer = m_writers[set];
        const VkDescriptorType type = m_layout.getStageInfo(set).types[0];
        if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            (void)accumulateDirty(
                writer.writeDynamicUniformBuffer(0u, upload.buffer, upload.handleId, 0u, size, upload.offset)
            );
        }
        else
        {
            (void)accumulateDirty(writer.writeUniformBuffer(0u, upload.buffer, upload.handleId, upload.offset, size));
        }
        packedUniformBuffer.clearDirty();
    }
    return true;
}

bool VulkanCommonPipelineDescriptorState::updateDescriptorSets(IVulkanPendingStateContext& context) noexcept
{
    if (m_usedSetsMask == 0u)
    {
        return false;
    }
    if (!uploadPackedUniformBuffers(context))
    {
        return false;
    }

    VulkanDescriptorSetCache* const cache = m_useDescriptorCache ? context.getDescriptorSetCache() : nullptr;
    if ((cache != nullptr) && !hasVolatileResources())
    {
        if (m_isResourcesDirty)
        {
            m_setsKey.generateFromData(
                m_writeContainer.hashableDescriptorInfo.data(),
                sizeof(VulkanHashableDescriptorInfo) *
                    static_cast<gp::USize>(m_writeContainer.hashableDescriptorInfo.size())
            );
            if (!cache->getDescriptorSets(
                    m_setsKey,
                    m_layout,
                    m_writers.data(),
                    m_numSets,
                    context.getCurrentFrame(),
                    m_descriptorSetHandles.data()
                ))
            {
                GP_LOG_ERROR(LogVulkanRHI, "The descriptor set cache could not provide {} sets.", m_numSets);
                return false;
            }
            m_isResourcesDirty = false;
        }
        return true;
    }

    const bool needsWrite = m_isResourcesDirty || shouldVulkanAlwaysWriteDescriptors();
    if (context.acquirePoolSetAndDescriptorsIfNeeded(m_layout, needsWrite, m_descriptorSetHandles.data()))
    {
        for (gp::UInt32 set = 0u; set < m_numSets; ++set)
        {
            if ((m_usedSetsMask & (1u << set)) != 0u)
            {
                m_writers[set].setDescriptorSet(m_descriptorSetHandles[set]);
            }
        }
        if ((m_device.getHandle() != VK_NULL_HANDLE) && (dynamic_api::vkUpdateDescriptorSets != nullptr) &&
            !m_writeContainer.descriptorWrites.isEmpty())
        {
            vkUpdateDescriptorSets(
                m_device.getHandle(),
                static_cast<gp::UInt32>(m_writeContainer.descriptorWrites.size()),
                m_writeContainer.descriptorWrites.data(),
                0u,
                nullptr
            );
        }
        m_isResourcesDirty = false;
        return true;
    }

    // No new sets: fine when nothing changed, a failed allocation otherwise.
    return !needsWrite;
}

void VulkanCommonPipelineDescriptorState::bindDescriptorSets(const VkCommandBuffer commandBuffer) const noexcept
{
    if ((commandBuffer == VK_NULL_HANDLE) || (m_pipelineLayout == VK_NULL_HANDLE) ||
        (dynamic_api::vkCmdBindDescriptorSets == nullptr))
    {
        return;
    }

    gp::UInt32 set = 0u;
    while (set < m_numSets)
    {
        if (m_descriptorSetHandles[set] == VK_NULL_HANDLE)
        {
            ++set;
            continue;
        }

        const gp::UInt32 firstSet = set;
        const gp::UInt32 firstDynamicOffset = m_dynamicOffsetsStart[set];
        gp::UInt32 numDynamicOffsets = 0u;
        while ((set < m_numSets) && (m_descriptorSetHandles[set] != VK_NULL_HANDLE))
        {
            numDynamicOffsets += m_numDynamicOffsets[set];
            ++set;
        }

        vkCmdBindDescriptorSets(
            commandBuffer,
            m_bindPoint,
            m_pipelineLayout,
            firstSet,
            set - firstSet,
            m_descriptorSetHandles.data() + firstSet,
            numDynamicOffsets,
            (numDynamicOffsets != 0u) ? m_dynamicOffsets.data() + firstDynamicOffset : nullptr
        );
    }
}

}   // namespace gp::rhi::vulkan
