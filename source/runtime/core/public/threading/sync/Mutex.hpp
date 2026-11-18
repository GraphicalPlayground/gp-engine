// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/sync/MutexForward.hpp"   // IWYU pragma: export
#include "threading/sync/MutexTypes.hpp"     // IWYU pragma: export

namespace gp::thread
{

/// @brief Compact 4-byte non-recursive mutual exclusion lock backed by an inline atomic fast path and OS kernel
///        address-based parking (`gp::platform::Futex`) ([ADR-0058]).
/// @details Modeled after Unreal Engine 5's `UE::FMutex` and WebKit/Rust `ParkingLot`:
///          - **4-Byte Footprint & `constexpr` Construction:** Packs `kMutexLockedBit` (`0x1`) and
///            `kMutexMayHaveWaitersBit` (`0x2`) into a single 32-bit `Atomic<UInt32>`, allowing 16 mutexes to fit
///            inside one 64-byte CPU cache line with zero dynamic memory allocation and zero static initialization
///            order hazards.
///          - **Single-Instruction Uncontended Fast Path:** Uncontended `lock()`, `tryLock()`, and `unlock()` execute
///            inline in the header via a single atomic instruction without crossing DLL boundaries or invoking kernel
///            syscalls.
///          - **Adaptive Spin-Then-Futex Contention Path:** When contended, `lockSlow()` spins in user space for
///            `kMutexSpinIterationCount` (`32`) iterations before setting `kMutexMayHaveWaitersBit` and parking OS
///            threads via `Futex::waitOnAddress()`. When called from inside a cooperative child `Fiber` ([ADR-0049]),
///            the slow path spins/yields in user space so the underlying `JobSystem` worker thread is never put into an
///            unscheduled kernel sleep.
///          - **Mechanical Fiber Non-Yielding Enforcement (`GP_DO_CHECK`):** Acquiring a `Mutex` increments the active
///            execution context's non-yielding lock counter (`Fiber::adjustActiveNonYieldingLockCount`), causing
///            `Fiber::switchTo()` to assert immediately if a child fiber illegally yields while holding a mutex.
/// @note Non-recursive: attempting to re-lock a `Mutex` already held by the calling execution context is a fatal bug
///       that deadlocks. Use `gp::thread::RecursiveMutex` (`gp::thread::CriticalSection`) when re-entrant acquisition
///       is required.
class GP_THREAD_CAPABILITY("mutex") GP_CORE_API Mutex final
{
public:
    /// @brief Constructs an unlocked `Mutex` (`isLocked() == false`).
    [[nodiscard]] constexpr Mutex() noexcept = default;

    /// @brief Trivially destructible; requires no OS kernel handle cleanup.
    constexpr ~Mutex() noexcept = default;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    Mutex& operator=(Mutex&&) = delete;

public:
    /// @brief Acquires exclusive ownership of the mutex, blocking if currently held by another thread or fiber.
    /// @details Executes an inline compare-and-exchange (`0u -> kMutexLockedBit`) on the uncontended fast path and
    ///          falls back to `lockSlow()` only when contention is detected.
    GP_ACQUIRE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void lock() noexcept
    {
        gp::UInt32 expected = 0u;
        if (m_state.compareExchangeWeak(expected, kMutexLockedBit, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed))
            [[likely]]
        {
#if GP_DO_CHECK
            Fiber::adjustActiveNonYieldingLockCount(1);
#endif
            return;
        }
        lockSlow();
    }

    /// @brief Attempts to acquire exclusive ownership of the mutex without blocking.
    /// @return True if the lock was acquired, false if already held by another execution context.
    [[nodiscard]] GP_TRY_ACQUIRE_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLock() noexcept
    {
        if consteval
        {
            gp::UInt32 expected = 0u;
            return m_state.compareExchangeStrong(
                expected, kMutexLockedBit, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
            );
        }
        else
        {
            const gp::UInt32 current = m_state.load(gp::MemoryOrder::Relaxed);
            if ((current & kMutexLockedBit) != 0u)
            {
                return false;
            }
            const bool acquired = (m_state.fetchOr(kMutexLockedBit, gp::MemoryOrder::Acquire) & kMutexLockedBit) == 0u;
#if GP_DO_CHECK
            if (acquired)
            {
                Fiber::adjustActiveNonYieldingLockCount(1);
            }
#endif
            return acquired;
        }
    }

