// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/JobSyncEnums.hpp"     // IWYU pragma: export
#include "threading/sync/JobSyncForward.hpp"   // IWYU pragma: export
#include "threading/sync/JobSyncTypes.hpp"     // IWYU pragma: export
#include "threading/sync/JobWaitNode.hpp"      // IWYU pragma: export

namespace gp::thread
{

/// @brief 16-byte lock-free job dependency counter with an intrusive stack-allocated wait list ([ADR-0050]).
/// @details Packs a 32-bit active job count (`bits [0..31]`), a 30-bit parked waiter count (`bits [32..61]`), a 1-bit
///          non-zero threshold waiter indicator (`bit [62]`), and a 1-bit Test-and-Test-and-Set (TTAS) wait-list latch
///          (`bit [63]`) into a single 64-bit `Atomic<UInt64>` alongside an intrusive `JobWaitNode*` head pointer:
///          - **Single-Instruction Uncontended Fast Path:** When `waiterCount == 0` and `kJobCounterLockBit == 0`, or
///            on intermediate decrements (`newCount > 0`) when all parked fibers wait for `0`
///            (`kJobCounterHasThresholdWaiterBit == 0`), `decrement()` executes a single `LOCK XADD` (`fetchSub`)
///            without acquiring the wait-list latch or touching `m_waitListHead`.
///          - **Zero Lost Wakeups:** `wait(targetCount)` acquires `kJobCounterLockBit`, re-verifies `count >
///            targetCount` while holding the bit, and links a stack-resident `JobWaitNode`. Any concurrent
///            `decrement()` that satisfies a waiter's threshold or races with wait-list insertion enters
///            `notifyWaiters()`.
///          - **Dual-Mode Waiting:** Supports both cooperative child `Fiber` yielding (via `JobFiberParker` or
///            `Fiber::yieldToReturnFiber()`) and OS-thread spin-waiting (`JobWaitMode::SpinOnly` or non-fiber callers).
class GP_CORE_API JobCounter final
{
private:
    Atomic<gp::UInt64> m_state{ 0ULL };
    JobWaitNode* m_waitListHead{ nullptr };

public:
    /// @brief Constructs a `JobCounter` initialized to `0` (`isComplete() == true`, 0 waiters).
    [[nodiscard]] constexpr JobCounter() noexcept = default;

    /// @brief Constructs a `JobCounter` with an initial active job count.
    /// @param[in] initialCount Initial 32-bit reference count (`0` to `kMaxJobCounterValue`).
    [[nodiscard]] constexpr explicit JobCounter(const JobCounterValue initialCount) noexcept
        : m_state(static_cast<gp::UInt64>(initialCount))
        , m_waitListHead(nullptr)
    {}

    /// @brief Destroys the counter, asserting in debug builds that no waiters remain parked on `m_waitListHead`.
    ~JobCounter() noexcept;

    JobCounter(const JobCounter&) = delete;
    JobCounter& operator=(const JobCounter&) = delete;
    JobCounter(JobCounter&&) = delete;
    JobCounter& operator=(JobCounter&&) = delete;

public:
    /// @section Reference Count Mutation & Synchronization

    /// @brief Atomically adds `delta` to the active job count.
    /// @param[in] delta Positive count increment (`delta > 0`).
    /// @param[out] outNewCount Optional pointer receiving the updated 32-bit count after addition.
    /// @return `JobSyncResult::Success`, `JobSyncResult::InvalidArguments` if `delta == 0`, or
    ///         `JobSyncResult::CounterOverflow` if adding `delta` would exceed `kMaxJobCounterValue`.
    [[nodiscard]] JobSyncResult add(JobCounterValue delta = 1u, JobCounterValue* outNewCount = nullptr) noexcept;

    /// @brief Atomically increments the active job count by `1`.
    /// @param[out] outNewCount Optional pointer receiving the updated 32-bit count after increment.
    /// @return `JobSyncResult::Success`, or `JobSyncResult::CounterOverflow` if already at `kMaxJobCounterValue`.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSyncResult increment(JobCounterValue* const outNewCount = nullptr) noexcept
    {
        return add(1u, outNewCount);
    }

    /// @brief Atomically subtracts `delta` from the active job count and wakes any parked waiters whose `targetCount`
    ///        is now satisfied (`newCount <= waiter.targetCount`).
    /// @param[in] delta Positive count decrement (`delta > 0`).
    /// @param[out] outNewCount Optional pointer receiving the updated 32-bit count after subtraction.
    /// @return `JobSyncResult::Success`, `JobSyncResult::InvalidArguments` if `delta == 0`, or
    ///         `JobSyncResult::CounterUnderflow` if `delta` exceeds the current active count.
    [[nodiscard]] JobSyncResult decrement(JobCounterValue delta = 1u, JobCounterValue* outNewCount = nullptr) noexcept;

