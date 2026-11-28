// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/InlineAllocator.hpp"   // IWYU pragma: keep
#include "memory/VulkanStagingManager.hpp"
#include "resources/VulkanResourceForward.hpp"     // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include "VulkanThirdParty.hpp"                    // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Maximum number of formats a mutable image can declare in its `VkImageFormatListCreateInfo`.
inline constexpr gp::USize kVulkanMaxImageViewFormats = 8u;

/// @brief Everything `vkCreateImage()` needs for a texture, with the format list chained into it.
/// @details `VkImageCreateInfo::pNext` points at `formatListInfo`, which points into `formatsUsed`, so the struct
///          must not be copied or moved once `generateVulkanImageCreateInfo()` has filled it.
struct VulkanImageCreateInfo final
{
    /// @brief Image creation parameters.
    VkImageCreateInfo imageCreateInfo{};

    /// @brief View format list (`VK_KHR_image_format_list`); chained only when the image is mutable.
    VkImageFormatListCreateInfo formatListInfo{};

    /// @brief Every format views of the image may use.
    gp::InlineVector<VkFormat, kVulkanMaxImageViewFormats> formatsUsed{};

    VulkanImageCreateInfo() noexcept = default;
    VulkanImageCreateInfo(const VulkanImageCreateInfo&) = delete;
    VulkanImageCreateInfo& operator=(const VulkanImageCreateInfo&) = delete;
    VulkanImageCreateInfo(VulkanImageCreateInfo&&) = delete;
    VulkanImageCreateInfo& operator=(VulkanImageCreateInfo&&) = delete;
};

/// @brief Host-visible buffer that replaces the image of a `CPUReadback` texture.
/// @details Mips are packed tightly one after the other; `mipOffsets[mip]` is the byte offset of each one.
struct VulkanCpuReadbackBuffer final
{
    /// @brief Buffer receiving the copies.
    VkBuffer buffer{ VK_NULL_HANDLE };

    /// @brief Byte offset of each mip inside `buffer`.
    gp::UInt32 mipOffsets[kMaxTextureMipCount]{};

    /// @brief Byte size of each mip.
    gp::UInt32 mipSizes[kMaxTextureMipCount]{};
};

/// @brief Bookkeeping for a texture lock that writes through a staging buffer.
struct VulkanPendingTextureLock final
{
    /// @brief Staging buffer handed out by the lock.
    VulkanStagingBuffer* stagingBuffer{ nullptr };

    /// @brief Locked mip.
    gp::UInt32 mipIndex{ 0u };

    /// @brief Combined array layer (`face + arrayIndex * 6` for cubes).
    gp::UInt32 arrayLayer{ 0u };

    /// @brief Access the lock was taken with.
    resource::ResourceLockMode lockMode{ resource::ResourceLockMode::WriteOnly };
};

}   // namespace gp::rhi::vulkan
