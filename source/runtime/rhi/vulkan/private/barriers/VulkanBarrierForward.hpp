// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Image-based variable rate shading encoding reported by the device.
enum class VulkanShadingRateImageType : gp::UInt8;

/// @brief Concrete Vulkan object kind a `sync::TransitionInfo` resolves to before barrier generation.
enum class VulkanBarrierResourceType : gp::UInt8;

/// @brief Stage mask, access mask and image layout derived from a single `sync::ResourceAccess` value.
struct VulkanStageAccessLayout;

/// @brief Stage mask and access mask pair derived from a `sync::ResourceAccess` value or an image layout.
struct VulkanStageAccessFlags;

/// @brief Device-wide inputs used when translating `sync::ResourceAccess` values into Vulkan flags.
struct VulkanBarrierTranslationDesc;

/// @brief Queue family and capability snapshot consumed by transition processing.
struct VulkanBarrierDeviceDesc;

/// @brief Runtime toggles controlling barrier batching, collapsing and split barriers.
struct VulkanBarrierSettings;

/// @brief Vulkan-side view of a transitioned resource (image, buffer or acceleration structure).
struct VulkanBarrierResource;

/// @brief Reserved buffer commit request gathered from transitions carrying `sync::CommitResourceInfo`.
struct VulkanCommitReservedResourceDesc;

/// @brief Batch of `synchronization2` barrier arrays recorded by `vkCmdPipelineBarrier2KHR`.
struct VulkanSync2BarrierArrays;

/// @brief Batch of legacy barrier arrays recorded by `vkCmdPipelineBarrier` with merged stage masks.
struct VulkanLegacyBarrierArrays;

/// @brief Side data kept with each legacy image barrier for depth/stencil plane merging.
struct VulkanImageBarrierExtraData;

/// @brief References every collaborator needed to turn transitions into recorded barriers.
struct VulkanBarrierContext;

/// @brief Backend payload stored inline inside every `sync::Transition`.
struct VulkanTransitionData;

/// @brief Batched memory, buffer and image barriers recorded internally by the backend.
struct VulkanPipelineBarrier;

/// @brief Per-subresource `VkImageLayout` table for a single image.
struct VulkanImageLayout;

/// @brief Tracks the current `VkImageLayout` of every image touched by a command list context.
class VulkanLayoutManager;

/// @brief Thread-safe pool recycling device-only `VkEvent` handles used by split barriers.
class VulkanBarrierEventPool;

/// @brief Interface mapping RHI resources referenced by transitions to their Vulkan handles.
class IVulkanBarrierResourceResolver;

/// @brief Interface receiving the barrier, event and semaphore commands produced by transition processing.
class IVulkanBarrierCommandRecorder;

/// @brief `IVulkanBarrierCommandRecorder` implementation recording into a `VulkanCommandBuffer`.
class VulkanCommandBufferBarrierRecorder;

}   // namespace gp::rhi::vulkan
