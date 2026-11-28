// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "containers/ContainerForward.hpp"               // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetKey.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "RHIVulkan.hpp"
#include "states/VulkanStateForward.hpp"   // IWYU pragma: keep
#include "views/VulkanViewForward.hpp"     // IWYU pragma: keep
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"            // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Fills the `VkWriteDescriptorSet` array of one descriptor set, binding by binding.
/// @details The writer owns no storage: `setupDescriptorWrites()` points it at sub-ranges of a
///          `VulkanDescriptorSetWriteContainer`, initializes one write per binding with default resources, and wires
///          every write to its image info, buffer info or acceleration structure extension. Each `write*()` call then
///          patches one binding and returns whether it changed, so callers skip `vkUpdateDescriptorSets` and set
///          allocation when nothing moved since the previous draw.
///
///          With a hashable info array (the descriptor set cache path), the writer compares resource *ids* instead of
///          raw handles and maintains a byte key (`getKey()`) that identifies the set contents; the trailing hashable
///          entry holds the layout identity. Without one, it compares the native handles directly.
///
///          In debug and development builds, the writer tracks which bindings were written since `reset()` so
///          `checkAllWritten()` can report a draw that leaves a descriptor unbound, which drivers do not forgive.
/// @note Not thread-safe; a writer belongs to a single pipeline state on one context.
class GP_RHI_VULKAN_API VulkanDescriptorSetWriter
{
public:
    /// @brief Creates a writer bound to no storage.
    VulkanDescriptorSetWriter() noexcept = default;

    /// @brief Points the writer at its storage and initializes one write per binding.
    /// @details Uniform and storage buffers consume one buffer info each, image types and samplers one image info
    ///          initialized to the default sampler and view in `VK_IMAGE_LAYOUT_GENERAL` (an image read through
    ///          `Load()` still needs a valid sampler), acceleration structures one extension struct chained through
    ///          `pNext`. Texel buffers consume nothing: their write points at the caller's view.
    /// @param[in] types Descriptor type of each binding, in binding order.
    /// @param[in,out] hashableDescriptorInfos `types.size() + 1` entries for the descriptor set cache, or null.
    /// @param[out] writeDescriptors `types.size()` write structs.
    /// @param[out] imageInfos One image info per image, sampler or input attachment binding.
    /// @param[out] bufferInfos One buffer info per uniform or storage buffer binding.
    /// @param[out] bindingToDynamicOffsetMap One entry per binding; receives the dynamic offset index of each dynamic
    ///             uniform buffer.
    /// @param[out] accelerationStructureWrites One extension struct per acceleration structure binding.
    /// @param[out] accelerationStructures One handle per acceleration structure binding.
    /// @param[in] defaultSampler Sampler bound until a binding is written.
    /// @param[in] defaultImageView Image view bound until a binding is written.
    /// @return Number of dynamic uniform buffers, i.e. of dynamic offsets the bind call expects.
    gp::UInt32 setupDescriptorWrites(
        gp::ConstVectorView<VkDescriptorType> types,
        VulkanHashableDescriptorInfo* hashableDescriptorInfos,
        VkWriteDescriptorSet* writeDescriptors,
        VkDescriptorImageInfo* imageInfos,
        VkDescriptorBufferInfo* bufferInfos,
        gp::UInt8* bindingToDynamicOffsetMap,
        VkWriteDescriptorSetAccelerationStructureKHR* accelerationStructureWrites,
        VkAccelerationStructureKHR* accelerationStructures,
        const VulkanSamplerState& defaultSampler,
        const VulkanTextureView& defaultImageView
    ) noexcept;

    /// @brief Points the writer at the dynamic offsets array written by dynamic uniform buffer writes.
    /// @param[in] dynamicOffsets One entry per dynamic uniform buffer of the set; may be null when it has none.
    void setDynamicOffsets(gp::UInt32* dynamicOffsets) noexcept
    {
        m_dynamicOffsets = dynamicOffsets;
    }

    /// @brief Writes a uniform buffer binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] buffer Native buffer.
    /// @param[in] handleId Unique id of `buffer`; must be non-zero on the cache path.
    /// @param[in] offset Byte offset of the bound range.
    /// @param[in] range Byte size of the bound range.
    /// @return True when the binding changed.
    bool writeUniformBuffer(
        gp::UInt32 descriptorIndex, VkBuffer buffer, gp::UInt32 handleId, VkDeviceSize offset, VkDeviceSize range
    ) noexcept;

