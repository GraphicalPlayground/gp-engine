// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/strings/Name.hpp"
#include "core/GPUMask.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/GPUFence.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Vulkan implementation of `gp::rhi::sync::GPUFence` backed by `VulkanSyncPoint` and optional `VkEvent`.
/// @details When timeline semaphores are enabled, completion is tracked directly through `m_completedSyncPoint`.
///          When timeline semaphores are unavailable, a `VkEvent` is additionally created so `poll()` can observe
///          GPU completion as soon as the command buffer is submitted (`m_submittedSyncPoint`).
class GP_RHI_VULKAN_API VulkanGPUFence final : public gp::rhi::sync::GPUFence
{
public:
    /// @brief Constructs a Vulkan GPU fence bound to `device`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] name Diagnostic `gp::Name` attached to this fence.
    /// @param[in] useTimelineSemaphores True when timeline semaphores are active (skips creating a fallback `VkEvent`).
    VulkanGPUFence(VulkanDevice& device, gp::Name name, bool useTimelineSemaphores = true) noexcept;

    /// @copydoc gp::rhi::sync::GPUFence::~GPUFence()
    /// @note Destroys the optional `VkEvent` immediately until deferred deletion queue integration lands.
    virtual ~VulkanGPUFence() noexcept override;

    using gp::rhi::sync::GPUFence::poll;

    /// @copydoc gp::rhi::sync::GPUFence::clear()
    virtual void clear() noexcept override;

    /// @copydoc gp::rhi::sync::GPUFence::poll()
    [[nodiscard]] virtual bool poll() const noexcept override;

    /// @copydoc gp::rhi::sync::GPUFence::wait()
    virtual void
        wait(gp::rhi::command::CommandListImmediate& cmdList, gp::rhi::GPUMask gpuMask) const noexcept override;

    /// @brief Returns the optional fallback `VkEvent` handle (`VK_NULL_HANDLE` when timeline semaphores are active).
    /// @return `VkEvent` handle or `VK_NULL_HANDLE`.
    [[nodiscard]] VkEvent getEvent() const noexcept
    {
        return m_event;
    }

    /// @brief Attaches the submission and completion sync points issued when `writeGPUFence` is queued.
    /// @param[in] submittedSyncPoint Sync point signaled once the command buffer is submitted (may be null).
    /// @param[in] completedSyncPoint Sync point signaled once the GPU completes the fence write.
    void setSyncPoints(VulkanSyncPointRef submittedSyncPoint, VulkanSyncPointRef completedSyncPoint) noexcept;

    /// @brief Returns the attached submission sync point, if any.
    /// @return Const reference to `m_submittedSyncPoint`.
    [[nodiscard]] const VulkanSyncPointRef& getSubmittedSyncPoint() const noexcept
    {
        return m_submittedSyncPoint;
    }

    /// @brief Returns the attached completion sync point, if any.
    /// @return Const reference to `m_completedSyncPoint`.
    [[nodiscard]] const VulkanSyncPointRef& getCompletedSyncPoint() const noexcept
    {
        return m_completedSyncPoint;
    }

private:
    VulkanDevice& m_device;
    VkEvent m_event{ VK_NULL_HANDLE };
    VulkanSyncPointRef m_submittedSyncPoint{};
    VulkanSyncPointRef m_completedSyncPoint{};
};

}   // namespace gp::rhi::vulkan
