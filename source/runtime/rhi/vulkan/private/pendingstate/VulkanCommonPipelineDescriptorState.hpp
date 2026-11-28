// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetKey.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanDescriptorSetWriteContainer.hpp"
#include "descriptors/VulkanDescriptorSetWriter.hpp"
#include "pendingstate/VulkanPackedUniformBufferState.hpp"
#include "pendingstate/VulkanPendingStateForward.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "RHIVulkan.hpp"
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "states/VulkanStateForward.hpp"     // IWYU pragma: keep
#include "views/VulkanViewForward.hpp"       // IWYU pragma: keep
#include "VulkanCommon.hpp"
#include "VulkanForward.hpp"                 // IWYU pragma: keep
#include "VulkanThirdParty.hpp"              // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Descriptor bindings of one pipeline, kept between draws and flushed into descriptor sets on demand.
/// @details The state owns one write list per descriptor set (set index == shader stage index), laid out once at
///          construction in a single container so a flush walks contiguous arrays:
///
///          @code
///          descriptorWrites      [ set 0 writes | set 1 writes | ... ]   one per binding
///          descriptorImageInfo   [ set 0 images | set 1 images | ... ]   pointed at by the writes
///          descriptorBufferInfo  [ set 0 bufs   | set 1 bufs   | ... ]
///          hashableDescriptorInfo[ set 0 ids + layout | set 1 ids + layout | ... ]   cache path only
///          dynamicOffsets        [ set 0 offsets | set 1 offsets | ... ]
///          @endcode
///
///          Setters only patch these arrays and raise a dirty flag when a value actually changed. The flush then
///          picks one of two paths:
///          - **Descriptor set cache:** the ids of every binding form a key; equal keys reuse sets written in an
///            earlier frame, so a static material binds without any `vkUpdateDescriptorSets`.
///          - **Per-frame pools:** fresh sets are allocated from the frame's pools and rewritten whenever a binding
///            changed. Used for transient views (they have no stable id) or when the cache is disabled.
///
///          Dynamic uniform buffers change through their offset only, which goes to `vkCmdBindDescriptorSets` and
///          never dirties the sets.
/// @note Not thread-safe; owned by a pending state on the recording thread. Not movable: the writers hold raw
///       pointers into the container.
class GP_RHI_VULKAN_API VulkanCommonPipelineDescriptorState
{
public:
    VulkanCommonPipelineDescriptorState(const VulkanCommonPipelineDescriptorState&) = delete;
    VulkanCommonPipelineDescriptorState& operator=(const VulkanCommonPipelineDescriptorState&) = delete;
    VulkanCommonPipelineDescriptorState(VulkanCommonPipelineDescriptorState&&) = delete;
    VulkanCommonPipelineDescriptorState& operator=(VulkanCommonPipelineDescriptorState&&) = delete;

    /// @brief Binds a uniform buffer; picks the static or dynamic descriptor from the layout.
    /// @details A dynamic binding stores `offset` as its dynamic offset and only dirties the sets when the buffer or
    ///          the range changed.
    /// @param[in] stage Shader stage (descriptor set index).
    /// @param[in] bindingIndex Binding in the stage's set.
    /// @param[in] buffer Native buffer.
    /// @param[in] handleId Unique id of `buffer`; must be non-zero on the cache path.
    /// @param[in] offset Byte offset of the uniform data.
    /// @param[in] range Size of the uniform data.
    /// @return True when the descriptor sets must be rewritten.
    bool setUniformBuffer(
        shader_stage::Stage stage,
        gp::UInt32 bindingIndex,
        VkBuffer buffer,
        gp::UInt32 handleId,
        gp::UInt32 offset,
        gp::UInt32 range
    ) noexcept;

    /// @brief Binds a sampler.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set; a sampler or combined image sampler.
    /// @param[in] sampler Sampler state.
    /// @return True when the descriptor sets must be rewritten.
    bool setSampler(shader_stage::Stage stage, gp::UInt32 bindingIndex, const VulkanSamplerState& sampler) noexcept;

