// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "threading/roles/ThreadRole.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "threading/roles/ThreadRoleForward.hpp"

namespace gp::thread
{

/// @brief RAII guard that adds scoped role bits (such as `ParallelRender`) to the calling thread for one C++ scope.
/// @details Typical use is at the top of a job that records render commands on a worker:
/// @code
/// void recordShadowPass(void* userData)
/// {
///     gp::thread::ScopedThreadRole parallelRender(gp::thread::ThreadRole::ParallelRender);
///     // Code below may now assert gp::thread::isInParallelRenderThread().
/// }
/// @endcode
///          Guards nest. Each one restores the mask it found, so destruction in reverse order unwinds cleanly.
/// @note Must be constructed and destroyed on the same OS thread, and must not span a fiber yield.
class ScopedThreadRole final
{
public:
    /// @brief Pushes `roles` onto the calling thread's TLS mask.
    /// @param[in] roles Scoped bits to add. Exclusive bits are rejected and leave the mask untouched.
    explicit ScopedThreadRole(const ThreadRole roles) noexcept
        : m_result(ThreadRoleRegistry::pushScopedRoles(roles, m_previousRoles))
    {}

    /// @brief Restores the scoped bits that were in place at construction, if the push succeeded.
    ~ScopedThreadRole() noexcept
    {
        if (m_result == ThreadRoleResult::Success)
        {
            ThreadRoleRegistry::popScopedRoles(m_previousRoles);
        }
    }

    ScopedThreadRole(const ScopedThreadRole&) = delete;
    ScopedThreadRole& operator=(const ScopedThreadRole&) = delete;
    ScopedThreadRole(ScopedThreadRole&&) = delete;
    ScopedThreadRole& operator=(ScopedThreadRole&&) = delete;

public:
    /// @brief Returns the result of the push performed by the constructor.
    /// @return `ThreadRoleResult::Success` or `ThreadRoleResult::InvalidArguments`.
    [[nodiscard]] GP_FORCEINLINE_HINT ThreadRoleResult getResult() const noexcept
    {
        return m_result;
    }

    /// @brief Checks whether the guard changed the mask and will restore it on destruction.
    /// @return True if the push succeeded.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isActive() const noexcept
    {
        return m_result == ThreadRoleResult::Success;
    }

private:
    ThreadRole m_previousRoles{ ThreadRole::None };
    ThreadRoleResult m_result{ ThreadRoleResult::InvalidArguments };
};

/// @brief RAII guard that claims a named thread for the calling OS thread and releases it on destruction.
/// @details Put one at the top of the render or RHI thread entry function so the claim is released on every exit path:
/// @code
/// gp::UInt32 renderThreadMain(void* userData)
/// {
///     gp::thread::ScopedNamedThread renderRole(gp::thread::NamedThread::Render);
///     if (!renderRole.isAttached())
///     {
///         return 1u;
///     }
///     // Pump the render queue until shutdown.
///     return 0u;
/// }
/// @endcode
/// @note The destructor only releases a claim this guard made. If the thread already owned the named thread
///       (`AlreadyAttached`), the outer owner stays responsible for releasing it.
class ScopedNamedThread final
{
public:
    /// @brief Attaches the calling OS thread to `thread`.
    /// @param[in] thread Named thread to claim.
    explicit ScopedNamedThread(const NamedThread thread) noexcept
        : m_thread(thread)
        , m_result(ThreadRoleRegistry::attachCurrentThread(thread))
    {}

    /// @brief Detaches the calling OS thread if, and only if, the constructor attached it.
    ~ScopedNamedThread() noexcept
    {
        if (m_result == ThreadRoleResult::Success)
        {
            (void)ThreadRoleRegistry::detachCurrentThread(m_thread);
        }
    }

    ScopedNamedThread(const ScopedNamedThread&) = delete;
    ScopedNamedThread& operator=(const ScopedNamedThread&) = delete;
    ScopedNamedThread(ScopedNamedThread&&) = delete;
    ScopedNamedThread& operator=(ScopedNamedThread&&) = delete;

public:
    /// @brief Returns the result of the attach performed by the constructor.
    /// @return Any value `ThreadRoleRegistry::attachCurrentThread()` can return.
    [[nodiscard]] GP_FORCEINLINE_HINT ThreadRoleResult getResult() const noexcept
    {
        return m_result;
    }

    /// @brief Checks whether this guard owns the claim and will release it.
    /// @return True if the attach succeeded.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isAttached() const noexcept
    {
        return m_result == ThreadRoleResult::Success;
    }

    /// @brief Returns the named thread this guard tried to claim.
    /// @return The `NamedThread` passed to the constructor.
    [[nodiscard]] GP_FORCEINLINE_HINT NamedThread getNamedThread() const noexcept
    {
        return m_thread;
    }

private:
    NamedThread m_thread{ NamedThread::Count };
    ThreadRoleResult m_result{ ThreadRoleResult::InvalidArguments };
};

}   // namespace gp::thread
