// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

struct VkAllocationCallbacks;

namespace gp::rhi::vulkan
{

/// @brief Dynamic Vulkan library loader and entry-point resolver.
class VulkanLoader;

/// @brief Owning wrapper managing `VkInstance` creation, extensions, layers, and debug callbacks.
class VulkanInstance;

/// @brief Creation parameters for `VulkanInstance::initialize()`.
struct VulkanInstanceCreateDesc;

/// @brief Inspected properties, memory heaps, UUID/LUID, and vendor metadata for a `VkPhysicalDevice`.
struct VulkanPhysicalDeviceInfo;

/// @brief Policy controlling physical device adapter selection.
struct VulkanPhysicalDeviceSelectionDesc;

/// @brief Context payload passed to Vulkan result verification error handlers.
struct VulkanResultErrorContext;

/// @brief Dynamic RHI module registering the Vulkan backend.
class VulkanDynamicRHIModule;

/// @brief Concrete `DynamicRHI` implementation backed by Vulkan.
class VulkanDynamicRHI;

/// @brief Logical Vulkan GPU device wrapper owning queues, memory managers, and extension state.
class VulkanDevice;

/// @brief Hardware command submission queue wrapper (`Graphics`, `AsyncCompute`, `Transfer`).
class VulkanQueue;

/// @brief Command buffer state machine tracking recording, render passes, and submission lifecycle.
class VulkanCommandBuffer;

/// @brief Pool allocating and recycling `VulkanCommandBuffer` objects for a queue family.
class VulkanCommandBufferPool;

/// @brief Batched queue submission payload carrying command buffers, semaphores, sync points, and completion state.
struct VulkanPayload;

/// @brief Concrete `command::IPlatformCommandList` wrapping a batch of `VulkanPayload` submissions.
class VulkanPlatformCommandList;

/// @brief Immediate and parallel command recording context targeting Vulkan command buffers.
class VulkanCommandListContext;

/// @brief Pooled `VkFence` wrapper tracking signaled state.
class VulkanFence;

/// @brief Manager pooling and recycling `VulkanFence` instances.
class VulkanFenceManager;

/// @brief Binary or timeline `VkSemaphore` synchronization primitive.
class VulkanSemaphore;

/// @brief Timeline synchronization point coordinating cross-queue and CPU waits.
class VulkanSyncPoint;

/// @brief Concrete `sync::GPUFence` backed by a Vulkan fence or timeline value.
class VulkanGPUFence;

/// @brief Low-level device heap allocation wrapping a single `VkDeviceMemory` handle.
class VulkanDeviceMemoryAllocation;

/// @brief Manages physical device memory properties, heap budgets, and raw `VkDeviceMemory` allocations.
class VulkanDeviceMemoryManager;

/// @brief Lightweight non-reference-counted handle describing a sub-allocated or dedicated Vulkan GPU allocation.
class VulkanAllocation;

/// @brief Single `MemoryPool` page backed by a `VulkanDeviceMemoryAllocation` (and optional whole-page `VkBuffer`).
class VulkanMemoryPool;

/// @brief Concrete `PoolAllocator` managing `VulkanMemoryPool` pages for a single configuration.
class VulkanPoolAllocator;

/// @brief Top-level Vulkan GPU memory manager routing buffer and image allocations across pools and dedicated heaps.
class VulkanMemoryManager;

/// @brief Frame-latency and GPU sync-point gated deferred deletion queue for Vulkan handles and memory allocations.
class VulkanDeferredDeletionQueue;

/// @brief Host-visible staging `VkBuffer` used for CPU-to-GPU uploads and GPU-to-CPU readbacks.
class VulkanStagingBuffer;

/// @brief Pool manager recycling `VulkanStagingBuffer` instances gated by GPU completion sync points.
class VulkanStagingManager;

/// @brief Ring of host-visible blocks serving volatile sub-allocations such as single-frame uniform buffers.
class VulkanTempBlockAllocator;

/// @brief Descriptor representing a Vulkan instance extension and its enablement state.
class VulkanInstanceExtension;

/// @brief Descriptor representing a Vulkan device extension and its enablement state.
class VulkanDeviceExtension;

/// @brief Aggregated optional instance extension availability flags.
struct OptionalVulkanInstanceExtensions;

/// @brief Aggregated optional device extension availability flags.
struct OptionalVulkanDeviceExtensions;

/// @brief Concrete `resource::Buffer` backed by a `VkBuffer`.
class VulkanBuffer;

/// @brief Concrete `resource::Texture` backed by a `VkImage`.
class VulkanTexture;

/// @brief Concrete `resource::UniformBuffer` backed by Vulkan descriptor or buffer memory.
class VulkanUniformBuffer;

/// @brief Base wrapper for compiled SPIR-V Vulkan shader modules.
class VulkanShader;

/// @brief Immutable render-target attachment layout used for Vulkan render pass compatibility.
class VulkanRenderTargetLayout;

/// @brief Hashable graphics pipeline creation descriptor.
struct VulkanGraphicsPipelineDesc;

/// @brief Platform swapchain wrapper managing presentable images and acquire/present synchronization.
class VulkanSwapChain;

/// @brief Concrete `presentation::Viewport` wrapping a `VulkanSwapChain`.
class VulkanViewport;

/// @brief Base native window context passed to surface and swapchain creation.
class VulkanGenericPlatformWindowContext;

/// @brief Default platform abstraction layer for Vulkan initialization, capabilities, and WSI hooks.
class VulkanGenericPlatform;

/// @brief Windows-specific Vulkan platform layer.
class VulkanWindowsPlatform;

/// @brief Linux-specific native window context carrying an immutable window identifier.
class VulkanLinuxPlatformWindowContext;

/// @brief Linux-specific Vulkan platform layer.
class VulkanLinuxPlatform;

/// @brief macOS-specific Vulkan platform layer targeting Metal WSI surfaces.
class VulkanMacPlatform;

/// @brief Android-specific native window context managing native window lifetime.
class VulkanAndroidPlatformWindowContext;

/// @brief Android-specific Vulkan platform layer.
class VulkanAndroidPlatform;

}   // namespace gp::rhi::vulkan
