// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Queries the memory size and alignment the device needs for a texture.
/// @details The descriptor is turned into the same `VkImageCreateInfo` a `VulkanTexture` would use, starting at
///          `firstMipIndex`. With `VK_KHR_maintenance4` the requirements are queried without an image; otherwise a
///          temporary image is created and destroyed. Results are cached per descriptor, since streaming asks for the
///          same descriptors over and over.
/// @note Thread-safe. Backs `DynamicRHI::calcTexturePlatformSize()`.
/// @param[in] services Texture services (device and memory manager).
/// @param[in] desc Texture descriptor.
/// @param[in] firstMipIndex First resident mip.
/// @return Size and alignment, or zeros when the descriptor is invalid or the device rejects it.
[[nodiscard]] GP_RHI_VULKAN_API resource::CalcTextureSizeResult calcVulkanTexturePlatformSize(
    const VulkanResourceServices& services, const resource::TextureDesc& desc, gp::UInt32 firstMipIndex
) noexcept;

/// @brief Returns the device memory size of a texture.
/// @note Backs `DynamicRHI::computeMemorySize()`.
/// @param[in] texture Texture (a `VulkanTexture`); `nullptr` gives `0`.
/// @return Size reported by `vkGetImageMemoryRequirements()`, or the readback buffer size.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64 computeVulkanTextureMemorySize(resource::Texture* texture) noexcept;

/// @brief Fills the RHI texture memory statistics.
/// @details Device-local heaps give the dedicated video memory and the other heaps the dedicated system memory (on
///          unified memory devices everything is video memory). Live texture bytes are reported as non-streaming.
///          Shared system memory is unknown and reported as `-1`.
/// @note Backs `DynamicRHI::getTextureMemoryStats()`.
/// @param[in] services Texture services (memory manager).
/// @param[out] outStats Receives the statistics.
GP_RHI_VULKAN_API void fillVulkanTextureMemoryStats(
    const VulkanResourceServices& services, resource::TextureMemoryStats& outStats
) noexcept;

}   // namespace gp::rhi::vulkan
