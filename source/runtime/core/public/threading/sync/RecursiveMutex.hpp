// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/MutexForward.hpp"   // IWYU pragma: export
#include "threading/sync/MutexTypes.hpp"     // IWYU pragma: export

namespace gp::thread
{

/// @brief 16-byte re-entrant mutual exclusion lock (`gp::thread::CriticalSection`) with dual-domain cooperative
///        `FiberId` / OS `ThreadId` ownership tracking ([ADR-0049], [ADR-0058]).
/// @details Solves the classic cooperative fiber hazard present in OS `CRITICAL_SECTION` and `pthread_mutex_t`
///          wrappers:
///          - **Fiber-Aware Execution Owner Token:** Traditional recursive mutexes key ownership solely to the OS
///            `ThreadId`. In an $M:N$ fiber scheduler ([ADR-0049]), multiple fibers multiplex onto the same OS worker
///            thread, so a thread-keyed `CRITICAL_SECTION` erroneously allows Fiber B to re-enter a lock held by
///            Fiber A on the same worker thread. `RecursiveMutex::currentExecutionOwnerToken()` returns
///            `(kRecursiveMutexFiberOwnerBit | currentFiberId)` when executing inside a child `gp::thread::Fiber` and
///            `Thread::currentThreadId()` when executing on a raw OS thread or `ThreadAnchor`.
///          - **Fast Re-Entrant Acquisition:** If the calling execution context already owns the lock
///            (`m_ownerToken == callerToken`), `lock()` and `tryLock()` increment `m_recursionDepth` in user space
///            without atomic RMW instructions.
///          - **16-Byte Compact Layout:** Fits in 16 bytes (`8B` atomic owner token + `4B` futex lock state + `4B`
///            recursion depth), $2.5\times$ smaller than Win32 `CRITICAL_SECTION` (`40B`) and $4\times$ smaller than
///            glibc `pthread_mutex_t` (`64B`).
class GP_THREAD_CAPABILITY("mutex") GP_CORE_API RecursiveMutex final
{
public:
    /// @brief Constructs an unlocked `RecursiveMutex` (`getRecursionDepth() == 0u`).
    [[nodiscard]] constexpr RecursiveMutex() noexcept = default;

    /// @brief Trivially destructible; requires no OS kernel cleanup.
    constexpr ~RecursiveMutex() noexcept = default;

    RecursiveMutex(const RecursiveMutex&) = delete;
    RecursiveMutex& operator=(const RecursiveMutex&) = delete;
    RecursiveMutex(RecursiveMutex&&) = delete;
    RecursiveMutex& operator=(RecursiveMutex&&) = delete;

public:
    /// @brief Acquires ownership of the recursive mutex, or increments its recursion depth if already owned by the
    ///        calling fiber or OS thread.
    GP_ACQUIRE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS void lock() noexcept;

    /// @brief Attempts to acquire ownership of the recursive mutex without blocking.
    /// @return True if acquired (or recursively re-entered by the current owner), false if held by another execution
    ///         context.
    [[nodiscard]] GP_TRY_ACQUIRE_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS bool tryLock() noexcept;

    /// @brief Decrements the recursion depth by `1` and releases the underlying lock when depth reaches `0`.
    /// @note Triggers a debug assertion if called by an execution context that does not currently own the lock.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS void unlock() noexcept;

public:
    /// @brief Queries whether this mutex is currently held by any execution context.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return True if locked, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLocked(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return (m_state.load(order) & kMutexLockedBit) != 0u;
    }

    /// @brief Checks whether the calling execution context (active child `Fiber` or OS `Thread`) currently owns this
    ///        recursive mutex.
    /// @return True if `m_ownerToken == currentExecutionOwnerToken()`.
    [[nodiscard]] bool isOwnedByCurrentContext() const noexcept;

    /// @brief Queries the current recursion depth (`0` when unlocked, $\ge 1$ when locked).
    /// @note Only synchronized when called by the current lock owner or for diagnostic inspection.
    /// @return Current recursion count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getRecursionDepth() const noexcept
    {
        return m_recursionDepth;
    }