    /// @brief Binds a texture view; picks the descriptor kind (sampled, storage, input attachment) from the layout.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set.
    /// @param[in] textureView Image view and its id.
    /// @param[in] layout Layout the image is in when the GPU reads it.
    /// @return True when the descriptor sets must be rewritten.
    bool setTextureView(
        shader_stage::Stage stage, gp::UInt32 bindingIndex, const VulkanTextureView& textureView, VkImageLayout layout
    ) noexcept;

    /// @brief Binds a read-only view: a texture, a typed buffer, a structured buffer or an acceleration structure.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set.
    /// @param[in,out] view View; typed buffer views are created on first use.
    /// @param[in] imageLayout Layout of the image for texture views.
    /// @return True when the descriptor sets must be rewritten.
    bool setShaderResourceView(
        shader_stage::Stage stage, gp::UInt32 bindingIndex, VulkanView& view, VkImageLayout imageLayout
    ) noexcept;

    /// @brief Binds a read-write view: a storage image, a storage texel buffer or a storage buffer.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set.
    /// @param[in,out] view View; typed buffer views are created on first use.
    /// @return True when the descriptor sets must be rewritten.
    bool setUnorderedAccessView(shader_stage::Stage stage, gp::UInt32 bindingIndex, VulkanView& view) noexcept;

    /// @brief Binds a ray tracing acceleration structure.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set.
    /// @param[in] accelerationStructure Native acceleration structure.
    /// @return True when the descriptor sets must be rewritten.
    bool setAccelerationStructure(
        shader_stage::Stage stage, gp::UInt32 bindingIndex, VkAccelerationStructureKHR accelerationStructure
    ) noexcept;

    /// @brief Writes a loose parameter into the stage's packed globals buffer.
    /// @param[in] stage Shader stage.
    /// @param[in] byteOffset Offset of the parameter in the packed buffer.
    /// @param[in] numBytes Size of the parameter.
    /// @param[in] data Parameter bytes.
    void setPackedGlobalParameter(
        shader_stage::Stage stage, gp::UInt32 byteOffset, gp::UInt32 numBytes, const void* data
    ) noexcept;

    /// @brief Forces the next flush to rewrite the sets and re-upload every packed globals buffer.
    /// @details Called when the pipeline is bound again after another one: its sets and its transient packed
    ///          uniform memory may have been recycled since.
    void reset() noexcept;

    /// @brief Forces the next flush to rewrite the sets.
    void markDirty() noexcept
    {
        m_isResourcesDirty = true;
    }

    /// @brief Uploads dirty packed globals and brings the descriptor sets up to date.
    /// @param[in,out] context Source of transient memory, descriptor pools and the set cache.
    /// @return True when the pipeline has descriptor sets ready to bind; false when it has none, or on failure.
    [[nodiscard]] bool updateDescriptorSets(IVulkanPendingStateContext& context) noexcept;

    /// @brief Records `vkCmdBindDescriptorSets` for every set with a handle, with the current dynamic offsets.
    /// @details Consecutive sets are bound by one call; a set without a handle (an empty set from the cache path)
    ///          splits the range, since a null set cannot be bound.
    /// @param[in] commandBuffer Command buffer in the recording state.
    void bindDescriptorSets(VkCommandBuffer commandBuffer) const noexcept;

    /// @brief Returns the number of descriptor sets of the pipeline layout.
    /// @return Set count.
    [[nodiscard]] gp::UInt32 getNumSets() const noexcept
    {
        return m_numSets;
    }

    /// @brief Returns the mask of sets that hold at least one binding.
    /// @return Bit `i` set when set `i` is used.
    [[nodiscard]] gp::UInt32 getUsedSetsMask() const noexcept
    {
        return m_usedSetsMask;
    }

    /// @brief Returns the current descriptor set handles, one per set.
    /// @return Handles; null until the first successful flush.
    [[nodiscard]] const gp::Vector<VkDescriptorSet>& getDescriptorSetHandles() const noexcept
    {
        return m_descriptorSetHandles;
    }

