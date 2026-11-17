// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"
#include "CoreMinimal.hpp"
#include "platforms/generic/system/GenericThread.hpp"
#include "templates/Enums.hpp"
#include "threading/queues/JobQueueTypes.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "threading/roles/ThreadRoleForward.hpp"
#include "threading/sync/JobSyncForward.hpp"

namespace gp::thread
{

/// @brief Number of named threads, usable as an array extent.
inline constexpr gp::USize kNamedThreadCount = static_cast<gp::USize>(NamedThread::Count);

/// @brief Role bits that map one-to-one onto a `NamedThread` claim and are owned by a single OS thread at a time.
inline constexpr ThreadRole kExclusiveThreadRoles = ThreadRole::Game | ThreadRole::Render | ThreadRole::RHI;

/// @brief Role bits that any number of OS threads may hold at once through `ScopedThreadRole`.
inline constexpr ThreadRole kScopedThreadRoles = ThreadRole::ParallelRender;

/// @brief Value stored in the ownership table for a `NamedThread` that no OS thread has claimed.
/// @note No user-mode thread gets id `0` on Win32 or POSIX, so it is safe to use as the empty marker.
inline constexpr gp::platform::ThreadId kUnclaimedThreadId = 0ULL;

/// @brief Default slot count of each named-thread dispatch queue (rounded to a power of two by the queue).
inline constexpr gp::USize kDefaultNamedThreadQueueCapacity = 1024u;

/// @brief `maxTaskCount` value telling `NamedThreadDispatcher::pump()` to drain until the queue looks empty.
inline constexpr gp::UInt32 kPumpAllTasks = ~0u;

/// @brief Checks whether `thread` names a real named thread (`Game`, `Render` or `RHI`).
/// @param[in] thread Value to validate.
/// @return True if `thread < NamedThread::Count`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValidNamedThread(const NamedThread thread) noexcept
{
    return static_cast<gp::UInt8>(thread) < static_cast<gp::UInt8>(NamedThread::Count);
}

/// @brief Converts a named thread into the exclusive role bit that its owner holds.
/// @param[in] thread Named thread to convert.
/// @return `ThreadRole::Game`, `ThreadRole::Render` or `ThreadRole::RHI`, or `ThreadRole::None` if `thread` is out of
///         range.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr ThreadRole toThreadRole(const NamedThread thread) noexcept
{
    if (!isValidNamedThread(thread))
    {
        return ThreadRole::None;
    }
    return static_cast<ThreadRole>(1u << static_cast<gp::UInt8>(thread));
}

/// @brief Tests a held role mask against a query mask with any-of semantics.
/// @details This is the single predicate behind every `isIn*Thread()` query. Asking for `Game | Render` returns true
///          on a thread that holds either role, and asking for `ThreadRole::None` always returns false.
/// @param[in] heldRoles Roles the thread currently plays.
/// @param[in] queriedRoles Roles to look for.
/// @return True if `heldRoles` and `queriedRoles` share at least one bit.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    hasAnyThreadRole(const ThreadRole heldRoles, const ThreadRole queriedRoles) noexcept
{
    return (heldRoles & queriedRoles) != ThreadRole::None;
}

/// @brief Returns a static, human-readable name for a named thread, for logs, profiler markers and OS thread names.
/// @param[in] thread Named thread to describe.
/// @return `"Game"`, `"Render"`, `"RHI"`, or `"Invalid"` for out-of-range values. The string has static storage.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr const char* getNamedThreadName(const NamedThread thread) noexcept
{
    switch (thread)
    {
    case NamedThread::Game:
        return "Game";
    case NamedThread::Render:
        return "Render";
    case NamedThread::RHI:
        return "RHI";
    case NamedThread::Count:
    default:
        break;
    }
    return "Invalid";
}

/// @brief 24-byte POD work item sitting in a named-thread dispatch queue.
/// @details The task is a stateless function pointer plus an opaque payload, the same shape as `JobDecl`
///          ([ADR-0022]). It never owns its payload, so copying it through a lock-free ring is a plain memcpy and the
///          producer keeps full control over the payload's lifetime.
struct NamedThreadTask
{
    /// @brief Callback run on the target named thread. Must not be null.
    JobEntryFunction entry{ nullptr };

    /// @brief Opaque payload forwarded to `entry`. The producer keeps it alive until the task has run.
    void* userData{ nullptr };

    /// @brief Optional completion counter. The dispatcher increments it on enqueue and decrements it after `entry`
    ///        returns, so a producer can wait for the task with `JobCounter::wait()`.
    JobCounter* counter{ nullptr };

    /// @brief Checks whether the task can be dispatched.
    /// @return True if `entry` is not null.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return entry != nullptr;
    }
};

static_assert(sizeof(NamedThreadTask) == 3u * sizeof(void*), "NamedThreadTask must be three pointers wide.");
static_assert(gp::concepts::IsTriviallyCopyable<NamedThreadTask>, "NamedThreadTask must be trivially copyable.");
static_assert(gp::concepts::IsStandardLayout<NamedThreadTask>, "NamedThreadTask must have standard layout.");

}   // namespace gp::thread
