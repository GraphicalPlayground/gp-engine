// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/MutexForward.hpp"   // IWYU pragma: export
#include "threading/sync/MutexTypes.hpp"     // IWYU pragma: export

namespace gp::thread
{

/// @brief Concept-constrained RAII exclusive lock guard (`gp::thread::UniqueLock`, aliased as
///        `gp::thread::ScopedLock`) ([ADR-0058]).
/// @details Manages exclusive ownership of any mutex satisfying `gp::concepts::IsBasicLockable` (`Mutex`,
///          `CacheAlignedMutex`, `RecursiveMutex`, `CriticalSection`, `SharedMutex`, etc.). Supports immediate
///          locking, deferred locking (`DeferLock`), non-blocking tentative locking (`TryToLock`), ownership adoption
///          (`AdoptLock`), move transfers, and explicit `lock()` / `tryLock()` / `unlock()` / `release()` control.
/// @tparam InMutexType Mutex type satisfying `gp::concepts::IsBasicLockable` (defaults to `gp::thread::Mutex`).
template <typename InMutexType>
class [[nodiscard]] GP_SCOPED_THREAD_CAPABILITY UniqueLock final
{
    static_assert(
        gp::concepts::IsBasicLockable<InMutexType>,
        "gp::thread::UniqueLock requires InMutexType to satisfy gp::concepts::IsBasicLockable."
    );

public:
    /// @brief Underlying mutex type managed by this guard.
    using MutexType = InMutexType;

public:
    /// @brief Constructs an empty `UniqueLock` not associated with any mutex (`ownsLock() == false`).
    [[nodiscard]] constexpr UniqueLock() noexcept = default;

    /// @brief Associates with `mutex` and immediately acquires exclusive ownership via `mutex.lock()`.
    /// @param[in,out] mutex Target mutex to lock for the lifetime of this guard.
    [[nodiscard]] explicit constexpr UniqueLock(InMutexType& mutex) noexcept GP_ACQUIRE_CAPABILITY(
        mutex
    ) GP_NO_THREAD_SAFETY_ANALYSIS
        : m_mutex(&mutex)
        , m_ownsLock(true)
    {
        m_mutex->lock();
    }

    /// @brief Associates with `mutex` without acquiring it (`ownsLock() == false`).
    /// @param[in,out] mutex Target mutex to bind without locking.
    /// @param[in] tag `DeferLock` tag selector.
    [[nodiscard]] constexpr UniqueLock(InMutexType& mutex, [[maybe_unused]] const DeferLockT tag) noexcept
        : m_mutex(&mutex)
        , m_ownsLock(false)
    {}

    /// @brief Associates with `mutex` and attempts to acquire exclusive ownership without blocking via
    ///        `mutex.tryLock()`.
    /// @param[in,out] mutex Target mutex on which to attempt non-blocking acquisition.
    /// @param[in] tag `TryToLock` tag selector.
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr UniqueLock(
        InMutexType& mutex, [[maybe_unused]] const TryToLockT tag
    ) noexcept requires gp::concepts::IsLockable<InMutexType>
        : m_mutex(&mutex)
        , m_ownsLock(mutex.tryLock())
    {}

    /// @brief Associates with `mutex` and adopts existing exclusive ownership already held by the caller.
    /// @param[in,out] mutex Target mutex already locked by the calling execution context.
    /// @param[in] tag `AdoptLock` tag selector.
    [[nodiscard]] constexpr UniqueLock(
        InMutexType& mutex, [[maybe_unused]] const AdoptLockT tag
    ) noexcept GP_REQUIRES_CAPABILITY(mutex) GP_NO_THREAD_SAFETY_ANALYSIS
        : m_mutex(&mutex)
        , m_ownsLock(true)
    {}

    /// @brief Releases the associated mutex via `unlock()` if `ownsLock() == true`.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS constexpr ~UniqueLock() noexcept
    {
        if (m_ownsLock && m_mutex != nullptr)
        {
            m_mutex->unlock();
        }
    }

    UniqueLock(const UniqueLock&) = delete;
    UniqueLock& operator=(const UniqueLock&) = delete;

    /// @brief Move constructor transferring mutex association and lock ownership from `other`.
    /// @param[in,out] other Source `UniqueLock` (left disassociated with `ownsLock() == false`).
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr UniqueLock(UniqueLock&& other) noexcept
        : m_mutex(other.m_mutex)
        , m_ownsLock(other.m_ownsLock)
    {
        other.m_mutex = nullptr;
        other.m_ownsLock = false;
    }

