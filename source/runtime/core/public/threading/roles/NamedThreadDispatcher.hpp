// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/queues/JobQueueTypes.hpp"
#include "threading/queues/MPMCRingQueue.hpp"
#include "threading/roles/NamedThreadDispatcherDesc.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "threading/roles/ThreadRoleForward.hpp"
#include "threading/roles/ThreadRoleTypes.hpp"
#include "threading/sync/JobSyncForward.hpp"

namespace gp::thread
{

/// @brief One bounded lock-free task queue per named thread, drained only by the thread that owns that role.
/// @details This covers the work that must happen on a specific thread and cannot go to the job system: graphics API
///          calls bound to the RHI thread, render-resource updates owned by the render thread, and gameplay callbacks
///          that must land on the game thread. Unreal Engine exposes the same idea through
///          `FTaskGraphInterface::QueueTask(..., ENamedThreads::RenderThread)` and
///          `ProcessThreadUntilIdle(ENamedThreads::GameThread)`.
///
///          Design:
///          - Producers on any thread call `dispatch()`. Each queue is an `MPMCRingQueue<NamedThreadTask>`
///            ([ADR-0051]), so producers never take a lock and each slot sits on its own cache line.
///          - The consumer is whoever holds the matching exclusive role. `pump()` checks the caller's TLS role mask
///            and returns `WrongThread` otherwise, which turns the single-consumer contract into a checked rule.
///          - The owner pumps at points it chooses (start of frame, between passes, while idle). Nothing runs behind
///            its back, so the order of side effects on each named thread stays deterministic.
///          - Storage is allocated once in `initialize()`. Dispatch and pump never allocate.
/// @note Instances are not movable. `initialize()` and `shutdown()` must not race with `dispatch()` or `pump()`.
class GP_CORE_API NamedThreadDispatcher final
{
public:
    /// @brief Constructs an uninitialized dispatcher. No memory is allocated until `initialize()`.
    [[nodiscard]] constexpr NamedThreadDispatcher() noexcept = default;

    /// @brief Destroys the dispatcher, calling `shutdown()` if needed.
    ~NamedThreadDispatcher() noexcept;

    NamedThreadDispatcher(const NamedThreadDispatcher&) = delete;
    NamedThreadDispatcher& operator=(const NamedThreadDispatcher&) = delete;
    NamedThreadDispatcher(NamedThreadDispatcher&&) = delete;
    NamedThreadDispatcher& operator=(NamedThreadDispatcher&&) = delete;

public:
    /// @section Lifecycle

    /// @brief Allocates one queue per named thread.
    /// @param[in] desc Queue capacities and allocator.
    /// @return `Success`, `AlreadyInitialized`, `InvalidArguments` if a capacity is zero or too large, or
    ///         `OutOfMemory`. On failure no queue stays allocated.
    [[nodiscard]] ThreadRoleResult
        initialize(const NamedThreadDispatcherDesc& desc = NamedThreadDispatcherDesc{}) noexcept;

    /// @brief Releases every queue. Safe to call more than once.
    /// @note Tasks still queued are dropped without running and their counters are not decremented. Owners should
    ///       pump their queues until idle before the engine calls this.
    void shutdown() noexcept;

    /// @brief Checks whether the dispatcher accepts tasks.
    /// @return True between a successful `initialize()` and `shutdown()`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isInitialized() const noexcept
    {
        return m_isInitialized.load(gp::MemoryOrder::Acquire) != 0u;
    }

public:
    /// @section Producer side (any thread)

    /// @brief Enqueues `task` on the queue of `target`.
    /// @details If `task.counter` is set it is incremented before the push and decremented again if the push fails,
    ///          so the counter always matches the number of tasks that are queued or running.
    /// @param[in] target Named thread that will run the task.
    /// @param[in] task Task to enqueue. `task.entry` must not be null.
    /// @return `Success`, `NotInitialized`, `InvalidArguments`, or `QueueFull`.
    [[nodiscard]] ThreadRoleResult dispatch(NamedThread target, const NamedThreadTask& task) noexcept;

    /// @brief Builds a `NamedThreadTask` from its parts and enqueues it on the queue of `target`.
    /// @param[in] target Named thread that will run the task.
    /// @param[in] entry Callback to run. Must not be null.
    /// @param[in] userData Opaque payload forwarded to `entry`.
    /// @param[in,out] counter Optional completion counter, incremented now and decremented after `entry` returns.
    /// @return `Success`, `NotInitialized`, `InvalidArguments`, or `QueueFull`.
    [[nodiscard]] GP_FORCEINLINE_HINT ThreadRoleResult dispatch(
        const NamedThread target,
        const JobEntryFunction entry,
        void* const userData = nullptr,
        JobCounter* const counter = nullptr
    ) noexcept
    {
        NamedThreadTask task{};
        task.entry = entry;
        task.userData = userData;
        task.counter = counter;
        return dispatch(target, task);
    }

public:
    /// @section Consumer side (owner thread only)

    /// @brief Runs up to `maxTaskCount` queued tasks of `target` on the calling thread, oldest first.
    /// @details Stops early when the queue looks empty. Tasks may dispatch more work to any queue, including the one
    ///          being pumped. Passing a finite `maxTaskCount` keeps a self-requeueing task from holding the owner
    ///          forever.
    /// @param[in] target Named thread whose queue to drain. The caller must hold `toThreadRole(target)`.
    /// @param[in] maxTaskCount Upper bound on tasks to run. `0` runs nothing; `kPumpAllTasks` drains until empty.
    /// @param[out] outExecutedCount Receives how many tasks ran. Always written, set to `0` on failure.
    /// @return `Success` (also when the queue was empty), `NotInitialized`, `InvalidArguments`, or `WrongThread`.
    [[nodiscard]] ThreadRoleResult
        pump(NamedThread target, gp::UInt32 maxTaskCount, gp::UInt32& outExecutedCount) noexcept;

    /// @brief Runs queued tasks of `target` until the queue looks empty.
    /// @param[in] target Named thread whose queue to drain. The caller must hold `toThreadRole(target)`.
    /// @param[out] outExecutedCount Receives how many tasks ran.
    /// @return Same values as `pump()`.
    [[nodiscard]] GP_FORCEINLINE_HINT ThreadRoleResult
        pumpUntilIdle(const NamedThread target, gp::UInt32& outExecutedCount) noexcept
    {
        return pump(target, kPumpAllTasks, outExecutedCount);
    }

public:
    /// @section Queries

    /// @brief Returns an approximate count of tasks waiting in the queue of `target`.
    /// @param[in] target Named thread to inspect.
    /// @return Queued task count, or `0` if uninitialized or `target` is out of range. The value can be stale as soon
    ///         as it is returned.
    [[nodiscard]] gp::USize getPendingTaskCountApprox(NamedThread target) const noexcept;

    /// @brief Returns the slot count of the queue of `target`.
    /// @param[in] target Named thread to inspect.
    /// @return Power-of-two capacity, or `0` if uninitialized or `target` is out of range.
    [[nodiscard]] gp::USize getQueueCapacity(NamedThread target) const noexcept;

private:
    MPMCRingQueue<NamedThreadTask> m_queues[kNamedThreadCount]{};
    Atomic<gp::UInt8> m_isInitialized{ 0u };
};

}   // namespace gp::thread
