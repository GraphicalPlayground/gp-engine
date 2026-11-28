// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi::vulkan
{

/// @brief Recording phase of the payload a context currently appends to.
enum class VulkanContextPhase : gp::UInt8;

/// @brief Bitmask controlling how far `VulkanContextCommon::flushCommands()` blocks.
enum class VulkanFlushFlags : gp::UInt8;

/// @brief Render pass and framebuffer that parallel (secondary) command buffers inherit on begin.
struct VulkanParallelRenderPassInfo;

/// @brief Non-owning bundle of device-level services a command context records against.
struct VulkanContextServices;

/// @brief Hands finalized payloads to the GPU and waits on their sync points.
class IVulkanPayloadSubmitter;

/// @brief `IVulkanPayloadSubmitter` that submits straight to the target queues and polls interrupts.
class VulkanImmediatePayloadSubmitter;

/// @brief Payload, command buffer and sync point bookkeeping shared by every Vulkan command context.
class VulkanContextCommon;

/// @brief Graphics or async compute command context implementing `command::ICommandContext`.
class VulkanCommandListContext;

/// @brief Staging-upload context implementing `command::IUploadContext`.
class VulkanUploadContext;

/// @brief Free-list of reusable `VulkanUploadContext` instances.
class VulkanUploadContextPool;

/// @brief Resolves command lists to Vulkan contexts for resources, queries and fences.
class VulkanCommandContextProvider;

}   // namespace gp::rhi::vulkan
