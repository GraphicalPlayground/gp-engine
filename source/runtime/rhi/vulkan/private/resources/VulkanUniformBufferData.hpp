// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"            // IWYU pragma: keep
#include "resources/UniformBufferLayoutDesc.hpp"
#include "RHIVulkan.hpp"
#include "shaders/ShaderEnums.hpp"
#include "views/DescriptorHandle.hpp"

namespace gp::rhi::vulkan
{

/// @brief Reports whether a uniform buffer member of `memberType` carries an RHI resource pointer that the uniform
///        buffer keeps alive in its resource table.
/// @details Textures, shader resource views, unordered access views, samplers and uniform blocks hold a pointer.
///          Constants, structs, render target slots, resource collections and descriptor ranges are ignored.
/// @param[in] memberType Member type from the layout.
/// @return True when the member stores a retained resource pointer.
[[nodiscard]] GP_RHI_VULKAN_API bool
    isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType memberType) noexcept;

/// @brief Reads the RHI resource pointer that `contents` stores for `member`.
/// @param[in] contents Shader parameter struct contents; may be null.
/// @param[in] member Layout entry describing the member offset and type.
/// @return The resource, or `nullptr` when `contents` is null, the member is ignored, or the slot is empty.
[[nodiscard]] GP_RHI_VULKAN_API resource::Resource*
    getVulkanUniformBufferResource(const void* contents, const resource::UniformBufferResource& member) noexcept;

/// @brief Reads the bindless descriptor handle of the resource that `contents` stores for `member`.
/// @param[in] contents Shader parameter struct contents; must not be null.
/// @param[in] member Layout entry describing the member offset and type.
/// @return The bindless handle, or an invalid handle when the slot is empty or the member has none.
[[nodiscard]] GP_RHI_VULKAN_API view::DescriptorHandle
    getVulkanUniformBufferBindlessHandle(const void* contents, const resource::UniformBufferResource& member) noexcept;

/// @brief Copies the constant block of `source` to `destination`, then patches bindless indices over resource slots.
/// @param[out] destination Mapped constant buffer memory of at least `layout.getConstantBufferSize()` bytes.
/// @param[in] source Shader parameter struct contents.
/// @param[in] layout Uniform buffer layout.
/// @param[in] allowBindless True to overwrite each resource slot with the 32-bit index of its valid bindless handle.
GP_RHI_VULKAN_API void updateVulkanUniformBufferConstants(
    void* destination, const void* source, const resource::UniformBufferLayout& layout, bool allowBindless
) noexcept;

/// @brief Checks that every resource slot of `layout` is populated in `contents`.
/// @details Logs every empty slot so the faulty parameter struct can be found.
/// @param[in] layout Uniform buffer layout.
/// @param[in] contents Shader parameter struct contents; may be null when the layout has no resources.
/// @return True when every retained resource slot is non-null.
[[nodiscard]] GP_RHI_VULKAN_API bool
    validateVulkanUniformBufferResources(const resource::UniformBufferLayout& layout, const void* contents) noexcept;

}   // namespace gp::rhi::vulkan