    /// @brief Releases exclusive ownership of the mutex and wakes a waiting thread if `kMutexMayHaveWaitersBit` is set.
    /// @note Calling `unlock()` when the mutex is not locked triggers a debug assertion.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlock() noexcept
    {
        if consteval
        {
            const gp::UInt32 prev = m_state.exchange(0u, gp::MemoryOrder::Release);
            GP_ASSERT((prev & kMutexLockedBit) != 0u, "Mutex::unlock() called on an unlocked Mutex.");
        }
        else
        {
#if GP_DO_CHECK
            Fiber::adjustActiveNonYieldingLockCount(-1);
#endif
            const gp::UInt32 prev = m_state.exchange(0u, gp::MemoryOrder::Release);
            GP_ASSERT((prev & kMutexLockedBit) != 0u, "Mutex::unlock() called on an unlocked Mutex.");
            if ((prev & kMutexMayHaveWaitersBit) != 0u) [[unlikely]]
            {
                unlockSlow();
            }
        }
    }

    /// @brief Queries whether the mutex is currently held by any execution context.
    /// @note Intended for diagnostic assertions and telemetry; do not use `isLocked()` to gate lock acquisition.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `kMutexLockedBit` is currently set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLocked(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & kMutexLockedBit) != 0u;
    }

    /// @brief Queries whether one or more threads may be parked waiting on this mutex.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `kMutexMayHaveWaitersBit` is currently set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        mayHaveWaiters(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & kMutexMayHaveWaitersBit) != 0u;
    }

private:
    GP_COLD void lockSlow() noexcept;
    GP_COLD void unlockSlow() noexcept;

private:
    Atomic<gp::UInt32> m_state{ 0u };
};

static_assert(sizeof(Mutex) == 4u, "gp::thread::Mutex must be exactly 4 bytes.");
static_assert(alignof(Mutex) == 4u, "gp::thread::Mutex must be 4-byte aligned.");

/// @brief 64-byte cache-line-isolated wrapper around `gp::thread::Mutex` preventing MESI false sharing on hot global
///        locks ([ADR-0025], [ADR-0058]).
struct alignas(gp::memory::HardwareConstants::cacheLineSize) GP_THREAD_CAPABILITY("mutex") GP_CORE_API
    CacheAlignedMutex final
{
    /// @brief Underlying 4-byte mutex instance.
    Mutex mutex{};

    /// @brief Explicit padding isolating `mutex` onto its own 64-byte hardware cache line.
    gp::UInt8 padding[gp::memory::HardwareConstants::cacheLineSize - sizeof(Mutex)]{};

    /// @brief Constructs an unlocked cache-aligned mutex.
    [[nodiscard]] constexpr CacheAlignedMutex() noexcept = default;

    /// @brief Trivially destructible.
    constexpr ~CacheAlignedMutex() noexcept = default;

    CacheAlignedMutex(const CacheAlignedMutex&) = delete;
    CacheAlignedMutex& operator=(const CacheAlignedMutex&) = delete;
    CacheAlignedMutex(CacheAlignedMutex&&) = delete;
    CacheAlignedMutex& operator=(CacheAlignedMutex&&) = delete;

    /// @brief Acquires exclusive ownership of the underlying mutex.
    GP_ACQUIRE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void lock() noexcept
    {
        mutex.lock();
    }

    /// @brief Attempts to acquire exclusive ownership of the underlying mutex without blocking.
    /// @return True if acquired, false otherwise.
    [[nodiscard]] GP_TRY_ACQUIRE_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLock() noexcept
    {
        return mutex.tryLock();
    }

    /// @brief Releases exclusive ownership of the underlying mutex.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlock() noexcept
    {
        mutex.unlock();
    }

    /// @brief Queries whether the underlying mutex is currently locked.
    /// @param[in] order Memory ordering constraint.
    /// @return True if locked, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLocked(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.isLocked(order);
    }

    /// @brief Queries whether one or more threads may be parked waiting on the underlying mutex.
    /// @param[in] order Memory ordering constraint.
    /// @return True if `kMutexMayHaveWaitersBit` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        mayHaveWaiters(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.mayHaveWaiters(order);
    }
};

static_assert(
    sizeof(CacheAlignedMutex) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedMutex size must match the target CPU cache line size (64 bytes)."
);

}   // namespace gp::thread
