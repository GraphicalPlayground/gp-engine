// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Every legacy access bit that only reads memory.
/// @details A barrier whose source access is a subset of this mask has no writes to make available, so an execution
///          dependency is enough and the access masks can be dropped from the merged memory barrier.
inline constexpr VkAccessFlags kVulkanReadAccessMask =
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
    VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT |
    VK_ACCESS_MEMORY_READ_BIT;

/// @brief Stage mask and access mask pair derived from a `sync::ResourceAccess` value or an image layout.
struct VulkanStageAccessFlags
{
    /// @brief Pipeline stages that touch the resource.
    VkPipelineStageFlags stageFlags{ 0u };

    /// @brief Memory accesses performed by those stages.
    VkAccessFlags accessFlags{ 0u };

    /// @brief Compares two stage/access pairs.
    /// @param[in] rhs Pair to compare against.
    /// @return True when both masks match.
    [[nodiscard]] constexpr bool operator==(const VulkanStageAccessFlags& rhs) const noexcept = default;
};

/// @brief Stage mask, access mask and image layout derived from a single `sync::ResourceAccess` value.
struct VulkanStageAccessLayout
{
    /// @brief Pipeline stages that touch the resource.
    VkPipelineStageFlags stageFlags{ 0u };

    /// @brief Memory accesses performed by those stages.
    VkAccessFlags accessFlags{ 0u };

    /// @brief Image layout the resource must be in; `VK_IMAGE_LAYOUT_UNDEFINED` when the access implies none.
    VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };

    /// @brief Compares two translation results.
    /// @param[in] rhs Result to compare against.
    /// @return True when stage mask, access mask and layout all match.
    [[nodiscard]] constexpr bool operator==(const VulkanStageAccessLayout& rhs) const noexcept = default;
};

/// @brief Vulkan-side view of a transitioned resource (image, buffer or acceleration structure).
/// @details Filled by an `IVulkanBarrierResourceResolver` from the RHI resource referenced by a
///          `sync::TransitionInfo`. UAVs are unwrapped to the texture or buffer they view, so barrier generation only
///          ever deals with the concrete underlying object.
struct VulkanBarrierResource
{
    /// @brief Concrete object kind; `None` makes the converter skip the transition.
    VulkanBarrierResourceType type{ VulkanBarrierResourceType::None };

    /// @brief Image handle when `type == Texture`.
    VkImage image{ VK_NULL_HANDLE };

    /// @brief Every aspect of the image format (color, or depth and/or stencil).
    VkImageAspectFlags fullAspectMask{ 0u };

    /// @brief True when the image format has a depth or stencil aspect.
    bool isDepthOrStencilAspect{ false };

    /// @brief True when the image was created with `VK_IMAGE_USAGE_SAMPLED_BIT`; otherwise SRV reads use `GENERAL`.
    bool supportsSampling{ true };

    /// @brief True when the image can stay in `VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ` while bound as an attachment.
    bool supportsRenderingLocalRead{ false };

    /// @brief True for 3D textures, which address slices through the array layer range under `VK_KHR_maintenance9`.
    bool is3DTexture{ false };

    /// @brief True for reserved (sparse) textures.
    bool isReservedResource{ false };

    /// @brief Mip level count of the image (used by layout tracking).
    gp::UInt32 numMips{ 1u };

    /// @brief Array layer count of the image, counting cube faces (used by layout tracking).
    gp::UInt32 numArrayLayers{ 1u };

    /// @brief Per-plane tracked access (`[0]` depth, `[1]` stencil) owned by the texture, or `nullptr`.
    /// @note Only read and written by the legacy barrier path, which must transition both planes together.
    sync::ResourceAccess* allPlanesTrackedAccess{ nullptr };

    /// @brief Buffer handle when `type == Buffer`.
    VkBuffer buffer{ VK_NULL_HANDLE };

    /// @brief Byte offset of the buffer range covered by the barrier.
    VkDeviceSize bufferOffset{ 0u };

    /// @brief Byte size of the buffer range covered by the barrier.
    VkDeviceSize bufferSize{ 0u };

    /// @brief `VkBufferUsageFlags` the buffer was created with; refines vertex, index and uniform read access.
    VkBufferUsageFlags bufferUsageFlags{ 0u };
};

/// @brief Reserved buffer commit request gathered from transitions carrying `sync::CommitResourceInfo`.
struct VulkanCommitReservedResourceDesc
{
    /// @brief Reserved buffer whose physical backing changes.
    resource::Buffer* resource{ nullptr };

    /// @brief Requested committed size in bytes.
    gp::UInt64 commitSizeInBytes{ 0u };

    /// @brief Compares two commit requests.
    /// @param[in] rhs Request to compare against.
    /// @return True when resource and size match.
    [[nodiscard]] constexpr bool operator==(const VulkanCommitReservedResourceDesc& rhs) const noexcept = default;
};

}   // namespace gp::rhi::vulkan
