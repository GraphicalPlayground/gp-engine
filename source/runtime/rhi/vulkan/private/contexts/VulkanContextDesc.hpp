// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "contexts/VulkanContextForward.hpp"             // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanPendingStateForward.hpp"    // IWYU pragma: keep
#include "queries/VulkanQueryForward.hpp"                // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"        // IWYU pragma: keep
#include "VulkanForward.hpp"                             // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Non-owning bundle of device-level services a command context records against.
/// @details The dynamic RHI owns every service; contexts only borrow them. Optional services may be `nullptr`, in
///          which case the matching feature degrades gracefully: no queries, no descriptor sets, no uniform memory,
///          no render passes. This keeps contexts constructible in unit tests with only a device and a queue.
struct VulkanContextServices
{
    /// @brief Logical device every command is recorded for. Required.
    VulkanDevice* device{ nullptr };

    /// @brief Queue the context's payloads target. Required.
    VulkanQueue* queue{ nullptr };

    /// @brief Destination of finalized payloads for `flushCommands()`; `nullptr` disables flushing.
    IVulkanPayloadSubmitter* submitter{ nullptr };

    /// @brief Query pool manager used for occlusion and timestamp queries.
    VulkanQueryPoolManager* queryPoolManager{ nullptr };

    /// @brief Cache of render passes and framebuffers used by `beginRenderPass()`.
    VulkanRenderPassManager* renderPassManager{ nullptr };

    /// @brief Descriptor pool manager feeding the per-context pool set acquirer.
    VulkanDescriptorPoolsManager* descriptorPoolsManager{ nullptr };

    /// @brief Default sampler and image view written into unbound descriptor slots.
    const VulkanDescriptorDefaults* descriptorDefaults{ nullptr };

    /// @brief Optional cache of descriptor sets keyed by their writes.
    VulkanDescriptorSetCache* descriptorSetCache{ nullptr };

    /// @brief Ring allocator backing packed globals and transient uniform buffers.
    VulkanTempBlockAllocator* tempBlockAllocator{ nullptr };
};

}   // namespace gp::rhi::vulkan
