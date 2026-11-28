// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"       // IWYU pragma: keep
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "memory/VulkanAllocation.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "RHIVulkan.hpp"
#include "views/DescriptorHandle.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Vulkan uniform buffer: a host-visible constant block plus the retained resource table of its layout.
/// @details Single-draw and single-frame buffers sub-allocate from the temp block allocator, so creating one costs a
///          bump allocation and a copy. Multi-frame buffers own a persistent pooled range. Every update reallocates the
///          constant range instead of writing in place, so the GPU keeps reading the previous contents of commands
///          already recorded.
class GP_RHI_VULKAN_API VulkanUniformBuffer final : public resource::UniformBuffer
{
public:
    /// @brief Creates the uniform buffer, fills its resource table and uploads `contents`.
    /// @param[in] services Device services; the memory manager must be bound.
    /// @param[in] layout Non-null layout with a constant block or at least one resource.
    /// @param[in] contents Shader parameter struct contents, or `nullptr` to leave the buffer uninitialized.
    /// @param[in] usage Lifetime class of the buffer.
    /// @param[in] validation Whether to validate that every resource slot is populated.
    VulkanUniformBuffer(
        const VulkanResourceServices& services,
        const resource::UniformBufferLayout* layout,
        const void* contents,
        resource::UniformBufferUsage usage,
        resource::UniformBufferValidation validation
    ) noexcept;

    /// @brief Frees the previous constant range and allocates a new one filled with `initialData`.
    /// @param[in] initialData Shader parameter struct contents, or `nullptr` to only reallocate.
    void allocateMemory(const void* initialData) noexcept;

    /// @brief Re-reads every resource slot of `layout` from `contents` into the resource table.
    /// @param[in] layout Layout of this buffer.
    /// @param[in] contents Shader parameter struct contents.
    /// @param[in] numResources Number of resources; must match the table size.
    void updateResourceTable(
        const resource::UniformBufferLayout& layout, const void* contents, gp::USize numResources
    ) noexcept;

    /// @brief Replaces the leading entries of the resource table with `resources`.
    /// @param[in] resources Resources to retain; at most the table size.
    void updateResourceTable(gp::ConstVectorView<resource::Resource*> resources) noexcept;

    /// @brief Returns the native buffer that holds the constant range.
    /// @return Buffer handle, or `VK_NULL_HANDLE` when the layout has no constants.
    [[nodiscard]] VkBuffer getBufferHandle() const noexcept
    {
        return m_allocation.getBufferHandle();
    }

    /// @brief Returns the byte offset of the constant range inside `getBufferHandle()`.
    /// @return Offset in bytes.
    [[nodiscard]] gp::UInt32 getOffset() const noexcept
    {
        return m_allocation.offset;
    }

    /// @brief Returns the allocation of the constant range.
    /// @return Current allocation.
    [[nodiscard]] const VulkanAllocation& getAllocation() const noexcept
    {
        return m_allocation;
    }

    /// @brief Returns the mapped CPU pointer of the current constant range.
    /// @return Mapped pointer, or `nullptr` when the layout has no constants.
    [[nodiscard]] const void* getMappedConstants() const noexcept
    {
        return m_mappedConstants;
    }

    /// @brief Returns the lifetime class of this buffer.
    /// @return Uniform buffer usage.
    [[nodiscard]] resource::UniformBufferUsage getUsage() const noexcept
    {
        return m_usage;
    }

    /// @brief Reports whether the constant range comes from the temp block allocator.
    /// @return True for temp block ranges.
    [[nodiscard]] bool isTempBlockAllocation() const noexcept
    {
        return m_allocation.isTempBlockAllocator != 0u;
    }

    /// @brief Returns the device address of the constant range.
    /// @return Address including the range offset, or 0 when buffer device address is unavailable.
    [[nodiscard]] VkDeviceAddress getDeviceAddress() const noexcept;

    /// @copydoc resource::UniformBuffer::getBindlessHandle()
    [[nodiscard]] virtual view::DescriptorHandle getBindlessHandle() const noexcept override;

protected:
    /// @brief Frees the constant range.
    virtual ~VulkanUniformBuffer() noexcept override;

private:
    /// @brief Device services the buffer allocates from.
    VulkanResourceServices m_services{};

    /// @brief Lifetime class of the buffer.
    resource::UniformBufferUsage m_usage{ resource::UniformBufferUsage::SingleDraw };

    /// @brief Current constant range.
    VulkanAllocation m_allocation{};

    /// @brief Mapped CPU pointer of `m_allocation`.
    void* m_mappedConstants{ nullptr };

    /// @brief Bindless constant buffer descriptor; invalid until a bindless descriptor manager is available.
    view::DescriptorHandle m_bindlessHandle{};
};

/// @brief Creates a Vulkan uniform buffer.
/// @param[in] services Device services; the memory manager must be bound.
/// @param[in] contents Shader parameter struct contents, or `nullptr`.
/// @param[in] layout Non-null uniform buffer layout.
/// @param[in] usage Lifetime class of the buffer.
/// @param[in] validation Whether to validate that every resource slot is populated.
/// @return New uniform buffer, or null when `services` is invalid.
[[nodiscard]] GP_RHI_VULKAN_API resource::UniformBufferRef createVulkanUniformBuffer(
    const VulkanResourceServices& services,
    const void* contents,
    const resource::UniformBufferLayout* layout,
    resource::UniformBufferUsage usage,
    resource::UniformBufferValidation validation = resource::UniformBufferValidation::ValidateResources
) noexcept;

/// @brief Updates the contents of a Vulkan uniform buffer on the bottom of the pipe.
/// @details The resource slots are read now and retained by the command list until the update runs; the constants
///          are copied into the command list arena unless it is bypassed.
/// @param[in,out] cmdList Command list that orders the update.
/// @param[in] uniformBuffer Uniform buffer created by `createVulkanUniformBuffer()`.
/// @param[in] contents Shader parameter struct contents matching the buffer layout.
GP_RHI_VULKAN_API void updateVulkanUniformBuffer(
    command::CommandListBase& cmdList, resource::UniformBuffer* uniformBuffer, const void* contents
) noexcept;

}   // namespace gp::rhi::vulkan