    /// @brief Move assignment operator releasing any currently owned lock before transferring ownership from `other`.
    /// @param[in,out] other Source `UniqueLock` (left disassociated with `ownsLock() == false`).
    /// @return Reference to this `UniqueLock`.
    GP_NO_THREAD_SAFETY_ANALYSIS constexpr UniqueLock& operator=(UniqueLock&& other) noexcept
    {
        if (this != &other)
        {
            if (m_ownsLock && m_mutex != nullptr)
            {
                m_mutex->unlock();
            }
            m_mutex = other.m_mutex;
            m_ownsLock = other.m_ownsLock;
            other.m_mutex = nullptr;
            other.m_ownsLock = false;
        }
        return *this;
    }

public:
    /// @brief Acquires exclusive ownership of the associated mutex.
    /// @note Triggers a debug assertion if no mutex is associated or if this guard already owns the lock.
    GP_ACQUIRE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void lock() noexcept
    {
        GP_ASSERT(m_mutex != nullptr, "UniqueLock::lock() called with a null mutex pointer.");
        GP_ASSERT(!m_ownsLock, "UniqueLock::lock() called when the lock is already owned by this guard.");
        if (m_mutex != nullptr && !m_ownsLock)
        {
            m_mutex->lock();
            m_ownsLock = true;
        }
    }

    /// @brief Attempts to acquire exclusive ownership of the associated mutex without blocking.
    /// @note Triggers a debug assertion if no mutex is associated or if this guard already owns the lock.
    /// @return True if the lock was acquired, false otherwise.
    [[nodiscard]] GP_TRY_ACQUIRE_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLock() noexcept requires gp::concepts::IsLockable<InMutexType>
    {
        GP_ASSERT(m_mutex != nullptr, "UniqueLock::tryLock() called with a null mutex pointer.");
        GP_ASSERT(!m_ownsLock, "UniqueLock::tryLock() called when the lock is already owned by this guard.");
        if (m_mutex == nullptr || m_ownsLock)
        {
            return false;
        }
        m_ownsLock = m_mutex->tryLock();
        return m_ownsLock;
    }

    /// @brief Releases exclusive ownership of the associated mutex.
    /// @note Triggers a debug assertion if this guard does not currently own the lock.
    GP_RELEASE_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlock() noexcept
    {
        GP_ASSERT(m_mutex != nullptr && m_ownsLock, "UniqueLock::unlock() called when lock is not owned.");
        if (m_mutex != nullptr && m_ownsLock)
        {
            m_mutex->unlock();
            m_ownsLock = false;
        }
    }

    /// @brief Exchanges mutex association and ownership state with `other`.
    /// @param[in,out] other Guard to swap state with.
    GP_NO_THREAD_SAFETY_ANALYSIS constexpr void swap(UniqueLock& other) noexcept
    {
        InMutexType* const tempMutex = m_mutex;
        const bool tempOwns = m_ownsLock;
        m_mutex = other.m_mutex;
        m_ownsLock = other.m_ownsLock;
        other.m_mutex = tempMutex;
        other.m_ownsLock = tempOwns;
    }

    /// @brief Disassociates this guard from its mutex without unlocking it and returns the previously bound pointer.
    /// @return Pointer to the previously associated mutex, or `nullptr`.
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr InMutexType* release() noexcept
    {
        InMutexType* const prevMutex = m_mutex;
        m_mutex = nullptr;
        m_ownsLock = false;
        return prevMutex;
    }

    /// @brief Checks whether this guard currently holds exclusive ownership of its associated mutex.
    /// @return True if `m_ownsLock == true`, false otherwise.
    [[nodiscard]] constexpr bool ownsLock() const noexcept
    {
        return m_ownsLock;
    }

    /// @brief Contextual boolean conversion returning `ownsLock()`.
    /// @return True if this guard currently holds the lock, false otherwise.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return m_ownsLock;
    }

    /// @brief Retrieves a pointer to the associated mutex, or `nullptr` if disassociated.
    /// @return Pointer to `InMutexType`.
    [[nodiscard]] constexpr InMutexType* mutex() const noexcept
    {
        return m_mutex;
    }

private:
    InMutexType* m_mutex{ nullptr };
    bool m_ownsLock{ false };
};