    /// @brief Returns the dynamic offsets passed to `vkCmdBindDescriptorSets`.
    /// @return Offsets in set order.
    [[nodiscard]] const gp::Vector<gp::UInt32>& getDynamicOffsets() const noexcept
    {
        return m_dynamicOffsets;
    }

    /// @brief Returns the writer of one set.
    /// @param[in] setIndex Set index; must be below `getNumSets()`.
    /// @return Writer.
    [[nodiscard]] const VulkanDescriptorSetWriter& getWriter(gp::UInt32 setIndex) const noexcept;

    /// @brief Returns the packed globals buffer of one stage.
    /// @param[in] stage Shader stage.
    /// @return Packed uniform buffer state; empty when the stage has no packed globals.
    [[nodiscard]] const VulkanPackedUniformBufferState&
        getPackedUniformBuffer(shader_stage::Stage stage) const noexcept;

    /// @brief Returns the write arrays shared by every writer.
    /// @return Write container.
    [[nodiscard]] const VulkanDescriptorSetWriteContainer& getWriteContainer() const noexcept
    {
        return m_writeContainer;
    }

    /// @brief Reports whether a binding changed since the sets were last written.
    /// @return True when the next flush rewrites the sets.
    [[nodiscard]] bool isResourcesDirty() const noexcept
    {
        return m_isResourcesDirty;
    }

    /// @brief Reports whether the state was built for the descriptor set cache path.
    /// @return True when hashable infos are tracked.
    [[nodiscard]] bool usesDescriptorCache() const noexcept
    {
        return m_useDescriptorCache;
    }

    /// @brief Reports whether a binding refers to a resource without a stable id.
    /// @return True when the sets cannot be cached.
    [[nodiscard]] bool hasVolatileResources() const noexcept;

    /// @brief Returns the set layouts the state writes.
    /// @return Descriptor sets layout.
    [[nodiscard]] const VulkanDescriptorSetsLayout& getDescriptorSetsLayout() const noexcept
    {
        return m_layout;
    }

    /// @brief Returns the pipeline bind point of the layout.
    /// @return Graphics or compute bind point.
    [[nodiscard]] VkPipelineBindPoint getBindPoint() const noexcept
    {
        return m_bindPoint;
    }

protected:
    /// @brief Lays out the write arrays of every set and binds the global samplers.
    /// @param[in] device Owning device; must outlive the state.
    /// @param[in] layout Set layouts finalized for the pipeline's shaders; must outlive the state. It does not need
    ///                   to be compiled: only the native handles are missing then.
    /// @param[in] pipelineLayout Native pipeline layout, or `VK_NULL_HANDLE` to never record binds.
    /// @param[in] codeHeaders Code header per stage; null entries for unused stages. Must outlive the state.
    /// @param[in] defaults Default and global samplers and views.
    /// @param[in] useDescriptorCache True to track hashable infos for the descriptor set cache.
    VulkanCommonPipelineDescriptorState(
        VulkanDevice& device,
        const VulkanDescriptorSetsLayout& layout,
        VkPipelineLayout pipelineLayout,
        const VulkanUniformBufferGatherInfo& codeHeaders,
        const VulkanDescriptorDefaults& defaults,
        bool useDescriptorCache
    ) noexcept;

    /// @brief Destroys the state; derived states are always destroyed through their own type.
    ~VulkanCommonPipelineDescriptorState() noexcept = default;

    /// @brief Binds an input attachment.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set; an input attachment.
    /// @param[in] textureView Attachment view.
    /// @param[in] layout Layout of the attachment during the subpass.
    /// @return True when the descriptor sets must be rewritten.
    bool setInputAttachment(
        shader_stage::Stage stage, gp::UInt32 bindingIndex, const VulkanTextureView& textureView, VkImageLayout layout
    ) noexcept;

    /// @brief Returns the code header of one stage.
    /// @param[in] stage Shader stage.
    /// @return Header, or null when the stage is unused.
    [[nodiscard]] const VulkanShaderHeader* getCodeHeader(const shader_stage::Stage stage) const noexcept
    {
        return m_codeHeaders.codeHeaders[static_cast<gp::USize>(stage)];
    }

private:
    /// @brief Sizes the container and sets every writer up over its slice.
    void createDescriptorWriteInfos() noexcept;