    /// @brief Non-blocking poll checking whether the counter has reached or dropped below `targetCount`.
    /// @param[in] targetCount Threshold value to check against (defaults to `0u`).
    /// @return True if `getCount(MemoryOrder::Acquire) <= targetCount`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryWait(const JobCounterValue targetCount = 0u) const noexcept
    {
        return isAtOrBelow(targetCount, gp::MemoryOrder::Acquire);
    }

    /// @brief Waits until the counter's active count drops to or below `targetCount` (`getCount() <= targetCount`).
    /// @details If `getCount() <= targetCount` on entry, returns `JobSyncResult::Success` immediately with zero stack
    ///          wait-node linkage. Otherwise allocates a `JobWaitNode` on the current stack frame, links it into
    ///          `m_waitListHead`, and either cooperatively yields the active child `gp::thread::Fiber` or spins until
    ///          woken by a matching `decrement()`.
    /// @param[in] targetCount Threshold value at or below which the wait is satisfied (defaults to `0u`).
    /// @param[in] mode Execution wait policy (`JobWaitMode::Auto` or `JobWaitMode::SpinOnly`).
    /// @return `JobSyncResult::Success` once `getCount() <= targetCount`, or `JobSyncResult::YieldFailed`.
    [[nodiscard]] JobSyncResult wait(JobCounterValue targetCount = 0u, JobWaitMode mode = JobWaitMode::Auto) noexcept;

    /// @brief Resets the counter's active count to `newCount` when no waiters are parked.
    /// @param[in] newCount Desired 32-bit active job count.
    /// @return `JobSyncResult::Success`, or `JobSyncResult::BusyWaiting` if waiters are currently parked.
    [[nodiscard]] JobSyncResult reset(JobCounterValue newCount = 0u) noexcept;

public:
    /// @section State Queries

    /// @brief Queries the current 32-bit active job count.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return Current 32-bit job count (`0` to `kMaxJobCounterValue`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobCounterValue
        getCount(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return static_cast<JobCounterValue>(m_state.load(order) & kJobCounterValueMask);
    }

    /// @brief Queries the number of waiters currently parked on this counter's intrusive wait list.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return Current 30-bit waiter count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
        getWaiterCount(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return static_cast<gp::UInt32>((m_state.load(order) & kJobCounterWaiterMask) >> kJobCounterWaiterShift);
    }

    /// @brief Checks whether any currently parked waiter has a non-zero `targetCount` (`targetCount > 0`).
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `kJobCounterHasThresholdWaiterBit` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasThresholdWaiters(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & kJobCounterHasThresholdWaiterBit) != 0ULL;
    }

    /// @brief Checks whether the counter has reached zero (`getCount() == 0u`).
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if the active job count is zero, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isComplete(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return getCount(order) == 0u;
    }

    /// @brief Checks whether the counter's active count is at or below `targetCount`.
    /// @param[in] targetCount Threshold value to compare against.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `getCount(order) <= targetCount`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAtOrBelow(
        const JobCounterValue targetCount, const gp::MemoryOrder order = gp::MemoryOrder::Acquire
    ) const noexcept
    {
        return getCount(order) <= targetCount;
    }

    /// @brief Extracts the 32-bit `count` field from a raw 64-bit packed `JobCounter` state word.
    /// @param[in] packedState Raw 64-bit state word.
    /// @return Extracted 32-bit `JobCounterValue`.
    [[nodiscard]] static constexpr JobCounterValue unpackCount(const gp::UInt64 packedState) noexcept
    {
        return static_cast<JobCounterValue>(packedState & kJobCounterValueMask);
    }

    /// @brief Extracts the 31-bit `waiterCount` field from a raw 64-bit packed `JobCounter` state word.
    /// @param[in] packedState Raw 64-bit state word.
    /// @return Extracted 31-bit waiter count.
    [[nodiscard]] static constexpr gp::UInt32 unpackWaiterCount(const gp::UInt64 packedState) noexcept
    {
        return static_cast<gp::UInt32>((packedState & kJobCounterWaiterMask) >> kJobCounterWaiterShift);
    }

private:
    void acquireWaitListLock() noexcept;
    void notifyWaiters() noexcept;
};

static_assert(sizeof(JobCounter) == 16u, "JobCounter must be compact (16 bytes: 8B state + 8B head pointer).");

/// @brief 64-byte cache-line-isolated wrapper around `gp::thread::JobCounter` preventing MESI false sharing during
///        high-contention `parallelFor` decrements across all hardware worker threads ([ADR-0025], [ADR-0050]).
struct alignas(gp::memory::HardwareConstants::cacheLineSize) GP_CORE_API CacheAlignedJobCounter final
{
    /// @brief Underlying 16-byte job dependency counter.
    JobCounter counter{};

    /// @brief Explicit padding bytes isolating `counter` onto a dedicated 64-byte CPU cache line.
    gp::UInt8 padding[gp::memory::HardwareConstants::cacheLineSize - sizeof(JobCounter)]{};

