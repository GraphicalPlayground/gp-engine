// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/roles/ThreadRole.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::thread
{

namespace detail
{

/// @brief Role mask of the calling OS thread.
/// @note A trivially constructible `thread_local` needs no lazy-init guard, so reading it is a single TLS load.
static thread_local ThreadRole tl_threadRoles{ ThreadRole::None };

/// @brief Owner id of each named thread, indexed by `NamedThread`.
/// @note `Atomic` has a `constexpr` constructor, so the table is constant-initialized and is valid before any static
///       constructor runs. A thread attached during static init still sees correct state.
static Atomic<gp::platform::ThreadId> g_namedThreadOwners[kNamedThreadCount]{};

/// @brief Compiler-only read/write barrier that stops the optimizer from caching the TLS address across a call that
///        may switch fibers.
static GP_FORCEINLINE void threadRoleCompilerBarrier() noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#else
    _ReadWriteBarrier();
#endif
}

/// @brief Reads the calling thread's role mask through an out-of-line barrier.
/// @details A fiber that yields on worker A and resumes on worker B must see B's mask. Keeping the read out of line
///          and fenced stops the compiler from reusing A's TLS slot address ([ADR-0049]).
/// @return Current role mask.
[[nodiscard]] static GP_FORCENOINLINE ThreadRole readRolesTLS() noexcept
{
    threadRoleCompilerBarrier();
    return tl_threadRoles;
}

/// @brief Writes the calling thread's role mask through an out-of-line barrier.
/// @param[in] roles New role mask.
static GP_FORCENOINLINE void writeRolesTLS(const ThreadRole roles) noexcept
{
    threadRoleCompilerBarrier();
    tl_threadRoles = roles;
    threadRoleCompilerBarrier();
}

/// @brief Maps a validated named thread to its slot in `g_namedThreadOwners`.
/// @param[in] thread Named thread. Must satisfy `isValidNamedThread()`.
/// @return Reference to the owner slot.
[[nodiscard]] static GP_FORCEINLINE Atomic<gp::platform::ThreadId>& ownerSlot(const NamedThread thread) noexcept
{
    return g_namedThreadOwners[static_cast<gp::USize>(thread)];
}

}   // namespace detail

ThreadRoleResult ThreadRoleRegistry::attachCurrentThread(const NamedThread thread) noexcept
{
    if (!isValidNamedThread(thread)) [[unlikely]]
    {
        return ThreadRoleResult::InvalidArguments;
    }

    const ThreadRole role = toThreadRole(thread);
    const ThreadRole currentRoles = detail::readRolesTLS();

    // The TLS mask decides "do I already own it", never the table. OS thread ids get reused, so a table entry left by a
    // dead thread could match the caller's id by accident.
    if (hasAnyThreadRole(currentRoles, role))
    {
        return ThreadRoleResult::AlreadyAttached;
    }

    gp::platform::ThreadId expected = kUnclaimedThreadId;
    const gp::platform::ThreadId selfId = gp::platform::Thread::currentThreadId();
    if (!detail::ownerSlot(thread).compareExchangeStrong(
            expected, selfId, gp::MemoryOrder::AcqRel, gp::MemoryOrder::Acquire
        ))
    {
        return ThreadRoleResult::RoleClaimedByOtherThread;
    }

    detail::writeRolesTLS(currentRoles | role);
    return ThreadRoleResult::Success;
}

ThreadRoleResult ThreadRoleRegistry::detachCurrentThread(const NamedThread thread) noexcept
{
    if (!isValidNamedThread(thread)) [[unlikely]]
    {
        return ThreadRoleResult::InvalidArguments;
    }

    const ThreadRole role = toThreadRole(thread);
    const ThreadRole currentRoles = detail::readRolesTLS();
    if (!hasAnyThreadRole(currentRoles, role))
    {
        return ThreadRoleResult::NotAttached;
    }

    // Clear TLS first so the caller stops passing role checks before another thread can claim the slot.
    detail::writeRolesTLS(currentRoles & ~role);
    detail::ownerSlot(thread).store(kUnclaimedThreadId, gp::MemoryOrder::Release);
    return ThreadRoleResult::Success;
}

bool ThreadRoleRegistry::isAttached(const NamedThread thread) noexcept
{
    return getOwnerThreadId(thread) != kUnclaimedThreadId;
}

gp::platform::ThreadId ThreadRoleRegistry::getOwnerThreadId(const NamedThread thread) noexcept
{
    if (!isValidNamedThread(thread)) [[unlikely]]
    {
        return kUnclaimedThreadId;
    }
    return detail::ownerSlot(thread).load(gp::MemoryOrder::Acquire);
}

ThreadRoleResult ThreadRoleRegistry::pushScopedRoles(const ThreadRole roles, ThreadRole& outPreviousRoles) noexcept
{
    const ThreadRole currentRoles = detail::readRolesTLS();
    outPreviousRoles = currentRoles;

    const bool isEmpty = roles == ThreadRole::None;
    const bool hasExclusiveBits = hasAnyThreadRole(roles, kExclusiveThreadRoles);
    const bool hasUnknownBits = hasAnyThreadRole(roles, ~(kExclusiveThreadRoles | kScopedThreadRoles));
    if (isEmpty || hasExclusiveBits || hasUnknownBits) [[unlikely]]
    {
        return ThreadRoleResult::InvalidArguments;
    }

    detail::writeRolesTLS(currentRoles | roles);
    return ThreadRoleResult::Success;
}

void ThreadRoleRegistry::popScopedRoles(const ThreadRole previousRoles) noexcept
{
    const ThreadRole currentRoles = detail::readRolesTLS();
    const ThreadRole restoredRoles = (currentRoles & kExclusiveThreadRoles) | (previousRoles & kScopedThreadRoles);
    detail::writeRolesTLS(restoredRoles);
}

ThreadRole ThreadRoleRegistry::getCurrentRoles() noexcept
{
    return detail::readRolesTLS();
}

bool ThreadRoleRegistry::isInRole(const ThreadRole roles) noexcept
{
    return hasAnyThreadRole(detail::readRolesTLS(), roles);
}

}   // namespace gp::thread
