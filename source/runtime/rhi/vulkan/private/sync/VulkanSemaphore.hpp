// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

class VulkanSemaphore;

/// @brief Reference-counted handle to a `VulkanSemaphore`.
using VulkanSemaphoreRef = gp::SharedPtr<VulkanSemaphore>;

/// @brief Internal Vulkan semaphore wrapper supporting binary, timeline (`VK_KHR_timeline_semaphore`), and externally
///        owned `VkSemaphore` handles.
class GP_RHI_VULKAN_API VulkanSemaphore final : public gp::SharedFromThis<VulkanSemaphore>
{
public:
    /// @brief Creates a new device-owned binary or timeline `VkSemaphore`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] flags Creation and ownership flags (`VulkanSemaphoreFlags::Timeline` for timeline semaphores).
    /// @param[in] initialTimelineValue Initial counter value when `VulkanSemaphoreFlags::Timeline` is set.
    VulkanSemaphore(
        VulkanDevice& device,
        VulkanSemaphoreFlags flags = VulkanSemaphoreFlags::None,
        gp::UInt64 initialTimelineValue = 0u
    ) noexcept;

    /// @brief Wraps an externally created `VkSemaphore` handle.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] externalSemaphore Externally owned `VkSemaphore` handle.
    /// @param[in] flags Ownership/usage flags (defaults to `ExternallyOwned | ExternallySignaled`).
    VulkanSemaphore(
        VulkanDevice& device,
        VkSemaphore externalSemaphore,
        VulkanSemaphoreFlags flags = VulkanSemaphoreFlags::ExternallyOwned | VulkanSemaphoreFlags::ExternallySignaled
    ) noexcept;

    /// @brief Destructor. Destroys the native `VkSemaphore` unless `ExternallyOwned` is set.
    /// @note Immediate destruction is used until the Vulkan deferred deletion queue is integrated.
    ~VulkanSemaphore() noexcept;

    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;
    VulkanSemaphore(VulkanSemaphore&&) = delete;
    VulkanSemaphore& operator=(VulkanSemaphore&&) = delete;

    /// @brief Factory creating a reference-counted binary or timeline `VulkanSemaphore`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] flags Creation and ownership flags.
    /// @param[in] initialTimelineValue Initial timeline counter value.
    /// @return Shared `VulkanSemaphoreRef`.
    [[nodiscard]] static VulkanSemaphoreRef create(
        VulkanDevice& device,
        VulkanSemaphoreFlags flags = VulkanSemaphoreFlags::None,
        gp::UInt64 initialTimelineValue = 0u
    ) noexcept;

    /// @brief Factory wrapping an externally created `VkSemaphore`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] externalSemaphore Externally owned `VkSemaphore` handle.
    /// @param[in] flags Ownership/usage flags.
    /// @return Shared `VulkanSemaphoreRef`.
    [[nodiscard]] static VulkanSemaphoreRef createExternal(
        VulkanDevice& device,
        VkSemaphore externalSemaphore,
        VulkanSemaphoreFlags flags = VulkanSemaphoreFlags::ExternallyOwned | VulkanSemaphoreFlags::ExternallySignaled
    ) noexcept;

    /// @brief Returns the native `VkSemaphore` handle.
    /// @return Native `VkSemaphore`, or `VK_NULL_HANDLE` if creation failed.
    [[nodiscard]] VkSemaphore getHandle() const noexcept
    {
        return m_handle;
    }

    /// @brief Reports whether this wrapper currently holds a non-null `VkSemaphore`.
    /// @return True when `m_handle != VK_NULL_HANDLE`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_handle != VK_NULL_HANDLE;
    }

    /// @brief Returns the flags configured for this semaphore.
    /// @return `VulkanSemaphoreFlags` bitmask.
    [[nodiscard]] VulkanSemaphoreFlags getFlags() const noexcept
    {
        return m_flags;
    }

    /// @brief Reports whether the handle is externally owned and will not be destroyed on teardown.
    /// @return True when `ExternallyOwned` is set.
    [[nodiscard]] bool isExternallyOwned() const noexcept
    {
        return hasAnySemaphoreFlags(m_flags, VulkanSemaphoreFlags::ExternallyOwned);
    }

    /// @brief Reports whether the semaphore is signaled externally (for example by `vkAcquireNextImageKHR`).
    /// @return True when `ExternallySignaled` is set.
    [[nodiscard]] bool isExternallySignaled() const noexcept
    {
        return hasAnySemaphoreFlags(m_flags, VulkanSemaphoreFlags::ExternallySignaled);
    }

    /// @brief Reports whether the semaphore is waited on externally (for example by `vkQueuePresentKHR`).
    /// @return True when `ExternallyWaited` is set.
    [[nodiscard]] bool isExternallyWaited() const noexcept
    {
        return hasAnySemaphoreFlags(m_flags, VulkanSemaphoreFlags::ExternallyWaited);
    }

    /// @brief Reports whether this semaphore is a timeline semaphore (`VK_SEMAPHORE_TYPE_TIMELINE`).
    /// @return True when `Timeline` is set.
    [[nodiscard]] bool isTimeline() const noexcept
    {
        return hasAnySemaphoreFlags(m_flags, VulkanSemaphoreFlags::Timeline);
    }

    /// @brief Reports whether this semaphore destroys its handle immediately rather than using deferred deletion.
    /// @return True when `ImmediateDeletion` is set.
    [[nodiscard]] bool isImmediateDeletion() const noexcept
    {
        return hasAnySemaphoreFlags(m_flags, VulkanSemaphoreFlags::ImmediateDeletion);
    }

    /// @brief Queries the current 64-bit counter value of a timeline semaphore.
    /// @param[out] outValue Receives the current timeline value (unchanged if `MAX_UINT64` device-lost sentinel is
    ///                      returned or on failure).
    /// @return True when the counter value was queried successfully and is not the device-lost sentinel.
    [[nodiscard]] bool getTimelineSemaphoreValue(gp::UInt64& outValue) const noexcept;

    /// @brief Blocks the calling thread until the timeline semaphore reaches at least `value` or `timeoutNanoseconds`
    ///        expires.
    /// @param[in] value Target timeline counter value.
    /// @param[in] timeoutNanoseconds Maximum wait duration in nanoseconds.
    /// @return True when the semaphore reached `value`; false on timeout or error.
    [[nodiscard]] bool waitForTimelineSemaphoreValue(gp::UInt64 value, gp::UInt64 timeoutNanoseconds) const noexcept;

    /// @brief Signals the timeline semaphore from the host CPU to `value`.
    /// @param[in] value New monotonically increasing counter value.
    /// @return True when `vkSignalSemaphore` succeeded; false otherwise.
    [[nodiscard]] bool signalTimelineSemaphoreValue(gp::UInt64 value) const noexcept;

private:
    VulkanDevice& m_device;
    VkSemaphore m_handle{ VK_NULL_HANDLE };
    const VulkanSemaphoreFlags m_flags{ VulkanSemaphoreFlags::None };
};

}   // namespace gp::rhi::vulkan
