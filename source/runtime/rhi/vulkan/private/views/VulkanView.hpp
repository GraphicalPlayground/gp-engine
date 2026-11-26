// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/PoolMoveRequest.hpp"
#include "CoreMinimal.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ViewEnums.hpp"
#include "views/VulkanViewEnums.hpp"
#include "views/VulkanViewForward.hpp"   // IWYU pragma: keep
#include "views/VulkanViewTypes.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native state of one RHI view: an image view, a texel buffer view, a storage buffer range or an
///        acceleration structure.
/// @details The four shapes never coexist, so the state is a tagged union selected by `VulkanViewKind`. Every
///          `initAs*()` call fills one arm; `invalidate()` hands the native object to the deferred deletion queue and
///          returns to `Null` while remembering that the view was initialized, so a rebuild after a resource rename
///          knows the old descriptor may still be read by in-flight work.
///
///          Each view also reserves a slot in the bindless descriptor heap for its descriptor type. Until the
///          bindless descriptor manager exists the slot is never allocated and `getBindlessHandle()` stays invalid;
///          descriptor sets are written from the native state instead.
/// @note Not thread-safe except `getOrCreateTypedBufferView()`: views are built and rebuilt on the RHI timeline.
class GP_RHI_VULKAN_API VulkanView
{
public:
    /// @brief Constructs an uninitialized view.
    /// @param[in] services Device and deletion services; copied into the view.
    /// @param[in] descriptorType Bindless descriptor type of the view, or `Invalid` for views that never need one
    ///                           (framebuffer attachments).
    VulkanView(const VulkanResourceServices& services, view::DescriptorType descriptorType) noexcept;

    /// @brief Releases the native object through the deferred deletion queue.
    ~VulkanView() noexcept;

    VulkanView(const VulkanView&) = delete;
    VulkanView& operator=(const VulkanView&) = delete;
    VulkanView(VulkanView&&) = delete;
    VulkanView& operator=(VulkanView&&) = delete;

    /// @brief Releases the native object and returns to the `Null` state.
    /// @details The initialized flag is carried over, so `isInitialized()` stays true after a rebuild starts.
    void invalidate() noexcept;

    /// @brief Returns the kind of native object currently held.
    /// @return Active storage arm.
    [[nodiscard]] VulkanViewKind getViewKind() const noexcept
    {
        return m_kind;
    }

    /// @brief Reports whether the view holds, or has held, a native object.
    /// @return True after the first `initAs*()` call, even while invalidated.
    [[nodiscard]] bool isInitialized() const noexcept
    {
        return (m_kind != VulkanViewKind::Null) || m_storage.invalidated.wasInitialized;
    }

    /// @brief Returns the texture view state.
    /// @note Asserts that the kind is `Texture`.
    /// @return Image view state.
    [[nodiscard]] const VulkanTextureView& getTextureView() const noexcept;

    /// @brief Returns the structured buffer view state.
    /// @note Asserts that the kind is `StructuredBuffer`.
    /// @return Buffer range state.
    [[nodiscard]] const VulkanStructuredBufferView& getStructuredBufferView() const noexcept;

    /// @brief Returns the acceleration structure view state.
    /// @note Asserts that the kind is `AccelerationStructure`.
    /// @return Acceleration structure state.
    [[nodiscard]] const VulkanAccelerationStructureView& getAccelerationStructureView() const noexcept;

    /// @brief Returns the typed buffer view state, creating the `VkBufferView` on first use when needed.
    /// @details Bindless descriptors are built from the buffer address and need no `VkBufferView`, so bindless
    ///          views only create it when a descriptor-set pipeline binds them.
    /// @note Asserts that the kind is `TypedBuffer`. Thread-safe: parallel translation contexts may bind the same
    ///       view concurrently.
    /// @return Texel buffer view state with a valid `view` handle (unless creation failed).
    [[nodiscard]] const VulkanTypedBufferView& getOrCreateTypedBufferView() noexcept;

    /// @brief Builds a texture view outside of any pipeline context (texture-owned internal views).
    /// @param[in] image Image to view.
    /// @param[in] viewDesc Backend view description.
    void createTextureView(VkImage image, const VulkanTextureViewDesc& viewDesc) noexcept;

