// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/generic/system/GenericThread.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "threading/roles/ThreadRoleForward.hpp"
#include "threading/roles/ThreadRoleTypes.hpp"

namespace gp::thread
{

/// @brief Process-wide registry that tells each OS thread which engine role it plays.
/// @details Two pieces of state back the registry:
///          - A thread-local `ThreadRole` mask per OS thread. Every `isIn*Thread()` query is one TLS load and one
///            AND, with no shared cache line touched, so asserts can call it on hot paths.
///          - A table of `kNamedThreadCount` atomic owner ids. Attaching a named thread is a compare-and-swap from
///            `kUnclaimedThreadId` to the caller's OS thread id, which is how exclusivity is enforced across threads.
///          Unreal Engine answers the same questions by comparing the current thread id with globals such as
///          `GGameThreadId` (`IsInGameThread()` in `CoreGlobals.h`). A role mask costs the same single load but also
///          lets one thread hold several roles and lets job workers borrow `ParallelRender` for a scope, which a bare
///          id comparison cannot express.
/// @note Roles belong to the OS thread, not to a fiber. A job that pushes `ParallelRender` must pop it before it
///       yields, because the fiber may resume on a different worker ([ADR-0049], [ADR-0052]).
class GP_CORE_API ThreadRoleRegistry final
{
public:
    ThreadRoleRegistry() = delete;

public:
    /// @section Named Thread Ownership

    /// @brief Claims `thread` for the calling OS thread and sets the matching exclusive role bit in its TLS mask.
    /// @details A thread may claim several named threads, one call each. The claim stays in place until the same
    ///          thread calls `detachCurrentThread()`.
    /// @param[in] thread Named thread to claim.
    /// @return `Success`, `InvalidArguments` if `thread` is out of range, `AlreadyAttached` if the caller already owns
    ///         it, or `RoleClaimedByOtherThread` if another OS thread owns it.
    /// @note Call `detachCurrentThread()` before the OS thread exits. A thread that dies while attached leaves its
    ///       claim in the table and blocks every later attach. `ScopedNamedThread` handles this automatically.
    [[nodiscard]] static ThreadRoleResult attachCurrentThread(NamedThread thread) noexcept;

    /// @brief Releases a named thread previously claimed by the calling OS thread.
    /// @param[in] thread Named thread to release.
    /// @return `Success`, `InvalidArguments` if `thread` is out of range, or `NotAttached` if the caller does not own
    ///         it.
    [[nodiscard]] static ThreadRoleResult detachCurrentThread(NamedThread thread) noexcept;

    /// @brief Checks whether any OS thread currently owns `thread`.
    /// @param[in] thread Named thread to inspect.
    /// @return True if the ownership table holds a thread id for `thread`. False for out-of-range values.
    [[nodiscard]] static bool isAttached(NamedThread thread) noexcept;

    /// @brief Returns the OS thread id that owns `thread`, for diagnostics and profiler thread naming.
    /// @param[in] thread Named thread to inspect.
    /// @return The owner's id, or `kUnclaimedThreadId` if nobody owns it or `thread` is out of range.
    [[nodiscard]] static gp::platform::ThreadId getOwnerThreadId(NamedThread thread) noexcept;

public:
    /// @section Scoped (Shared) Roles

    /// @brief Adds scoped role bits to the calling thread's TLS mask.
    /// @details Prefer `ScopedThreadRole`, which pairs this call with `popScopedRoles()` automatically.
    /// @param[in] roles Bits to add. Must be non-empty and contain only bits from `kScopedThreadRoles`.
    /// @param[out] outPreviousRoles Receives the mask as it was before the call, on success and on failure.
    /// @return `Success`, or `InvalidArguments` if `roles` is empty or contains an exclusive bit.
    [[nodiscard]] static ThreadRoleResult pushScopedRoles(ThreadRole roles, ThreadRole& outPreviousRoles) noexcept;

    /// @brief Restores the scoped bits of the calling thread's TLS mask from a value returned by `pushScopedRoles()`.
    /// @details Only the scoped bits are restored. Exclusive bits keep their current value, so a stale snapshot can
    ///          neither revoke a named thread attached in between nor forge one that was never claimed.
    /// @param[in] previousRoles Mask previously written to `outPreviousRoles`.
    static void popScopedRoles(ThreadRole previousRoles) noexcept;

public:
    /// @section Queries

    /// @brief Returns the full role mask of the calling OS thread.
    /// @return Current TLS mask (`ThreadRole::None` on a thread that never registered).
    /// @note Implemented out of line behind a compiler barrier so the TLS address is never cached across a fiber
    ///       switch.
    [[nodiscard]] static ThreadRole getCurrentRoles() noexcept;

    /// @brief Checks whether the calling OS thread holds any of `roles`.
    /// @param[in] roles Roles to look for, combined with `|`.
    /// @return `hasAnyThreadRole(getCurrentRoles(), roles)`.
    [[nodiscard]] static bool isInRole(ThreadRole roles) noexcept;
};

/// @section Free query functions
/// @details These are the entry points gameplay and rendering code should use in asserts such as
///          `GP_ASSERT(gp::thread::isInRenderThread())`. They all go through the same predicate,
///          `ThreadRoleRegistry::isInRole()`, so there is one rule to learn.

/// @brief Checks whether the calling OS thread holds any of `roles`.
/// @param[in] roles Roles to look for, combined with `|`.
/// @return True if at least one bit of `roles` is held.
[[nodiscard]] GP_FORCEINLINE_HINT bool isInThreadRole(const ThreadRole roles) noexcept
{
    return ThreadRoleRegistry::isInRole(roles);
}

/// @brief Checks whether the calling OS thread owns `thread`.
/// @param[in] thread Named thread to test.
/// @return True if the caller holds `toThreadRole(thread)`. False for out-of-range values.
[[nodiscard]] GP_FORCEINLINE_HINT bool isInThreadRole(const NamedThread thread) noexcept
{
    return ThreadRoleRegistry::isInRole(toThreadRole(thread));
}

/// @brief Checks whether the calling OS thread owns the game thread role.
/// @return True if the caller holds `ThreadRole::Game`.
[[nodiscard]] GP_FORCEINLINE_HINT bool isInGameThread() noexcept
{
    return isInThreadRole(ThreadRole::Game);
}

/// @brief Checks whether the calling OS thread owns the render thread role.
/// @return True if the caller holds `ThreadRole::Render`.
/// @note Returns false on job workers recording render work. Use `isInParallelRenderThread()` for code that may run
///       there too.
[[nodiscard]] GP_FORCEINLINE_HINT bool isInRenderThread() noexcept
{
    return isInThreadRole(ThreadRole::Render);
}

/// @brief Checks whether the calling OS thread owns the RHI thread role.
/// @return True if the caller holds `ThreadRole::RHI`.
[[nodiscard]] GP_FORCEINLINE_HINT bool isInRHIThread() noexcept
{
    return isInThreadRole(ThreadRole::RHI);
}

/// @brief Checks whether the calling OS thread may touch render-thread data, either as the render thread itself or as
///        a worker inside a `ParallelRender` scope.
/// @return True if the caller holds `ThreadRole::Render` or `ThreadRole::ParallelRender`.
[[nodiscard]] GP_FORCEINLINE_HINT bool isInParallelRenderThread() noexcept
{
    return isInThreadRole(ThreadRole::Render | ThreadRole::ParallelRender);
}

}   // namespace gp::thread
