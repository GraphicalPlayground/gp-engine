// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "sync/TransitionDesc.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Backend payload stored inline inside every `sync::Transition`.
/// @details Built by `createVulkanTransition()` when the transition is created and destroyed by
///          `releaseVulkanTransition()` right before the transition memory is freed. The payload keeps its own copy of
///          the transition infos because the caller's array does not outlive `createTransition()`.
struct GP_RHI_VULKAN_API VulkanTransitionData
{
    /// @brief Pipelines that execute the begin half of the transition.
    sync::PipelineStage srcPipelines{ sync::PipelineStage::None };

    /// @brief Pipelines that execute the end half of the transition.
    sync::PipelineStage dstPipelines{ sync::PipelineStage::None };

    /// @brief Binary semaphore signaled on the source queue and waited on by the destination queue.
    /// @note Only set for cross-queue transitions.
    VulkanSemaphoreRef semaphore{};

    /// @brief Device-only event used to split the barrier between begin and end on the same queue.
    /// @note Owned by the device event pool; the command buffer returns it once the GPU is done with it.
    VkEvent eventHandle{ VK_NULL_HANDLE };

    /// @brief Copy of the per-resource transitions.
    gp::Vector<sync::TransitionInfo> transitionInfos{};
};

/// @brief Returns the byte size of the payload appended to every `sync::Transition`.
/// @return `sizeof(VulkanTransitionData)`.
[[nodiscard]] GP_RHI_VULKAN_API gp::USize getVulkanTransitionPrivateDataSize() noexcept;

/// @brief Returns the alignment of the payload appended to every `sync::Transition`.
/// @return `alignof(VulkanTransitionData)`.
[[nodiscard]] GP_RHI_VULKAN_API gp::USize getVulkanTransitionPrivateDataAlign() noexcept;

/// @brief Decides whether a transition needs a semaphore between its source and destination queues.
/// @details Cross-queue transitions get a semaphore unless `TransitionCreateFlags::NoFence` is set. When queue family
///          ownership transfers are needed the semaphore is always created, because the release and acquire barriers
///          must be submitted in order.
/// @param[in] srcPipelines Source pipelines.
/// @param[in] dstPipelines Destination pipelines.
/// @param[in] flags Transition creation flags.
/// @param[in] needsQueueFamilyOwnershipTransfers True when the device needs ownership transfers.
/// @return True when a semaphore must be created.
[[nodiscard]] GP_RHI_VULKAN_API bool needsVulkanCrossQueueSemaphore(
    sync::PipelineStage srcPipelines,
    sync::PipelineStage dstPipelines,
    sync::TransitionCreateFlags flags,
    bool needsQueueFamilyOwnershipTransfers
) noexcept;

/// @brief Decides whether a transition may be split across `beginTransitions()` and `endTransitions()`.
/// @details Splitting needs the setting enabled, unlimited barrier batches, `synchronization2` support, the same
///          source and destination pipelines, and no `TransitionCreateFlags::NoSplit`.
/// @param[in] srcPipelines Source pipelines.
/// @param[in] dstPipelines Destination pipelines.
/// @param[in] flags Transition creation flags.
/// @param[in] settings Barrier settings.
/// @param[in] supportsParallelRendering True when the device supports `synchronization2`.
/// @return True when a split-barrier event may be used.
[[nodiscard]] GP_RHI_VULKAN_API bool canUseVulkanSplitBarrier(
    sync::PipelineStage srcPipelines,
    sync::PipelineStage dstPipelines,
    sync::TransitionCreateFlags flags,
    const VulkanBarrierSettings& settings,
    bool supportsParallelRendering
) noexcept;

/// @brief Reports whether any transition reads on the host (`ResourceAccess::CPURead`) before or after the barrier.
/// @details Events cannot carry host stage dependencies, so such transitions are never split.
/// @param[in] transitionInfos Transitions to scan.
/// @return True when any of them touches `CPURead`.
[[nodiscard]] GP_RHI_VULKAN_API bool
    vulkanTransitionIncludesHostSync(gp::ConstVectorView<sync::TransitionInfo> transitionInfos) noexcept;

/// @brief Constructs the `VulkanTransitionData` payload inside `transition`.
/// @details Copies the transition infos, creates the cross-queue semaphore when needed and takes a split-barrier
///          event from the device pool when the transition can be split.
/// @param[in,out] device Device owning semaphores and events.
/// @param[in,out] transition Newly allocated transition with zeroed private data.
/// @param[in] createInfo Source/destination pipelines, flags and transitions.
/// @param[in] settings Barrier settings.
GP_RHI_VULKAN_API void createVulkanTransition(
    VulkanDevice& device,
    sync::Transition* transition,
    const sync::TransitionCreateInfo& createInfo,
    const VulkanBarrierSettings& settings
) noexcept;

/// @brief Destroys the `VulkanTransitionData` payload inside `transition`.
/// @note The split-barrier event is not returned here; the command buffer that waited on it returns it.
/// @param[in,out] transition Retiring transition.
GP_RHI_VULKAN_API void releaseVulkanTransition(sync::Transition* transition) noexcept;

}   // namespace gp::rhi::vulkan
