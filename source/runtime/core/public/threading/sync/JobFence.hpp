// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/JobSyncEnums.hpp"     // IWYU pragma: export
#include "threading/sync/JobSyncForward.hpp"   // IWYU pragma: export
#include "threading/sync/JobSyncTypes.hpp"     // IWYU pragma: export
#include "threading/sync/JobWaitNode.hpp"      // IWYU pragma: export

namespace gp::thread
{

/// @brief 8-byte pointer-packed binary completion barrier for frame-phase transitions and one-shot job handoffs
///        ([ADR-0050]).
/// @details Packs the 8-byte-aligned intrusive `JobWaitNode*` wait-list head pointer alongside `kJobFenceSignaledBit`
///          (`bit [0]`) and `kJobFenceLockBit` (`bit [1]`) inside a single pointer-sized `Atomic<UPtr>`:
///          - **Zero Heap Allocations:** Waiting fibers and OS threads allocate `JobWaitNode` directly on their own
///            call stacks.
///          - **Atomic Bulk Detach on `signal()`:** Signaling the fence sets `kJobFenceSignaledBit` and detaches the
///            entire intrusive wait list in one atomic transition, waking all waiting fibers and OS threads in FIFO
///            submission order.
class GP_CORE_API JobFence final
{
private:
    Atomic<gp::UIntPtr> m_state{ 0ULL };

public:
    /// @brief Constructs an unsignaled `JobFence` (`isSignaled() == false`).
    [[nodiscard]] constexpr JobFence() noexcept = default;

    /// @brief Constructs a `JobFence` with an explicit initial signal state.
    /// @param[in] initiallySignaled True to construct in the signaled state, false for unsignaled.
    [[nodiscard]] constexpr explicit JobFence(const bool initiallySignaled) noexcept
        : m_state(initiallySignaled ? kJobFenceSignaledBit : 0ULL)
    {}

    /// @brief Destroys the fence, asserting in debug builds that no waiters remain parked on its wait list.
    ~JobFence() noexcept;

    JobFence(const JobFence&) = delete;
    JobFence& operator=(const JobFence&) = delete;
    JobFence(JobFence&&) = delete;
    JobFence& operator=(JobFence&&) = delete;

public:
    /// @section Barrier Operations

    /// @brief Transitions the fence to the signaled state and wakes all parked fiber and OS-thread waiters.
    /// @note Idempotent; calling `signal()` on an already-signaled fence is a safe no-op.
    void signal() noexcept;

    /// @brief Resets the fence back to the unsignaled state when no waiters are parked.
    /// @return `JobSyncResult::Success` if reset, or `JobSyncResult::BusyWaiting` if waiters are currently parked.
    [[nodiscard]] JobSyncResult reset() noexcept;

    /// @brief Non-blocking poll checking whether the fence is currently signaled.
    /// @return True if `isSignaled(MemoryOrder::Acquire)` is true, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryWait() const noexcept
    {
        return isSignaled(gp::MemoryOrder::Acquire);
    }

    /// @brief Waits until the fence is signaled (`isSignaled() == true`).
    /// @details Returns `JobSyncResult::Success` immediately if the fence is already signaled. Otherwise links a
    ///          stack-allocated `JobWaitNode` into the pointer-packed wait list and either cooperatively yields the
    ///          active child `gp::thread::Fiber` or spins until `signal()` is invoked.
    /// @param[in] mode Execution wait policy (`JobWaitMode::Auto` or `JobWaitMode::SpinOnly`).
    /// @return `JobSyncResult::Success` once signaled, or `JobSyncResult::YieldFailed`.
    [[nodiscard]] JobSyncResult wait(JobWaitMode mode = JobWaitMode::Auto) noexcept;

public:
    /// @section State Queries

    /// @brief Checks whether the fence is currently in the signaled state.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `kJobFenceSignaledBit` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isSignaled(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & kJobFenceSignaledBit) != 0ULL;
    }

    /// @brief Checks whether one or more waiters are currently parked on this fence's wait list.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if the packed `JobWaitNode*` head pointer is non-null.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasWaiters(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & kJobFencePointerMask) != 0ULL;
    }
};

static_assert(sizeof(JobFence) == sizeof(gp::UIntPtr), "JobFence must be pointer-sized (8 bytes on 64-bit targets).");

}   // namespace gp::thread
