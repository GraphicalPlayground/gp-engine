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

/// @brief 8-byte writer-preferring reader-writer lock (`gp::thread::SharedMutex`) backed by inline atomic fast paths
///        and dual-channel futex parking ([ADR-0058]).
/// @details Modeled after Unreal Engine 5's `UE::FSharedMutex` and Decima's RWLock architecture:
///          - **8-Byte Compact Layout:** Consists of two 32-bit atomic words:
///            1. `m_state`: Packs `kSharedMutexWriterLockedBit` (`bit 31`), `kSharedMutexWriterWaitingBit` (`bit 30`),
///               `kSharedMutexReaderWaitingBit` (`bit 29`), and a 29-bit active shared reader count (`bits [0..28]`,
///               supporting up to 536,870,911 concurrent readers). Contending writers park directly on `m_state`.
///            2. `m_readerWakeEpoch`: Monotonic broadcast generation counter on which contending shared readers park
///               when a writer holds or awaits the lock, allowing a departing writer to wake all waiting readers in a
///               single `Futex::wakeAll()` call without waking writers prematurely.
///          - **Writer-Preferring Fairness:** Once a writer sets `kSharedMutexWriterWaitingBit`, new incoming readers
///            observe `(state & (kSharedMutexWriterLockedBit | kSharedMutexWriterWaitingBit)) != 0` and wait behind the
///            pending writer, preventing continuous reader streams from starving exclusive updates.
///          - **Atomic Reader-to-Writer Upgrade & Writer-to-Reader Downgrade:** Supports `tryUnlockSharedAndLock()`
///            (atomically converting a sole reader into an exclusive writer in 1 CAS without dropping the lock) and
///            `unlockAndLockShared()` (atomically downgrading an exclusive writer into a shared reader while waking
///            any waiting readers).
/// @note Non-recursive: a thread or fiber holding a shared lock must not call `lockShared()` or `lock()` again before
///       calling `unlockShared()` (if a writer sets `kSharedMutexWriterWaitingBit` in between, a recursive
///       `lockShared()` on the same thread would deadlock waiting for itself).
class GP_THREAD_CAPABILITY("mutex") GP_CORE_API SharedMutex final
{
public:
    /// @brief Constructs an unlocked `SharedMutex` (`0` readers, no writer).
    [[nodiscard]] constexpr SharedMutex() noexcept = default;

    /// @brief Trivially destructible; requires no OS kernel cleanup.
    constexpr ~SharedMutex() noexcept = default;

    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;
    SharedMutex(SharedMutex&&) = delete;
    SharedMutex& operator=(SharedMutex&&) = delete;

public:
    /// @section Exclusive (Writer) Locking

    /// @brief Acquires exclusive writer ownership, blocking until all active readers and prior writers release the
    ///        lock.
    GP_ACQUIRE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void lock() noexcept
    {
        gp::UInt32 expected = 0u;
        if (m_state.compareExchangeWeak(
                expected, kSharedMutexWriterLockedBit, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
            )) [[likely]]
        {
#if GP_DO_CHECK
            Fiber::adjustActiveNonYieldingLockCount(1);
#endif
            return;
        }
        lockSlow();
    }

    /// @brief Attempts to acquire exclusive writer ownership without blocking.
    /// @return True if exclusive ownership was acquired, false if held by any reader or writer.
    [[nodiscard]] GP_TRY_ACQUIRE_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLock() noexcept
    {
        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
        while ((expected & (kSharedMutexWriterLockedBit | kSharedMutexReaderCountMask)) == 0u)
        {
            if (m_state.compareExchangeWeak(
                    expected, expected | kSharedMutexWriterLockedBit, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
                ))
            {
                if !consteval
                {
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                }
                return true;
            }
        }
        return false;
    }

    /// @brief Releases exclusive writer ownership and wakes waiting writers or broadcasts to waiting readers.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlock() noexcept
    {
        if consteval
        {
            const gp::UInt32 prev = m_state.fetchAnd(~kSharedMutexWriterLockedBit, gp::MemoryOrder::Release);
            GP_ASSERT(
                (prev & kSharedMutexWriterLockedBit) != 0u,
                "SharedMutex::unlock() called when exclusive writer lock is not held."
            );
        }
        else
        {
#if GP_DO_CHECK
            Fiber::adjustActiveNonYieldingLockCount(-1);
#endif
            const gp::UInt32 prev = m_state.fetchAnd(~kSharedMutexWriterLockedBit, gp::MemoryOrder::Release);
            GP_ASSERT(
                (prev & kSharedMutexWriterLockedBit) != 0u,
                "SharedMutex::unlock() called when exclusive writer lock is not held."
            );
            if ((prev & (kSharedMutexWriterWaitingBit | kSharedMutexReaderWaitingBit)) != 0u) [[unlikely]]
            {
                unlockSlow();
            }
        }
    }

public:
    /// @section Shared (Reader) Locking