/// @brief Attempts to lock two or more `IsLockable` mutexes without deadlock using a smart-and-backoff try-lock
///        protocol ([ADR-0058]).
/// @details Returns `-1` if all mutexes were locked; otherwise unlocks any mutexes acquired so far and returns the
///          0-based index of the mutex whose `tryLock()` failed.
/// @tparam InMutex1 First lockable mutex type.
/// @tparam InMutex2 Second lockable mutex type.
/// @tparam InMutexes Remaining lockable mutex types.
/// @param[in,out] m1 First mutex to try-lock.
/// @param[in,out] m2 Second mutex to try-lock.
/// @param[in,out] rest Remaining mutexes to try-lock.
/// @return `-1` on complete acquisition, or the 0-based index of the failed mutex.
template <typename InMutex1, typename InMutex2, typename... InMutexes>
requires gp::concepts::IsLockable<InMutex1> && gp::concepts::IsLockable<InMutex2> &&
         (gp::concepts::IsLockable<InMutexes> && ...) [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr gp::Int32
    tryLockMultiple(InMutex1& m1, InMutex2& m2, InMutexes&... rest) noexcept
{
    UniqueLock<InMutex1> firstLock(m1, TryToLock);
    if (!firstLock.ownsLock())
    {
        return 0;
    }

    if constexpr (sizeof...(InMutexes) == 0u)
    {
        if (!m2.tryLock())
        {
            return 1;
        }
        (void)firstLock.release();
        return -1;
    }
    else
    {
        const gp::Int32 failedSubIndex = tryLockMultiple(m2, rest...);
        if (failedSubIndex != -1)
        {
            return failedSubIndex + 1;
        }
        (void)firstLock.release();
        return -1;
    }
}

/// @brief Acquires exclusive ownership of two or more `IsLockable` mutexes simultaneously without AB/BA deadlocks
///        ([ADR-0058]).
/// @details For two mutexes of the same type (`&m1 != &m2`), locks in canonical memory-address order (`&m1 < &m2`) for
///          deterministic single-pass acquisition with zero backoff retries. For heterogeneous mutex sets or 3+
///          mutexes, uses a persistent lock-first-then-try-rest rotation loop.
/// @tparam InMutex1 First lockable mutex type.
/// @tparam InMutex2 Second lockable mutex type.
/// @tparam InMutexes Remaining lockable mutex types.
/// @param[in,out] m1 First mutex to lock.
/// @param[in,out] m2 Second mutex to lock.
/// @param[in,out] rest Remaining mutexes to lock.
template <typename InMutex1, typename InMutex2, typename... InMutexes>
requires gp::concepts::IsLockable<InMutex1> && gp::concepts::IsLockable<InMutex2> &&
         (gp::concepts::IsLockable<InMutexes> && ...)
GP_NO_THREAD_SAFETY_ANALYSIS inline void lockMultiple(InMutex1& m1, InMutex2& m2, InMutexes&... rest) noexcept
{
    if constexpr (sizeof...(InMutexes) == 0u && gp::concepts::IsSameAs<InMutex1, InMutex2>)
    {
        GP_ASSERT(&m1 != &m2, "lockMultiple() called with two references to the exact same mutex instance.");
        if (reinterpret_cast<gp::UIntPtr>(&m1) < reinterpret_cast<gp::UIntPtr>(&m2))
        {
            m1.lock();
            m2.lock();
        }
        else
        {
            m2.lock();
            m1.lock();
        }
    }
    else
    {
        for (;;)
        {
            UniqueLock<InMutex1> firstLock(m1);
            if constexpr (sizeof...(InMutexes) == 0u)
            {
                if (m2.tryLock())
                {
                    (void)firstLock.release();
                    return;
                }
            }
            else
            {
                if (tryLockMultiple(m2, rest...) == -1)
                {
                    (void)firstLock.release();
                    return;
                }
            }
            firstLock.unlock();

            UniqueLock<InMutex2> secondLock(m2);
            if constexpr (sizeof...(InMutexes) == 0u)
            {
                if (m1.tryLock())
                {
                    (void)secondLock.release();
                    return;
                }
            }
            else
            {
                if (tryLockMultiple(rest..., m1) == -1)
                {
                    (void)secondLock.release();
                    return;
                }
            }
        }
    }
}

}   // namespace gp::thread

namespace gp
{

using thread::lockMultiple;
using thread::tryLockMultiple;

}   // namespace gp