    /// @brief Writes a dynamic uniform buffer binding and its dynamic offset.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] buffer Native buffer.
    /// @param[in] handleId Unique id of `buffer`; must be non-zero on the cache path.
    /// @param[in] offset Byte offset of the bound range.
    /// @param[in] range Byte size of the bound range.
    /// @param[in] dynamicOffset Offset added at bind time.
    /// @return True when the descriptor changed; the dynamic offset alone never requires an update.
    bool writeDynamicUniformBuffer(
        gp::UInt32 descriptorIndex,
        VkBuffer buffer,
        gp::UInt32 handleId,
        VkDeviceSize offset,
        VkDeviceSize range,
        gp::UInt32 dynamicOffset
    ) noexcept;

    /// @brief Writes the sampler of a sampler or combined image sampler binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] sampler Sampler state.
    /// @return True when the binding changed.
    bool writeSampler(gp::UInt32 descriptorIndex, const VulkanSamplerState& sampler) noexcept;

    /// @brief Writes the image of a sampled image or combined image sampler binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] textureView Native view; must outlive the descriptor update.
    /// @param[in] layout Layout the image is in when the shader reads it.
    /// @return True when the binding changed.
    bool writeImage(gp::UInt32 descriptorIndex, const VulkanTextureView& textureView, VkImageLayout layout) noexcept;

    /// @brief Writes an input attachment binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] textureView Native view.
    /// @param[in] layout Layout the attachment is in when the shader reads it.
    /// @return True when the binding changed.
    bool writeInputAttachment(
        gp::UInt32 descriptorIndex, const VulkanTextureView& textureView, VkImageLayout layout
    ) noexcept;

    /// @brief Writes a storage image binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] textureView Native view.
    /// @param[in] layout Layout the image is in when the shader accesses it.
    /// @return True when the binding changed.
    bool writeStorageImage(
        gp::UInt32 descriptorIndex, const VulkanTextureView& textureView, VkImageLayout layout
    ) noexcept;

    /// @brief Writes a storage texel buffer binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] view Typed buffer view; the write points at `view.view`, so it must outlive the update.
    /// @return True when the binding changed; always true off the cache path or for a volatile view.
    bool writeStorageTexelBuffer(gp::UInt32 descriptorIndex, const VulkanTypedBufferView& view) noexcept;

    /// @brief Writes a uniform texel buffer binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] view Typed buffer view; the write points at `view.view`, so it must outlive the update.
    /// @return True when the binding changed; always true off the cache path or for a volatile view.
    bool writeUniformTexelBuffer(gp::UInt32 descriptorIndex, const VulkanTypedBufferView& view) noexcept;

    /// @brief Writes a storage buffer binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] view Structured buffer view.
    /// @return True when the binding changed.
    bool writeStorageBuffer(gp::UInt32 descriptorIndex, const VulkanStructuredBufferView& view) noexcept;

    /// @brief Writes an acceleration structure binding.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] accelerationStructure Native top-level acceleration structure.
    /// @return True when the binding changed.
    /// @note Not supported on the descriptor set cache path: acceleration structures carry no resource id yet.
    bool writeAccelerationStructure(
        gp::UInt32 descriptorIndex, VkAccelerationStructureKHR accelerationStructure
    ) noexcept;

    /// @brief Sets the destination set of every write.
    /// @param[in] descriptorSet Set the next `vkUpdateDescriptorSets` call writes.
    void setDescriptorSet(VkDescriptorSet descriptorSet) noexcept;

    /// @brief Returns the key of the current set contents, regenerating it when a write changed it.
    /// @return Key over the hashable infos and the trailing layout entry.
    /// @note Only valid on the descriptor set cache path.
    [[nodiscard]] const VulkanDescriptorSetKey& getKey() const noexcept;

    /// @brief Returns the write structs.
    /// @return First write, or null before `setupDescriptorWrites()`.
    [[nodiscard]] const VkWriteDescriptorSet* getWriteDescriptors() const noexcept
    {
        return m_writeDescriptors;
    }

    /// @brief Returns the number of writes, i.e. of bindings.
    /// @return Write count.
    [[nodiscard]] gp::UInt32 getNumWrites() const noexcept
    {
        return m_numWrites;
    }

