// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

class VulkanFenceManager;

/// @brief Internal Vulkan fence wrapper managed exclusively by `VulkanFenceManager`.
/// @details Tracks the cached signaled state (`NotReady` vs `Signaled`) alongside the native `VkFence` handle so
///          repeated polls avoid redundant driver calls once a fence has completed.
class GP_RHI_VULKAN_API VulkanFence final
{
public:
    /// @brief Constructs a `VkFence` owned by `owner`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] owner Owning `VulkanFenceManager`.
    /// @param[in] createSignaled True to create the fence in the `VK_FENCE_CREATE_SIGNALED_BIT` state.
    VulkanFence(VulkanDevice& device, VulkanFenceManager& owner, bool createSignaled) noexcept;

    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;
    VulkanFence(VulkanFence&&) = delete;
    VulkanFence& operator=(VulkanFence&&) = delete;

    /// @brief Returns the native `VkFence` handle.
    /// @return Native `VkFence`, or `VK_NULL_HANDLE` if creation failed.
    [[nodiscard]] VkFence getHandle() const noexcept
    {
        return m_handle;
    }

    /// @brief Reports whether the cached CPU state indicates this fence is signaled.
    /// @return True when cached state is `State::Signaled`.
    [[nodiscard]] bool isSignaled() const noexcept
    {
        return m_state == State::Signaled;
    }

    /// @brief Returns the owning `VulkanFenceManager`.
    /// @return Mutable reference to `VulkanFenceManager`.
    [[nodiscard]] VulkanFenceManager& getOwner() noexcept
    {
        return m_owner;
    }

private:
    /// @brief Cached CPU-side completion state.
    enum class State : gp::UInt8
    {
        NotReady = 0u,
        Signaled
    };

    /// @brief Private destructor enforcing destruction exclusively through `VulkanFenceManager`.
    ~VulkanFence() noexcept;

    VkFence m_handle{ VK_NULL_HANDLE };
    State m_state{ State::NotReady };
    VulkanFenceManager& m_owner;

    friend class VulkanFenceManager;
};

/// @brief Pool manager allocating, polling, waiting on, resetting, and recycling `VulkanFence` instances.
/// @note Pool acquire, release, and teardown operations are synchronized with `m_fenceLock`. Per-fence operations
///       (`isFenceSignaled`, `waitForFence`, `resetFence`) assume single-owner access to each checked-out fence.
class GP_RHI_VULKAN_API VulkanFenceManager final
{
public:
    /// @brief Constructs a fence manager bound to `device`.
    /// @param[in] device Owning `VulkanDevice`.
    explicit VulkanFenceManager(VulkanDevice& device) noexcept
        : m_device(device)
    {}

    /// @brief Destructor. Ensures all pooled fences are destroyed via `deinit()`.
    ~VulkanFenceManager() noexcept;

    VulkanFenceManager(const VulkanFenceManager&) = delete;
    VulkanFenceManager& operator=(const VulkanFenceManager&) = delete;
    VulkanFenceManager(VulkanFenceManager&&) = delete;
    VulkanFenceManager& operator=(VulkanFenceManager&&) = delete;

    /// @brief Destroys all pooled free fences (and logs a warning if any fences remain checked out).
    void deinit() noexcept;

    /// @brief Acquires a `VulkanFence` from the free pool or allocates a new `VkFence` from the device.
    /// @param[in] createSignaled True to obtain a fence initially marked signaled.
    /// @return Pointer to an active `VulkanFence`, or `nullptr` if the device is invalid or allocation failed.
    [[nodiscard]] VulkanFence* allocateFence(bool createSignaled = false) noexcept;

    /// @brief Polls whether `fence` has been signaled by the GPU without blocking.
    /// @param[in] fence Fence to inspect.
    /// @return True when signaled; false if unsignaled or null.
    [[nodiscard]] bool isFenceSignaled(VulkanFence* fence) noexcept;

    /// @brief Blocks until `fence` becomes signaled or `timeInNanoseconds` expires.
    /// @param[in] fence Fence to wait on.
    /// @param[in] timeInNanoseconds Maximum wait duration in nanoseconds.
    /// @return True when signaled; false on timeout or invalid input.
    [[nodiscard]] bool waitForFence(VulkanFence* fence, gp::UInt64 timeInNanoseconds) noexcept;

    /// @brief Blocks until any fence in `fences` becomes signaled or `timeInNanoseconds` expires.
    /// @param[in] fences View of candidate `VulkanFence` pointers.
    /// @param[in] timeInNanoseconds Maximum wait duration in nanoseconds.
    /// @return True when at least one fence is signaled; false on timeout or empty input.
    [[nodiscard]] bool waitForAnyFence(gp::ConstVectorView<VulkanFence*> fences, gp::UInt64 timeInNanoseconds) noexcept;

    /// @brief Resets a signaled `fence` back to the unsignaled state via `vkResetFences`.
    /// @param[in] fence Fence to reset.
    void resetFence(VulkanFence* fence) noexcept;

    /// @brief Resets `fence`, returns it to the free pool (or destroys it if reuse is disabled), and nulls the pointer.
    /// @param[in,out] fence Reference to the fence pointer; set to `nullptr` on return.
    void releaseFence(VulkanFence*& fence) noexcept;

    /// @brief Waits for `fence` if not yet signaled, resets it, returns it to the free pool, and nulls the pointer.
    /// @param[in,out] fence Reference to the fence pointer; set to `nullptr` on return.
    /// @param[in] timeInNanoseconds Maximum wait duration in nanoseconds.
    void waitAndReleaseFence(VulkanFence*& fence, gp::UInt64 timeInNanoseconds) noexcept;

    /// @brief Returns the number of idle fences currently held in the free pool.
    /// @return Free fence count.
    [[nodiscard]] gp::USize getFreeFenceCount() const noexcept;

    /// @brief Returns the number of active fences currently checked out to callers.
    /// @return Active fence count.
    [[nodiscard]] gp::USize getUsedFenceCount() const noexcept;

private:
    /// @brief Queries `vkGetFenceStatus` for an unsignaled fence and updates its cached state.
    /// @param[in] fence Fence to poll.
    /// @return True when `VK_SUCCESS` is returned.
    [[nodiscard]] bool checkFenceState(VulkanFence* fence) noexcept;

    /// @brief Removes `fence` from `m_usedFences` using swap-remove.
    /// @param[in] fence Fence pointer to remove.
    void removeUsedFenceLocked(VulkanFence* fence) noexcept;

    /// @brief Destroys the native `VkFence` handle and deletes `fence`.
    /// @param[in] fence Fence to destroy.
    void destroyFence(VulkanFence* fence) noexcept;

    VulkanDevice& m_device;
    mutable gp::thread::Mutex m_fenceLock{};
    gp::Vector<VulkanFence*> m_freeFences{};
    gp::Vector<VulkanFence*> m_usedFences{};
};

}   // namespace gp::rhi::vulkan
