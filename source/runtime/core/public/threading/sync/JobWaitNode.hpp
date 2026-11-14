// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/FiberForward.hpp"
#include "threading/sync/JobSyncEnums.hpp"     // IWYU pragma: export
#include "threading/sync/JobSyncForward.hpp"   // IWYU pragma: export
#include "threading/sync/JobSyncTypes.hpp"     // IWYU pragma: export

namespace gp::thread
{

/// @brief Stack-allocated intrusive wait-list node representing a suspended `gp::thread::Fiber` or spinning OS thread
///        waiting on a `JobCounter` or `JobFence` ([ADR-0050]).
/// @details Allocated directly on the caller's active call stack inside `JobCounter::wait()` or `JobFence::wait()`.
///          Because a suspended `Fiber` (or blocked OS thread) preserves its stack frame in virtual memory until the
///          wait completes, linking `JobWaitNode` intrusively requires zero heap allocations and imposes no fixed
///          limit on concurrent waiters.
/// @note Aligned to 8 bytes so `JobFence` can pack `kJobFenceSignaledBit` and `kJobFenceLockBit` into the lowest 2
///       alignment bits of a `JobWaitNode*` pointer.
struct alignas(8) JobWaitNode final
{
    /// @brief Intrusive pointer to the next waiting node in the counter or fence wait list.
    JobWaitNode* next{ nullptr };

    /// @brief Pointer to the suspended child `Fiber` awaiting resumption, or `nullptr` if an OS thread is waiting.
    Fiber* waitingFiber{ nullptr };

    /// @brief Stateless callback invoked by the waker thread to re-schedule `waitingFiber` when unblocked.
    JobWakeFunction wakeCallback{ nullptr };

    /// @brief Opaque scheduler or user context pointer forwarded to `wakeCallback`.
    void* wakeUserData{ nullptr };

    /// @brief Target counter value at or below which this waiter is satisfied (`0u` for completion or fence waits).
    JobCounterValue targetCount{ 0u };

    /// @brief Atomic lifecycle state transitioning from `Waiting` to `Signaled` when detached by the waker.
    Atomic<JobWaitNodeState> state{ JobWaitNodeState::Idle };

    /// @brief Checks whether this wait node has been detached and marked `JobWaitNodeState::Signaled`.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `state == JobWaitNodeState::Signaled`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isSignaled(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return state.load(order) == JobWaitNodeState::Signaled;
    }
};

static_assert(alignof(JobWaitNode) >= 8u, "JobWaitNode must be at least 8-byte aligned for pointer bit-packing.");

/// @brief Thread-local scheduler hook pairing stateless fiber-yield and fiber-wake callbacks ([ADR-0022], [ADR-0050]).
/// @details Worker threads in the `M:N` fiber scheduler bind a `JobFiberParker` on startup so `JobCounter::wait()` and
///          `JobFence::wait()` automatically park yielding child fibers and push woken fibers back onto the
///          scheduler's ready queues without circular header or module dependencies.
struct GP_CORE_API JobFiberParker final
{
    /// @brief Callback invoked on the waiting child fiber to switch back to the worker thread's scheduler loop.
    JobYieldFunction yieldCallback{ nullptr };

    /// @brief Callback invoked on the decrementing/signaling thread to enqueue a woken `Fiber*`.
    JobWakeFunction wakeCallback{ nullptr };

    /// @brief Opaque scheduler context pointer forwarded to both `yieldCallback` and `wakeCallback`.
    void* userData{ nullptr };

    /// @brief Checks whether both `yieldCallback` and `wakeCallback` are non-null.
    /// @return True if this parker is fully configured.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return yieldCallback != nullptr && wakeCallback != nullptr;
    }

    /// @brief Binds `parker` as the active fiber parker for the calling OS worker thread.
    /// @note Implemented out-of-line with `GP_FORCENOINLINE` to prevent TLS base-address register caching across
    ///       cooperative fiber switches and core migrations ([ADR-0049], [ADR-0050]).
    /// @param[in] parker New parker descriptor to install on the calling OS thread.
    /// @return Previously bound `JobFiberParker` on the calling OS thread.
    static JobFiberParker bindThreadParker(const JobFiberParker& parker) noexcept;

    /// @brief Queries the `JobFiberParker` currently bound to the calling OS worker thread.
    /// @return Active `JobFiberParker` on the calling OS thread.
    [[nodiscard]] static JobFiberParker getThreadParker() noexcept;

    /// @brief Checks whether a valid `JobFiberParker` is currently bound to the calling OS worker thread.
    /// @return True if `getThreadParker().isValid()`.
    [[nodiscard]] static bool hasThreadParker() noexcept;
};

/// @brief RAII scope guard that binds a `JobFiberParker` to the calling OS worker thread on construction and restores
///        the previous `JobFiberParker` on destruction.
class GP_CORE_API ScopedJobFiberParker final
{
private:
    JobFiberParker m_previousParker{};

public:
    /// @brief Binds `parker` to the calling OS thread and saves the prior binding.
    /// @param[in] parker Parker configuration to activate for the duration of this scope.
    explicit ScopedJobFiberParker(const JobFiberParker& parker) noexcept
        : m_previousParker(JobFiberParker::bindThreadParker(parker))
    {}

    /// @brief Restores the calling OS thread's previous `JobFiberParker` binding.
    ~ScopedJobFiberParker() noexcept
    {
        (void)JobFiberParker::bindThreadParker(m_previousParker);
    }

    ScopedJobFiberParker(const ScopedJobFiberParker&) = delete;
    ScopedJobFiberParker& operator=(const ScopedJobFiberParker&) = delete;
    ScopedJobFiberParker(ScopedJobFiberParker&&) = delete;
    ScopedJobFiberParker& operator=(ScopedJobFiberParker&&) = delete;

public:
    /// @brief Retrieves the previous `JobFiberParker` that will be restored when this guard exits scope.
    /// @return Const reference to `m_previousParker`.
    [[nodiscard]] constexpr const JobFiberParker& getPreviousParker() const noexcept
    {
        return m_previousParker;
    }
};

}   // namespace gp::thread