    /// @brief Constructs a cache-aligned counter initialized to `0`.
    [[nodiscard]] constexpr CacheAlignedJobCounter() noexcept = default;

    /// @brief Constructs a cache-aligned counter with an initial active job count.
    /// @param[in] initialCount Initial 32-bit reference count.
    [[nodiscard]] constexpr explicit CacheAlignedJobCounter(const JobCounterValue initialCount) noexcept
        : counter(initialCount)
    {}

    CacheAlignedJobCounter(const CacheAlignedJobCounter&) = delete;
    CacheAlignedJobCounter& operator=(const CacheAlignedJobCounter&) = delete;
    CacheAlignedJobCounter(CacheAlignedJobCounter&&) = delete;
    CacheAlignedJobCounter& operator=(CacheAlignedJobCounter&&) = delete;

    /// @brief Destructor.
    ~CacheAlignedJobCounter() noexcept = default;

    /// @brief Forwards `add()` to the underlying `JobCounter`.
    /// @param[in] delta Positive count increment (`delta > 0`).
    /// @param[out] outNewCount Optional pointer receiving the updated 32-bit count.
    /// @return `JobSyncResult` status code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSyncResult
        add(const JobCounterValue delta = 1u, JobCounterValue* const outNewCount = nullptr) noexcept
    {
        return counter.add(delta, outNewCount);
    }

    /// @brief Forwards `increment()` to the underlying `JobCounter`.
    /// @param[out] outNewCount Optional pointer receiving the updated 32-bit count.
    /// @return `JobSyncResult` status code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSyncResult increment(JobCounterValue* const outNewCount = nullptr) noexcept
    {
        return counter.increment(outNewCount);
    }

    /// @brief Forwards `decrement()` to the underlying `JobCounter`.
    /// @param[in] delta Positive count decrement (`delta > 0`).
    /// @param[out] outNewCount Optional pointer receiving the updated 32-bit count.
    /// @return `JobSyncResult` status code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSyncResult
        decrement(const JobCounterValue delta = 1u, JobCounterValue* const outNewCount = nullptr) noexcept
    {
        return counter.decrement(delta, outNewCount);
    }

    /// @brief Forwards `tryWait()` to the underlying `JobCounter`.
    /// @param[in] targetCount Threshold value to check against (defaults to `0u`).
    /// @return True if `getCount(MemoryOrder::Acquire) <= targetCount`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryWait(const JobCounterValue targetCount = 0u) const noexcept
    {
        return counter.tryWait(targetCount);
    }

    /// @brief Forwards `wait()` to the underlying `JobCounter`.
    /// @param[in] targetCount Threshold value at or below which the wait is satisfied.
    /// @param[in] mode Execution wait policy.
    /// @return `JobSyncResult` status code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSyncResult
        wait(const JobCounterValue targetCount = 0u, const JobWaitMode mode = JobWaitMode::Auto) noexcept
    {
        return counter.wait(targetCount, mode);
    }

    /// @brief Forwards `reset()` to the underlying `JobCounter`.
    /// @param[in] newCount Desired 32-bit active job count.
    /// @return `JobSyncResult` status code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSyncResult reset(const JobCounterValue newCount = 0u) noexcept
    {
        return counter.reset(newCount);
    }

    /// @brief Queries the current 32-bit active job count.
    /// @param[in] order Memory ordering constraint.
    /// @return Current 32-bit job count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobCounterValue
        getCount(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return counter.getCount(order);
    }

    /// @brief Queries the number of parked waiters.
    /// @param[in] order Memory ordering constraint.
    /// @return Current 30-bit waiter count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
        getWaiterCount(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return counter.getWaiterCount(order);
    }

    /// @brief Checks whether any currently parked waiter has a non-zero `targetCount`.
    /// @param[in] order Memory ordering constraint.
    /// @return True if `kJobCounterHasThresholdWaiterBit` is set.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        hasThresholdWaiters(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return counter.hasThresholdWaiters(order);
    }

    /// @brief Checks whether the counter has reached zero.
    /// @param[in] order Memory ordering constraint.
    /// @return True if `getCount(order) == 0u`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isComplete(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return counter.isComplete(order);
    }

    /// @brief Checks whether the counter's active count is at or below `targetCount`.
    /// @param[in] targetCount Threshold value to compare against.
    /// @param[in] order Memory ordering constraint.
    /// @return True if `getCount(order) <= targetCount`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAtOrBelow(
        const JobCounterValue targetCount, const gp::MemoryOrder order = gp::MemoryOrder::Acquire
    ) const noexcept
    {
        return counter.isAtOrBelow(targetCount, order);
    }
};

static_assert(
    sizeof(CacheAlignedJobCounter) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedJobCounter size must match the target CPU cache line size (64 bytes)."
);

}   // namespace gp::thread