    /// @brief Returns whether a written resource has no stable id, which forbids caching the set.
    /// @return True after a write of a volatile texel buffer view since the last `reset()`.
    [[nodiscard]] bool hasVolatileResources() const noexcept
    {
        return m_hasVolatileResources;
    }

    /// @brief Starts a new draw: clears the volatile flag and the written mask back to the defaults.
    void reset() noexcept;

    /// @brief Reports every binding not written since `reset()`.
    /// @return True when every binding was written or excluded; always true when validation is compiled out.
    bool checkAllWritten() const noexcept;

    /// @brief Excludes bindings from `checkAllWritten()`, for resources a shader declares but never reads.
    /// @param[in] excludedDescriptorIndices Binding indices to exclude.
    void setExcludedResources(gp::ConstVectorView<gp::UInt32> excludedDescriptorIndices) noexcept;

private:
    /// @brief Shared body of the uniform, dynamic uniform and storage buffer writes.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] expectedType Type the caller writes.
    /// @param[in] buffer Native buffer.
    /// @param[in] handleId Unique id of `buffer`.
    /// @param[in] offset Byte offset of the bound range.
    /// @param[in] range Byte size of the bound range.
    /// @param[in] dynamicOffset Offset stored for a dynamic uniform buffer.
    /// @return True when the binding changed.
    bool writeBuffer(
        gp::UInt32 descriptorIndex,
        VkDescriptorType expectedType,
        VkBuffer buffer,
        gp::UInt32 handleId,
        VkDeviceSize offset,
        VkDeviceSize range,
        gp::UInt32 dynamicOffset
    ) noexcept;

    /// @brief Shared body of the image, input attachment and storage image writes.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] expectedType Type the caller writes.
    /// @param[in] textureView Native view.
    /// @param[in] layout Image layout.
    /// @return True when the binding changed.
    bool writeTextureView(
        gp::UInt32 descriptorIndex,
        VkDescriptorType expectedType,
        const VulkanTextureView& textureView,
        VkImageLayout layout
    ) noexcept;

    /// @brief Shared body of the texel buffer writes.
    /// @param[in] descriptorIndex Binding index.
    /// @param[in] expectedType Type the caller writes.
    /// @param[in] view Typed buffer view.
    /// @return True when the binding changed.
    bool writeBufferView(
        gp::UInt32 descriptorIndex, VkDescriptorType expectedType, const VulkanTypedBufferView& view
    ) noexcept;

    /// @brief Marks a binding written for the current draw.
    /// @param[in] descriptorIndex Binding index.
    void setWritten(gp::UInt32 descriptorIndex) noexcept;

    /// @brief Marks a binding written by default, so `reset()` keeps it written.
    /// @param[in] descriptorIndex Binding index.
    void setWrittenBase(gp::UInt32 descriptorIndex) noexcept;

    /// @brief Sizes the validation masks for a binding count and clears them.
    /// @param[in] numDescriptorWrites Binding count.
    void initWrittenMasks(gp::UInt32 numDescriptorWrites) noexcept;

private:
    /// @brief Write structs, in the container's storage.
    VkWriteDescriptorSet* m_writeDescriptors{ nullptr };

    /// @brief Binding to dynamic offset index map, in the container's storage.
    gp::UInt8* m_bindingToDynamicOffsetMap{ nullptr };

    /// @brief Dynamic offsets, in the pipeline state's storage.
    gp::UInt32* m_dynamicOffsets{ nullptr };

    /// @brief Hashable infos for the descriptor set cache; null off the cache path.
    VulkanHashableDescriptorInfo* m_hashableDescriptorInfos{ nullptr };

    /// @brief Key over `m_hashableDescriptorInfos`, regenerated lazily.
    mutable VulkanDescriptorSetKey m_key{};

#if GP_VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
    /// @brief Bindings written since `reset()`, one bit each.
    gp::Vector<gp::UInt32> m_writtenMask{};

    /// @brief Bindings written by default (samplers), copied into `m_writtenMask` on `reset()`.
    gp::Vector<gp::UInt32> m_baseWrittenMask{};

    /// @brief Bindings ignored by `checkAllWritten()`.
    gp::Vector<gp::UInt32> m_excludedMask{};
#endif

    /// @brief Number of writes.
    gp::UInt32 m_numWrites{ 0u };

    /// @brief Whether `m_key` is stale.
    mutable bool m_isKeyDirty{ true };

    /// @brief Whether a written resource has no stable id.
    bool m_hasVolatileResources{ false };
};

}   // namespace gp::rhi::vulkan
