// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "threading/sync/MutexEnums.hpp"     // IWYU pragma: export
#include "threading/sync/MutexForward.hpp"   // IWYU pragma: export

namespace gp::thread
{

/// @brief Tag type indicating that a `UniqueLock` or `SharedLock` should bind to a mutex without acquiring it on
///        construction ([ADR-0058]).
struct DeferLockT final
{
    /// @brief Explicit constexpr default constructor.
    explicit constexpr DeferLockT() noexcept = default;
};

/// @brief Tag type indicating that a `UniqueLock` or `SharedLock` should attempt a non-blocking `tryLock()` or
///        `tryLockShared()` on construction ([ADR-0058]).
struct TryToLockT final
{
    /// @brief Explicit constexpr default constructor.
    explicit constexpr TryToLockT() noexcept = default;
};

/// @brief Tag type indicating that the calling context already holds the lock and transfers ownership to a
///        `UniqueLock` or `SharedLock` on construction ([ADR-0058]).
struct AdoptLockT final
{
    /// @brief Explicit constexpr default constructor.
    explicit constexpr AdoptLockT() noexcept = default;
};

/// @brief Tag instance selecting deferred lock acquisition (`UniqueLock(mutex, DeferLock)`).
inline constexpr DeferLockT DeferLock{};

/// @brief Tag instance selecting non-blocking lock acquisition (`UniqueLock(mutex, TryToLock)`).
inline constexpr TryToLockT TryToLock{};

/// @brief Tag instance selecting adoption of an already-held lock (`UniqueLock(mutex, AdoptLock)`).
inline constexpr AdoptLockT AdoptLock{};

/// @brief Bit [0] (`0x1`) in `Mutex` and `RecursiveMutex` indicating that the lock is currently held.
inline constexpr gp::UInt32 kMutexLockedBit = 0x00000001u;

/// @brief Bit [1] (`0x2`) in `Mutex` and `RecursiveMutex` indicating that one or more threads may be parked in
///        `Futex::waitOnAddress()`.
inline constexpr gp::UInt32 kMutexMayHaveWaitersBit = 0x00000002u;

/// @brief Default user-space `_mm_pause` / `yield` spin count before falling back to `Futex::waitOnAddress()`.
inline constexpr gp::UInt32 kMutexSpinIterationCount = 32u;

/// @brief Bit [63] in `RecursiveMutex`'s 64-bit owner token distinguishing a cooperative child `FiberId` (`1`) from
///        an OS hardware `ThreadId` (`0`) ([ADR-0049], [ADR-0058]).
inline constexpr gp::UInt64 kRecursiveMutexFiberOwnerBit = (1ULL << 63u);

/// @brief Sentinel 64-bit owner token (`0`) representing an unowned `RecursiveMutex`.
inline constexpr gp::UInt64 kInvalidExecutionOwnerToken = 0ULL;

/// @brief Bit [31] (`0x80000000u`) in `SharedMutex::m_state` indicating an active exclusive writer holds the lock.
inline constexpr gp::UInt32 kSharedMutexWriterLockedBit = (1u << 31u);

/// @brief Bit [30] (`0x40000000u`) in `SharedMutex::m_state` indicating at least one writer is waiting for exclusive
///        access, blocking new shared readers to prevent writer starvation.
inline constexpr gp::UInt32 kSharedMutexWriterWaitingBit = (1u << 30u);

/// @brief Bit [29] (`0x20000000u`) in `SharedMutex::m_state` indicating at least one shared reader is parked on
///        `m_readerWakeEpoch`.
inline constexpr gp::UInt32 kSharedMutexReaderWaitingBit = (1u << 29u);

/// @brief Bitmask (`0x1FFFFFFFu`) isolating the 29-bit active shared reader count in `SharedMutex::m_state`.
inline constexpr gp::UInt32 kSharedMutexReaderCountMask = 0x1FFFFFFFu;

/// @brief Maximum simultaneous shared readers (`536,870,911`) representable in `SharedMutex::m_state`.
inline constexpr gp::UInt32 kSharedMutexMaxReaders = kSharedMutexReaderCountMask;

/// @brief Increment quantum (`1u`) representing one active shared reader in `SharedMutex::m_state`.
inline constexpr gp::UInt32 kSharedMutexOneReader = 1u;

}   // namespace gp::thread

namespace gp::concepts
{

/// @brief Concept satisfied by any mutex type providing `lock()` and `unlock()` member functions ([ADR-0058]).
template <typename InMutexType>
concept IsBasicLockable = requires(InMutexType& mutex) {
    {
        mutex.lock()
    } noexcept;
    {
        mutex.unlock()
    } noexcept;
};

/// @brief Concept satisfied by any mutex type providing `lock()`, `tryLock()`, and `unlock()` ([ADR-0058]).
template <typename InMutexType>
concept IsLockable = IsBasicLockable<InMutexType> && requires(InMutexType& mutex) {
    {
        mutex.tryLock()
    } noexcept -> IsSameAs<bool>;
};

/// @brief Concept satisfied by any reader-writer mutex type providing `lockShared()`, `tryLockShared()`, and
///        `unlockShared()` alongside exclusive locking ([ADR-0058]).
template <typename InMutexType>
concept IsSharedLockable = IsLockable<InMutexType> && requires(InMutexType& mutex) {
    {
        mutex.lockShared()
    } noexcept;
    {
        mutex.tryLockShared()
    } noexcept -> IsSameAs<bool>;
    {
        mutex.unlockShared()
    } noexcept;
};

}   // namespace gp::concepts

namespace gp
{

using thread::AdoptLock;
using thread::DeferLock;
using thread::TryToLock;

}   // namespace gp
