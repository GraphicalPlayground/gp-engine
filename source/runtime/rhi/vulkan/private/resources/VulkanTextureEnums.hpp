// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Who owns the `VkImage` a `VulkanTexture` points at, which decides what the destructor releases.
/// @details
///          - `None`: no image (CPU readback textures live in a `VkBuffer`).
///          - `LocalOwner`: the texture created the image and its memory, and destroys both.
///          - `ExternalOwner`: the image belongs to someone else (a swapchain); only the views are destroyed.
///          - `Aliased`: the texture borrows the image, views and memory of another texture it keeps alive.
enum class VulkanImageOwnerType : gp::UInt8
{
    None = 0u,
    LocalOwner,
    ExternalOwner,
    Aliased
};

/// @brief Memory accounting bucket a texture's bytes are charged to.
enum class VulkanTextureStatCategory : gp::UInt8
{
    Texture2D = 0u,
    Texture3D,
    TextureCube,
    RenderTarget,
    CPUReadback,
    Count
};

/// @brief Number of `VulkanTextureStatCategory` buckets.
inline constexpr gp::USize kVulkanTextureStatCategoryCount = static_cast<gp::USize>(VulkanTextureStatCategory::Count);

}   // namespace gp::rhi::vulkan
