// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi::vulkan
{

/// @brief CPU mapping state of a `VulkanBuffer`.
enum class VulkanBufferLockStatus : gp::UInt8;

/// @brief Memory accounting bucket a buffer's bytes are charged to.
enum class VulkanBufferStatCategory : gp::UInt8;

/// @brief Current GPU allocation backing a `VulkanBuffer` (handle, host pointer, device address).
struct VulkanBufferAlloc;

/// @brief Bookkeeping for a buffer lock that goes through a staging buffer.
struct VulkanPendingBufferLock;

/// @brief One memory block bound into a sparse (reserved) buffer.
struct VulkanSparseBufferAllocation;

/// @brief Sparse binding state of a reserved `VulkanBuffer`.
struct VulkanReservedResourceData;

/// @brief Process-wide registry of staging-backed buffer locks awaiting their unlock.
class VulkanPendingBufferLockRegistry;

/// @brief Non-owning bundle of the device-level services a Vulkan resource needs.
struct VulkanResourceServices;

/// @brief Records transfer and barrier commands for resource uploads and readbacks.
class IVulkanTransferCommandRecorder;

/// @brief `IVulkanTransferCommandRecorder` that records into a `VulkanCommandBuffer`.
class VulkanCommandBufferTransferRecorder;

/// @brief Resolves the command contexts a resource operation records into.
class IVulkanCommandContextProvider;

/// @brief Shader resource or unordered access view that must be rebuilt when its resource is renamed.
class VulkanLinkedView;

/// @brief Observer notified when the backing allocation of a viewable resource changes.
class IVulkanResourceRenameListener;

/// @brief Resource that keeps a list of linked views and rename listeners.
class VulkanViewableResource;

/// @brief Readback buffer handed out by `createStagingBuffer()`.
class VulkanReadbackStagingBuffer;

/// @brief Who owns the `VkImage` of a `VulkanTexture`.
enum class VulkanImageOwnerType : gp::UInt8;

/// @brief Memory accounting bucket a texture's bytes are charged to.
enum class VulkanTextureStatCategory : gp::UInt8;

/// @brief Image creation parameters with the chained view format list.
struct VulkanImageCreateInfo;

/// @brief Host-visible buffer replacing the image of a `CPUReadback` texture.
struct VulkanCpuReadbackBuffer;

/// @brief Bookkeeping for a staging-backed texture lock.
struct VulkanPendingTextureLock;

/// @brief Process-wide registry of staging-backed texture locks awaiting their unlock.
class VulkanPendingTextureLockRegistry;

/// @brief Vulkan implementation of a GPU texture.
class VulkanTexture;

/// @brief Vulkan implementation of a texture reference.
class VulkanTextureReference;

}   // namespace gp::rhi::vulkan