    /// @brief Acquires shared reader ownership, blocking if an exclusive writer holds or is waiting for the lock.
    GP_ACQUIRE_SHARED_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void lockShared() noexcept
    {
        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
        while ((expected & (kSharedMutexWriterLockedBit | kSharedMutexWriterWaitingBit)) == 0u) [[likely]]
        {
            GP_ASSERT(
                (expected & kSharedMutexReaderCountMask) < kSharedMutexMaxReaders, "SharedMutex reader count overflow."
            );
            if (m_state.compareExchangeWeak(
                    expected, expected + kSharedMutexOneReader, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
                )) [[likely]]
            {
#if GP_DO_CHECK
                Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                return;
            }
        }
        lockSharedSlow();
    }

    /// @brief Attempts to acquire shared reader ownership without blocking.
    /// @return True if shared reader ownership was acquired, false if a writer currently holds or awaits the lock.
    [[nodiscard]] GP_TRY_ACQUIRE_SHARED_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLockShared() noexcept
    {
        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
        while ((expected & (kSharedMutexWriterLockedBit | kSharedMutexWriterWaitingBit)) == 0u)
        {
            if ((expected & kSharedMutexReaderCountMask) >= kSharedMutexMaxReaders) [[unlikely]]
            {
                return false;
            }
            if (m_state.compareExchangeWeak(
                    expected, expected + kSharedMutexOneReader, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
                ))
            {
                if !consteval
                {
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                }
                return true;
            }
        }
        return false;
    }

    /// @brief Releases shared reader ownership and wakes a waiting writer if this was the final active reader.
    GP_RELEASE_SHARED_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void
        unlockShared() noexcept
    {
        if consteval
        {
            const gp::UInt32 prev = m_state.fetchSub(kSharedMutexOneReader, gp::MemoryOrder::Release);
            GP_ASSERT(
                (prev & kSharedMutexReaderCountMask) > 0u,
                "SharedMutex::unlockShared() called when no shared readers hold the lock."
            );
        }
        else
        {
#if GP_DO_CHECK
            Fiber::adjustActiveNonYieldingLockCount(-1);
#endif
            const gp::UInt32 prev = m_state.fetchSub(kSharedMutexOneReader, gp::MemoryOrder::Release);
            GP_ASSERT(
                (prev & kSharedMutexReaderCountMask) > 0u,
                "SharedMutex::unlockShared() called when no shared readers hold the lock."
            );
            const gp::UInt32 remainingReaders = (prev - kSharedMutexOneReader) & kSharedMutexReaderCountMask;
            if (remainingReaders == 0u && (prev & kSharedMutexWriterWaitingBit) != 0u) [[unlikely]]
            {
                unlockSharedSlow();
            }
        }
    }

public:
    /// @section Atomic Lock Upgrade & Downgrade

    /// @brief Attempts to atomically upgrade the calling reader's shared ownership into exclusive writer ownership
    ///        without releasing the lock in between ([ADR-0058]).
    /// @details Succeeds in a single atomic compare-and-exchange if and only if the calling thread is the sole active
    ///          reader (`getSharedReaderCount() == 1`). If other shared readers are concurrently active, returns
    ///          `false` and leaves the caller's shared lock intact.
    /// @return True if upgraded to exclusive writer ownership, false if other readers prevented atomic upgrade.
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr bool tryUnlockSharedAndLock() noexcept
    {
        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
        while ((expected & (kSharedMutexWriterLockedBit | kSharedMutexReaderCountMask)) == kSharedMutexOneReader)
        {
            const gp::UInt32 desired = (expected - kSharedMutexOneReader) | kSharedMutexWriterLockedBit;
            if (m_state.compareExchangeWeak(expected, desired, gp::MemoryOrder::AcqRel, gp::MemoryOrder::Relaxed))
            {
                return true;
            }
        }
        return false;
    }

    /// @brief Atomically downgrades exclusive writer ownership into shared reader ownership (`1` active reader) and
    ///        wakes any waiting readers if no writer is pending ([ADR-0058]).
    /// @details Guarantees that no waiting writer can barge in between releasing the exclusive lock and acquiring the
    ///          shared read lock.
    GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlockAndLockShared() noexcept
    {
        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
        for (;;)
        {
            GP_ASSERT(
                (expected & kSharedMutexWriterLockedBit) != 0u,
                "SharedMutex::unlockAndLockShared() called when exclusive writer lock is not held."
            );
            const gp::UInt32 desired = (expected & ~kSharedMutexWriterLockedBit) + kSharedMutexOneReader;
            if (m_state.compareExchangeWeak(expected, desired, gp::MemoryOrder::AcqRel, gp::MemoryOrder::Relaxed))
            {
                if !consteval
                {
                    if ((expected & kSharedMutexReaderWaitingBit) != 0u &&
                        (expected & kSharedMutexWriterWaitingBit) == 0u) [[unlikely]]
                    {
                        wakeWaitingReadersSlow();
                    }
                }
                return;
            }
        }
    }

public:
    /// @section State & Telemetry Queries