    /// @brief Rebuilds a texture view after its image changed.
    /// @param[in] contexts Pipeline contexts active during the rename.
    /// @param[in] image New image.
    /// @param[in] viewDesc Backend view description.
    void updateTextureView(
        const allocator::ContextArray& contexts, VkImage image, const VulkanTextureViewDesc& viewDesc
    ) noexcept;

    /// @brief Builds a texel buffer view.
    /// @note The view must be `Null`.
    /// @param[in] contexts Pipeline contexts that may already use the descriptor.
    /// @param[in] buffer Target buffer.
    /// @param[in] format Texel format.
    /// @param[in] offset Byte offset inside the buffer; the allocation offset is added internally.
    /// @param[in] size Byte size of the view.
    void initAsTypedBufferView(
        const allocator::ContextArray& contexts,
        const VulkanBuffer& buffer,
        PixelFormat format,
        gp::UInt32 offset,
        gp::UInt32 size
    ) noexcept;

    /// @brief Builds a structured (storage buffer) view.
    /// @note The view must be `Null`.
    /// @param[in] contexts Pipeline contexts that may already use the descriptor.
    /// @param[in] buffer Target buffer.
    /// @param[in] offset Byte offset inside the buffer; the allocation offset is added internally.
    /// @param[in] size Byte size of the view.
    void initAsStructuredBufferView(
        const allocator::ContextArray& contexts, const VulkanBuffer& buffer, gp::UInt32 offset, gp::UInt32 size
    ) noexcept;

    /// @brief Builds a top-level acceleration structure over a buffer range.
    /// @note The view must be `Null` and `VK_KHR_acceleration_structure` must be loaded.
    /// @param[in] contexts Pipeline contexts that may already use the descriptor.
    /// @param[in] buffer Buffer holding the acceleration structure data.
    /// @param[in] offset Byte offset inside the buffer; the allocation offset is added internally.
    /// @param[in] size Byte size of the acceleration structure.
    void initAsAccelerationStructureView(
        const allocator::ContextArray& contexts, const VulkanBuffer& buffer, gp::UInt32 offset, gp::UInt32 size
    ) noexcept;

    /// @brief Returns the bindless descriptor slot of the view.
    /// @return Slot handle; invalid while bindless descriptors are not allocated.
    [[nodiscard]] view::DescriptorHandle getBindlessHandle() const noexcept
    {
        return m_bindlessHandle;
    }

    /// @brief Returns the bindless descriptor type the view is written as.
    /// @return Descriptor type, or `Invalid` for views without a bindless slot.
    [[nodiscard]] view::DescriptorType getDescriptorType() const noexcept
    {
        return m_descriptorType;
    }

protected:
    /// @brief Builds an image view.
    /// @note The view must be `Null`.
    /// @param[in] contexts Pipeline contexts that may already use the descriptor.
    /// @param[in] image Image to view.
    /// @param[in] viewDesc Backend view description.
    void initAsTextureView(
        const allocator::ContextArray& contexts, VkImage image, const VulkanTextureViewDesc& viewDesc
    ) noexcept;

private:
    /// @brief Tagged storage; `m_kind` selects the live arm. Every arm is trivially copyable.
    union Storage
    {
        /// @brief Starts in the `Null` arm.
        Storage() noexcept
            : invalidated()
        {}

        VulkanInvalidatedViewState invalidated;
        VulkanTypedBufferView typedBuffer;
        VulkanTextureView texture;
        VulkanStructuredBufferView structuredBuffer;
        VulkanAccelerationStructureView accelerationStructure;
    };

    /// @brief Creates the `VkBufferView` described by `typedBufferView`.
    /// @param[in,out] typedBufferView State whose `view` handle receives the new view.
    void createTypedBufferViewHandle(VulkanTypedBufferView& typedBufferView) const noexcept;

    VulkanResourceServices m_services{};
    Storage m_storage{};
    view::DescriptorHandle m_bindlessHandle{};
    view::DescriptorType m_descriptorType{ view::DescriptorType::Invalid };
    VulkanViewKind m_kind{ VulkanViewKind::Null };
};

}   // namespace gp::rhi::vulkan