    /// @brief Queries the raw 64-bit owner token (`0` when unlocked).
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return 64-bit owner token (`kRecursiveMutexFiberOwnerBit | FiberId` or OS `ThreadId`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
        getOwnerToken(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return m_ownerToken.load(order);
    }

    /// @brief Computes the non-zero 64-bit execution owner token for the calling child `Fiber` or OS `Thread`.
    /// @return Unique 64-bit token identifying the active child fiber (bit 63 set) or OS hardware thread (bit 63
    /// clear).
    [[nodiscard]] static gp::UInt64 currentExecutionOwnerToken() noexcept;

private:
    GP_COLD void lockContended(gp::UInt64 callerToken) noexcept;

private:
    Atomic<gp::UInt64> m_ownerToken{ kInvalidExecutionOwnerToken };
    Atomic<gp::UInt32> m_state{ 0u };
    gp::UInt32 m_recursionDepth{ 0u };
};

static_assert(sizeof(RecursiveMutex) == 16u, "gp::thread::RecursiveMutex must be exactly 16 bytes.");
static_assert(alignof(RecursiveMutex) == 8u, "gp::thread::RecursiveMutex must be 8-byte aligned.");

/// @brief 64-byte cache-line-isolated wrapper around `gp::thread::RecursiveMutex` (`CacheAlignedCriticalSection`)
///        preventing MESI false sharing ([ADR-0025], [ADR-0058]).
struct alignas(gp::memory::HardwareConstants::cacheLineSize) GP_THREAD_CAPABILITY("mutex") GP_CORE_API
    CacheAlignedRecursiveMutex final
{
    /// @brief Underlying 16-byte recursive mutex instance.
    RecursiveMutex mutex{};

    /// @brief Explicit padding isolating `mutex` onto a dedicated 64-byte cache line.
    gp::UInt8 padding[gp::memory::HardwareConstants::cacheLineSize - sizeof(RecursiveMutex)]{};

    /// @brief Constructs an unlocked cache-aligned recursive mutex.
    [[nodiscard]] constexpr CacheAlignedRecursiveMutex() noexcept = default;

    /// @brief Trivially destructible.
    constexpr ~CacheAlignedRecursiveMutex() noexcept = default;

    CacheAlignedRecursiveMutex(const CacheAlignedRecursiveMutex&) = delete;
    CacheAlignedRecursiveMutex& operator=(const CacheAlignedRecursiveMutex&) = delete;
    CacheAlignedRecursiveMutex(CacheAlignedRecursiveMutex&&) = delete;
    CacheAlignedRecursiveMutex& operator=(CacheAlignedRecursiveMutex&&) = delete;

    /// @brief Acquires or re-enters the underlying recursive mutex.
    GP_ACQUIRE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void lock() noexcept
    {
        mutex.lock();
    }

    /// @brief Attempts to acquire or re-enter the underlying recursive mutex without blocking.
    /// @return True if acquired, false otherwise.
    [[nodiscard]] GP_TRY_ACQUIRE_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        bool tryLock() noexcept
    {
        return mutex.tryLock();
    }

    /// @brief Decrements recursion depth and unlocks when depth reaches zero.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT void unlock() noexcept
    {
        mutex.unlock();
    }

    /// @brief Queries whether the underlying recursive mutex is currently locked.
    /// @param[in] order Memory ordering constraint.
    /// @return True if locked, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isLocked(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.isLocked(order);
    }

    /// @brief Checks whether the calling execution context currently owns the underlying recursive mutex.
    /// @return True if owned by the calling fiber or thread.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isOwnedByCurrentContext() const noexcept
    {
        return mutex.isOwnedByCurrentContext();
    }

    /// @brief Queries the current recursion depth of the underlying recursive mutex.
    /// @return Current recursion count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getRecursionDepth() const noexcept
    {
        return mutex.getRecursionDepth();
    }

    /// @brief Queries the raw 64-bit owner token of the underlying recursive mutex.
    /// @param[in] order Memory ordering constraint.
    /// @return 64-bit owner token (`0` when unlocked).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
        getOwnerToken(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return mutex.getOwnerToken(order);
    }
};

static_assert(
    sizeof(CacheAlignedRecursiveMutex) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedRecursiveMutex size must match the target CPU cache line size (64 bytes)."
);

}   // namespace gp::thread
