// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "threading/sync/MutexForward.hpp"   // IWYU pragma: export
#include "threading/sync/MutexTypes.hpp"     // IWYU pragma: export
#include "threading/sync/SharedMutex.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::thread
{

/// @brief Concept-constrained RAII shared reader lock guard (`gp::thread::SharedLock`) ([ADR-0058]).
/// @details Manages shared reader ownership of any reader-writer mutex satisfying `gp::concepts::IsSharedLockable`
///          (`SharedMutex`, `CacheAlignedSharedMutex`). Supports immediate shared locking (`lockShared()`), deferred
///          locking (`DeferLock`), non-blocking tentative shared locking (`TryToLock`), ownership adoption
///          (`AdoptLock`), writer-to-reader downgrade construction (`SharedLock(UniqueLock&&)`), move semantics, and
///          explicit `lock()` / `tryLock()` / `unlock()` / `release()` control.
/// @tparam InSharedMutexType Shared mutex type satisfying `gp::concepts::IsSharedLockable` (defaults to
///         `gp::thread::SharedMutex`).
template <typename InSharedMutexType>
class [[nodiscard]] GP_SCOPED_THREAD_CAPABILITY SharedLock final
{
    static_assert(
        gp::concepts::IsSharedLockable<InSharedMutexType>,
        "gp::thread::SharedLock requires InSharedMutexType to satisfy gp::concepts::IsSharedLockable."
    );

public:
    /// @brief Underlying shared mutex type managed by this guard.
    using MutexType = InSharedMutexType;

public:
    /// @brief Constructs an empty `SharedLock` not associated with any mutex (`ownsLock() == false`).
    [[nodiscard]] constexpr SharedLock() noexcept = default;

    /// @brief Associates with `mutex` and immediately acquires shared reader ownership via `mutex.lockShared()`.
    /// @param[in,out] mutex Target shared mutex to lock in shared mode for the lifetime of this guard.
    [[nodiscard]] explicit constexpr SharedLock(InSharedMutexType& mutex) noexcept GP_ACQUIRE_SHARED_CAPABILITY(
        mutex
    ) GP_NO_THREAD_SAFETY_ANALYSIS
        : m_mutex(&mutex)
        , m_ownsLock(true)
    {
        m_mutex->lockShared();
    }

    /// @brief Associates with `mutex` without acquiring it (`ownsLock() == false`).
    /// @param[in,out] mutex Target shared mutex to bind without locking.
    /// @param[in] tag `DeferLock` tag selector.
    [[nodiscard]] constexpr SharedLock(InSharedMutexType& mutex, [[maybe_unused]] const DeferLockT tag) noexcept
        : m_mutex(&mutex)
        , m_ownsLock(false)
    {}

    /// @brief Associates with `mutex` and attempts to acquire shared reader ownership without blocking via
    ///        `mutex.tryLockShared()`.
    /// @param[in,out] mutex Target shared mutex on which to attempt non-blocking shared acquisition.
    /// @param[in] tag `TryToLock` tag selector.
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr SharedLock(
        InSharedMutexType& mutex, [[maybe_unused]] const TryToLockT tag
    ) noexcept
        : m_mutex(&mutex)
        , m_ownsLock(mutex.tryLockShared())
    {}

    /// @brief Associates with `mutex` and adopts existing shared reader ownership already held by the caller.
    /// @param[in,out] mutex Target shared mutex already locked in shared mode by the calling execution context.
    /// @param[in] tag `AdoptLock` tag selector.
    [[nodiscard]] constexpr SharedLock(
        InSharedMutexType& mutex, [[maybe_unused]] const AdoptLockT tag
    ) noexcept GP_REQUIRES_SHARED_CAPABILITY(mutex) GP_NO_THREAD_SAFETY_ANALYSIS
        : m_mutex(&mutex)
        , m_ownsLock(true)
    {}

    /// @brief Atomically downgrades an exclusive `UniqueLock<InSharedMutexType>` into a `SharedLock` via
    ///        `unlockAndLockShared()` without allowing waiting writers to barge in ([ADR-0058]).
    /// @param[in,out] exclusiveLock Rvalue `UniqueLock` to downgrade (left disassociated with `ownsLock() == false`).
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS explicit constexpr SharedLock(
        UniqueLock<InSharedMutexType>&& exclusiveLock
    ) noexcept requires requires(InSharedMutexType& m) {
        {
            m.unlockAndLockShared()
        } noexcept;
    }
        : m_mutex(exclusiveLock.mutex())
        , m_ownsLock(exclusiveLock.ownsLock())
    {
        if (m_ownsLock && m_mutex != nullptr)
        {
            m_mutex->unlockAndLockShared();
        }
        (void)exclusiveLock.release();
    }

    /// @brief Releases shared reader ownership via `unlockShared()` if `ownsLock() == true`.
    GP_RELEASE_SHARED_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS constexpr ~SharedLock() noexcept
    {
        if (m_ownsLock && m_mutex != nullptr)
        {
            m_mutex->unlockShared();
        }
    }

    SharedLock(const SharedLock&) = delete;
    SharedLock& operator=(const SharedLock&) = delete;

    /// @brief Move constructor transferring shared mutex association and ownership from `other`.
    /// @param[in,out] other Source `SharedLock` (left disassociated with `ownsLock() == false`).
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr SharedLock(SharedLock&& other) noexcept
        : m_mutex(other.m_mutex)
        , m_ownsLock(other.m_ownsLock)
    {
        other.m_mutex = nullptr;
        other.m_ownsLock = false;
    }

    /// @brief Move assignment operator releasing any currently owned shared lock before transferring ownership from
    ///        `other`.
    /// @param[in,out] other Source `SharedLock` (left disassociated with `ownsLock() == false`).
    /// @return Reference to this `SharedLock`.
    GP_NO_THREAD_SAFETY_ANALYSIS constexpr SharedLock& operator=(SharedLock&& other) noexcept
    {
        if (this != &other)
        {
            if (m_ownsLock && m_mutex != nullptr)
            {
                m_mutex->unlockShared();
            }
            m_mutex = other.m_mutex;
            m_ownsLock = other.m_ownsLock;
            other.m_mutex = nullptr;
            other.m_ownsLock = false;
        }
        return *this;
    }

public:
    /// @brief Acquires shared reader ownership of the associated mutex via `lockShared()`.
    /// @note Triggers a debug assertion if no mutex is associated or if this guard already owns the lock.
    GP_ACQUIRE_SHARED_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void lock() noexcept
    {
        GP_ASSERT(m_mutex != nullptr, "SharedLock::lock() called with a null mutex pointer.");
        GP_ASSERT(!m_ownsLock, "SharedLock::lock() called when the shared lock is already owned by this guard.");
        if (m_mutex != nullptr && !m_ownsLock)
        {
            m_mutex->lockShared();
            m_ownsLock = true;
        }
    }

    /// @brief Attempts to acquire shared reader ownership of the associated mutex without blocking via
    ///        `tryLockShared()`.
    /// @note Triggers a debug assertion if no mutex is associated or if this guard already owns the lock.
    /// @return True if shared reader ownership was acquired, false otherwise.
    [[nodiscard]] GP_TRY_ACQUIRE_SHARED_CAPABILITY(true) GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT
        constexpr bool tryLock() noexcept
    {
        GP_ASSERT(m_mutex != nullptr, "SharedLock::tryLock() called with a null mutex pointer.");
        GP_ASSERT(!m_ownsLock, "SharedLock::tryLock() called when the shared lock is already owned by this guard.");
        if (m_mutex == nullptr || m_ownsLock)
        {
            return false;
        }
        m_ownsLock = m_mutex->tryLockShared();
        return m_ownsLock;
    }

    /// @brief Releases shared reader ownership of the associated mutex via `unlockShared()`.
    /// @note Triggers a debug assertion if this guard does not currently own a shared lock.
    GP_RELEASE_SHARED_CAPABILITY() GP_NO_THREAD_SAFETY_ANALYSIS GP_FORCEINLINE_HINT constexpr void unlock() noexcept
    {
        GP_ASSERT(m_mutex != nullptr && m_ownsLock, "SharedLock::unlock() called when shared lock is not owned.");
        if (m_mutex != nullptr && m_ownsLock)
        {
            m_mutex->unlockShared();
            m_ownsLock = false;
        }
    }

    /// @brief Attempts to atomically upgrade this shared reader guard into an exclusive `UniqueLock` via
    ///        `tryUnlockSharedAndLock()` ([ADR-0058]).
    /// @details If the underlying shared mutex upgrades in place (`getSharedReaderCount() == 1`), transfers ownership
    /// to
    ///          the returned `UniqueLock` (`ownsLock() == true`) and disassociates this `SharedLock`. If other readers
    ///          are active, returns an empty `UniqueLock` (`ownsLock() == false`) and leaves this `SharedLock` intact.
    /// @return Engaged `UniqueLock<InSharedMutexType>` on upgrade success, or empty `UniqueLock` on failure.
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr UniqueLock<InSharedMutexType> tryUpgradeToUnique() noexcept
        requires requires(InSharedMutexType& m) {
            {
                m.tryUnlockSharedAndLock()
            } noexcept -> gp::concepts::IsSameAs<bool>;
        }
    {
        if (!m_ownsLock || m_mutex == nullptr)
        {
            return UniqueLock<InSharedMutexType>{};
        }

        if (!m_mutex->tryUnlockSharedAndLock())
        {
            return UniqueLock<InSharedMutexType>{};
        }

        InSharedMutexType* const upgradedMutex = release();
        return UniqueLock<InSharedMutexType>(*upgradedMutex, AdoptLock);
    }

    /// @brief Exchanges mutex association and shared ownership state with `other`.
    /// @param[in,out] other Guard to swap state with.
    GP_NO_THREAD_SAFETY_ANALYSIS constexpr void swap(SharedLock& other) noexcept
    {
        InSharedMutexType* const tempMutex = m_mutex;
        const bool tempOwns = m_ownsLock;
        m_mutex = other.m_mutex;
        m_ownsLock = other.m_ownsLock;
        other.m_mutex = tempMutex;
        other.m_ownsLock = tempOwns;
    }

    /// @brief Disassociates this guard from its mutex without unlocking it and returns the previously bound pointer.
    /// @return Pointer to the previously associated shared mutex, or `nullptr`.
    [[nodiscard]] GP_NO_THREAD_SAFETY_ANALYSIS constexpr InSharedMutexType* release() noexcept
    {
        InSharedMutexType* const prevMutex = m_mutex;
        m_mutex = nullptr;
        m_ownsLock = false;
        return prevMutex;
    }

    /// @brief Checks whether this guard currently holds shared reader ownership of its associated mutex.
    /// @return True if `m_ownsLock == true`, false otherwise.
    [[nodiscard]] constexpr bool ownsLock() const noexcept
    {
        return m_ownsLock;
    }

    /// @brief Contextual boolean conversion returning `ownsLock()`.
    /// @return True if this guard currently holds the shared lock, false otherwise.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return m_ownsLock;
    }

    /// @brief Retrieves a pointer to the associated shared mutex, or `nullptr` if disassociated.
    /// @return Pointer to `InSharedMutexType`.
    [[nodiscard]] constexpr InSharedMutexType* mutex() const noexcept
    {
        return m_mutex;
    }

private:
    InSharedMutexType* m_mutex{ nullptr };
    bool m_ownsLock{ false };
};

}   // namespace gp::thread
