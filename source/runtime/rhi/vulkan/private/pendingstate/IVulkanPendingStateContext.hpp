// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanPendingStateForward.hpp"    // IWYU pragma: keep
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "views/VulkanViewForward.hpp"                   // IWYU pragma: keep
#include "VulkanThirdParty.hpp"                          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Services a pending state needs from the command context that owns it.
/// @details The pending state decides *what* must be recorded before a draw or a dispatch, but the memory it records
///          from belongs to the command context: descriptor pools of the current frame, the transient uniform ring
///          and the render targets of the active render pass. This seam keeps the pending state free of the context
///          type, so the flush logic can be built and tested before the context exists, with a fake behind it.
/// @note Every call happens on the thread recording the command buffer.
class IVulkanPendingStateContext
{
public:
    /// @brief Destroys the context.
    virtual ~IVulkanPendingStateContext() noexcept = default;

    /// @brief Allocates fresh descriptor sets for `layout` when they must be rewritten.
    /// @details The context acquires a pool set container lazily for the frame. Acquiring a new container, or a new
    ///          typed pool set inside it, forces an allocation even when `needDescriptors` is false: sets of an
    ///          earlier container may have been recycled.
    /// @param[in] layout Compiled set layouts of the pipeline.
    /// @param[in] needDescriptors True when the caller's resources changed since its sets were last written.
    /// @param[out] outSets Receives one set per set layout when the function returns true.
    /// @return True when new sets were allocated and must be written; false when the old sets stay valid, or when
    ///         allocation failed.
    [[nodiscard]] virtual bool acquirePoolSetAndDescriptorsIfNeeded(
        const VulkanDescriptorSetsLayout& layout, bool needDescriptors, VkDescriptorSet* outSets
    ) noexcept = 0;

    /// @brief Allocates transient, CPU-mapped memory for one packed uniform buffer.
    /// @param[in] size Number of bytes.
    /// @param[out] outUpload Receives the buffer, its id, the byte offset and the mapped pointer.
    /// @return False when the transient allocator is exhausted.
    [[nodiscard]] virtual bool allocateUniformMemory(gp::UInt32 size, VulkanUniformUpload& outUpload) noexcept = 0;

    /// @brief Returns the cross-frame descriptor set cache.
    /// @return Cache, or null when the cache path is disabled.
    [[nodiscard]] virtual VulkanDescriptorSetCache* getDescriptorSetCache() noexcept = 0;

    /// @brief Returns the frame number used to age cached descriptor sets.
    /// @return Current frame.
    [[nodiscard]] virtual gp::UInt64 getCurrentFrame() const noexcept = 0;

    /// @brief Returns the view of the active render pass attachment read as an input attachment.
    /// @param[in] type Attachment slot a shader reads.
    /// @param[out] outView Receives the attachment view.
    /// @param[out] outLayout Receives the layout the attachment is in during the subpass.
    /// @return False when no render pass is active or the slot holds no attachment.
    [[nodiscard]] virtual bool getInputAttachmentView(
        VulkanShaderAttachmentType type, VulkanTextureView& outView, VkImageLayout& outLayout
    ) const noexcept = 0;
};

}   // namespace gp::rhi::vulkan