    /// @brief Queries whether an exclusive writer currently holds the lock.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `kSharedMutexWriterLockedBit` is set, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLockedExclusive(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & kSharedMutexWriterLockedBit) != 0u;
    }

    /// @brief Queries whether one or more shared readers currently hold the lock.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if `getSharedReaderCount(order) > 0u`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLockedShared(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return getSharedReaderCount(order) > 0u;
    }

    /// @brief Queries whether the mutex is currently held in either exclusive or shared mode.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if held by a writer or at least one reader.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLocked(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & (kSharedMutexWriterLockedBit | kSharedMutexReaderCountMask)) != 0u;
    }

    /// @brief Queries the number of active shared readers currently holding the lock.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return Active 29-bit reader count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
        getSharedReaderCount(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return m_state.load(order) & kSharedMutexReaderCountMask;
    }

private:
    GP_COLD void lockSlow() noexcept;
    GP_COLD void unlockSlow() noexcept;
    GP_COLD void lockSharedSlow() noexcept;
    GP_COLD void unlockSharedSlow() noexcept;
    GP_COLD void wakeWaitingReadersSlow() noexcept;

private:
    Atomic<gp::UInt32> m_state{ 0u };
    Atomic<gp::UInt32> m_readerWakeEpoch{ 0u };
};

static_assert(sizeof(SharedMutex) == 8u, "gp::thread::SharedMutex must be exactly 8 bytes.");
static_assert(alignof(SharedMutex) == 4u, "gp::thread::SharedMutex must be 4-byte aligned.");

/// @brief 64-byte cache-line-isolated wrapper around `gp::thread::SharedMutex` preventing MESI false sharing on hot
///        reader-writer registries ([ADR-0025], [ADR-0058]).
struct alignas(gp::memory::HardwareConstants::cacheLineSize) GP_THREAD_CAPABILITY("mutex") GP_CORE_API
    CacheAlignedSharedMutex final
{
    /// @brief Underlying 8-byte shared mutex instance.
    SharedMutex mutex{};

    /// @brief Explicit padding isolating `mutex` onto a dedicated 64-byte CPU cache line.
    gp::UInt8 padding[gp::memory::HardwareConstants::cacheLineSize - sizeof(SharedMutex)]{};

    /// @brief Constructs an unlocked cache-aligned shared mutex.
    [[nodiscard]] constexpr CacheAlignedSharedMutex() noexcept = default;

    /// @brief Trivially destructible.
    constexpr ~CacheAlignedSharedMutex() noexcept = default;

    CacheAlignedSharedMutex(const CacheAlignedSharedMutex&) = delete;
    CacheAlignedSharedMutex& operator=(const CacheAlignedSharedMutex&) = delete;
    CacheAlignedSharedMutex(CacheAlignedSharedMutex&&) = delete;
    CacheAlignedSharedMutex& operator=(CacheAlignedSharedMutex&&) = delete;

    /// @brief Acquires exclusive writer ownership.
    GP_ACQUIRE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void lock() noexcept
    {
        mutex.lock();
    }

    /// @brief Attempts to acquire exclusive writer ownership without blocking.
    /// @return True if acquired, false otherwise.
    [[nodiscard]] GP_TRY_ACQUIRE_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLock() noexcept
    {
        return mutex.tryLock();
    }

    /// @brief Releases exclusive writer ownership.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlock() noexcept
    {
        mutex.unlock();
    }

    /// @brief Acquires shared reader ownership.
    GP_ACQUIRE_SHARED_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void lockShared() noexcept
    {
        mutex.lockShared();
    }

    /// @brief Attempts to acquire shared reader ownership without blocking.
    /// @return True if acquired, false otherwise.
    [[nodiscard]] GP_TRY_ACQUIRE_SHARED_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLockShared() noexcept
    {
        return mutex.tryLockShared();
    }

    /// @brief Releases shared reader ownership.
    GP_RELEASE_SHARED_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void
        unlockShared() noexcept
    {
        mutex.unlockShared();
    }

    /// @brief Attempts to atomically upgrade sole shared reader ownership to exclusive writer ownership.
    /// @return True if upgraded, false otherwise.
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr bool tryUnlockSharedAndLock() noexcept
    {
        return mutex.tryUnlockSharedAndLock();
    }

    /// @brief Atomically downgrades exclusive writer ownership to shared reader ownership.
    GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlockAndLockShared() noexcept
    {
        mutex.unlockAndLockShared();
    }

    /// @brief Queries whether an exclusive writer currently holds the lock.
    /// @param[in] order Memory ordering constraint.
    /// @return True if locked exclusively.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLockedExclusive(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.isLockedExclusive(order);
    }

    /// @brief Queries whether one or more shared readers currently hold the lock.
    /// @param[in] order Memory ordering constraint.
    /// @return True if locked in shared mode.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLockedShared(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.isLockedShared(order);
    }

    /// @brief Queries whether the mutex is currently held in either exclusive or shared mode.
    /// @param[in] order Memory ordering constraint.
    /// @return True if held by a writer or at least one reader.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLocked(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.isLocked(order);
    }

    /// @brief Queries the number of active shared readers.
    /// @param[in] order Memory ordering constraint.
    /// @return Active shared reader count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
        getSharedReaderCount(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.getSharedReaderCount(order);
    }
};

static_assert(
    sizeof(CacheAlignedSharedMutex) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedSharedMutex size must match the target CPU cache line size (64 bytes)."
);

}   // namespace gp::thread
