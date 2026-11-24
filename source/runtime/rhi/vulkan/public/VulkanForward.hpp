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

/// @brief Concrete `resource::StagingBuffer` backed by host-visible Vulkan buffer memory.
class VulkanStagingBuffer;

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