    /// @brief Binds the engine-wide samplers the shaders reference by type.
    void writeGlobalSamplers() noexcept;

    /// @brief Copies dirty packed globals to transient memory and points binding 0 of their set at it.
    /// @param[in,out] context Source of transient memory.
    /// @return False when the transient allocator is exhausted.
    [[nodiscard]] bool uploadPackedUniformBuffers(IVulkanPendingStateContext& context) noexcept;

    /// @brief Returns the writer of a stage, checking the stage has a set.
    /// @param[in] stage Shader stage.
    /// @return Writer.
    [[nodiscard]] VulkanDescriptorSetWriter& getStageWriter(shader_stage::Stage stage) noexcept;

    /// @brief Returns the descriptor type of a binding, checking it exists.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set.
    /// @return Descriptor type.
    [[nodiscard]] VkDescriptorType getBindingType(shader_stage::Stage stage, gp::UInt32 bindingIndex) const noexcept;

    /// @brief Writes a typed buffer view as a uniform or storage texel buffer, as the layout declares.
    /// @param[in] stage Shader stage.
    /// @param[in] bindingIndex Binding in the stage's set.
    /// @param[in] view Typed buffer view.
    /// @return True when the descriptor sets must be rewritten.
    bool writeTypedBufferView(
        shader_stage::Stage stage, gp::UInt32 bindingIndex, const VulkanTypedBufferView& view
    ) noexcept;

    /// @brief Folds a setter's result into the dirty flag.
    /// @param[in] hasChanged Setter result.
    /// @return `hasChanged`.
    bool accumulateDirty(bool hasChanged) noexcept;

private:
    /// @brief Owning device.
    VulkanDevice& m_device;

    /// @brief Set layouts written by this state.
    const VulkanDescriptorSetsLayout& m_layout;

    /// @brief Native pipeline layout bound with the sets.
    VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };

    /// @brief Bind point of the layout.
    VkPipelineBindPoint m_bindPoint{ VK_PIPELINE_BIND_POINT_GRAPHICS };

    /// @brief Code header per stage.
    VulkanUniformBufferGatherInfo m_codeHeaders{};

    /// @brief Default and global samplers and views.
    VulkanDescriptorDefaults m_defaults{};

    /// @brief Write arrays of every set.
    VulkanDescriptorSetWriteContainer m_writeContainer{};

    /// @brief One writer per set.
    gp::Vector<VulkanDescriptorSetWriter> m_writers{};

    /// @brief Current set handles, one per set.
    gp::Vector<VkDescriptorSet> m_descriptorSetHandles{};

    /// @brief Dynamic offsets of every set, in set order.
    gp::Vector<gp::UInt32> m_dynamicOffsets{};

    /// @brief Index of each set's first dynamic offset in `m_dynamicOffsets`.
    gp::Array<gp::UInt32, kVulkanMaxNumDescriptorStages> m_dynamicOffsetsStart{};

    /// @brief Number of dynamic offsets of each set.
    gp::Array<gp::UInt32, kVulkanMaxNumDescriptorStages> m_numDynamicOffsets{};

    /// @brief Packed globals buffer of each stage.
    gp::Array<VulkanPackedUniformBufferState, kVulkanMaxNumDescriptorStages> m_packedUniformBuffers{};

    /// @brief Key of the whole binding, rebuilt when a binding changed (cache path).
    VulkanDescriptorSetKey m_setsKey{};

    /// @brief Number of descriptor sets.
    gp::UInt32 m_numSets{ 0u };

    /// @brief Bit `i` set when set `i` holds bindings.
    gp::UInt32 m_usedSetsMask{ 0u };

    /// @brief True when the sets must be rewritten.
    bool m_isResourcesDirty{ true };

    /// @brief True when hashable infos are tracked.
    bool m_useDescriptorCache{ false };
};

}   // namespace gp::rhi::vulkan
