// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Events/DelegateHandle.hpp"
#include "Events/MulticastDelegate.hpp"
#include <functional>

namespace GP::Events
{

/// \brief An RAII wrapper around FDelegateHandle that automatically unregisters its binding from a TMulticastDelegate
/// when it goes out of scope or is explicitly released.
///
/// FScopedDelegateHandle is move-only. Moving it transfers ownership of the remove-callback; the source becomes empty
/// (equivalent to an invalid handle) and will not call Remove().
///
/// ### Recommended Usage
/// Use FScopedDelegateHandle as a member of any class that subscribes to an event, ensuring the subscription is cleaned
/// up automatically when the class is destroyed.
///
/// \par Usage Example
/// \code
/// class FMyRenderer
/// {
/// public:
///     explicit FMyRenderer(IWindow& window)
///     {
///         m_onResizedHandle = FScopedDelegateHandle(
///             window.OnResized,
///             window.OnResized.AddMember(this, &FMyRenderer::HandleResize)
///         );
///     }
///
/// private:
///     void HandleResize(const FWindowResizedEvent& e) { /* ... */ }
///     FScopedDelegateHandle m_onResizedHandle;
/// };
/// \endcode
///
/// \note FScopedDelegateHandle stores the removal operation as a type-erased std::function, allowing it to work with
/// any TMulticastDelegate instantiation without templating the handle itself.
class FScopedDelegateHandle final
{
private:
    FDelegateHandle m_handle;          //<! The delegate handle being managed.
    std::function<void()> m_remover;   //<! The callback that will remove the handle from the delegate when invoked.

public:
    /// \brief Constructs an empty, inactive scoped handle.
    FScopedDelegateHandle() noexcept = default;

    /// \brief Constructs a scoped handle that will call delegate.Remove(handle) on destruction.
    /// \tparam Args The parameter types of the target TMulticastDelegate.
    /// \param delegate The delegate to remove from. Must outlive this scoped handle.
    /// \param handle The FDelegateHandle returned by a previous delegate.Add*() call.
    template <typename... Args>
    FScopedDelegateHandle(TMulticastDelegate<void(Args...)>& delegate, FDelegateHandle handle)
        : m_handle(handle)
        , m_remover([&delegate, handle]() mutable { delegate.Remove(handle); })
    {}

    /// \brief On destruction, removes the associated binding from the delegate (if still valid).
    ~FScopedDelegateHandle() { Release(); }

    /// \brief Move constructor — transfers ownership; source becomes empty.
    /// \param other The source FScopedDelegateHandle to move from.
    FScopedDelegateHandle(FScopedDelegateHandle&& other) noexcept
        : m_handle(other.m_handle)
        , m_remover(std::move(other.m_remover))
    {
        other.m_handle.Reset();
    }

    /// \brief Copy constructor is deleted to prevent multiple handles managing the same binding.
    FScopedDelegateHandle(const FScopedDelegateHandle&) = delete;

public:
    /// \brief Move assignment — releases current binding, then takes ownership from source.
    /// \param other The source FScopedDelegateHandle to move from.
    /// \return *this after move assignment.
    FScopedDelegateHandle& operator=(FScopedDelegateHandle&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_handle = other.m_handle;
            m_remover = std::move(other.m_remover);
            other.m_handle.Reset();
        }
        return *this;
    }

    /// \brief Copy assignment is deleted to prevent multiple handles managing the same binding.
    FScopedDelegateHandle& operator=(const FScopedDelegateHandle&) = delete;

public:
    /// \brief Returns true if this handle refers to a live binding.
    /// \return True if the handle is valid, false if it is invalid (i.e., has an ID of InvalidID).
    GP_NODISCARD bool IsValid() const noexcept { return m_handle.IsValid(); }

    /// \brief Returns the underlying FDelegateHandle (non-owning view).
    /// \return The FDelegateHandle being managed. Note that this handle may become invalid if the scoped handle is
    /// moved from or released.
    GP_NODISCARD FDelegateHandle GetHandle() const noexcept { return m_handle; }

    /// \brief Immediately removes the binding and invalidates this handle. Subsequent destructor calls will be no-ops.
    void Release()
    {
        if (m_handle.IsValid() && m_remover)
        {
            m_remover();
            m_remover = nullptr;
            m_handle.Reset();
        }
    }

    /// \brief Detaches ownership without removing the binding. The caller becomes responsible for manually calling
    /// delegate.Remove().
    /// \return The underlying FDelegateHandle.
    FDelegateHandle Detach() noexcept
    {
        const FDelegateHandle detached = m_handle;
        m_handle.Reset();
        m_remover = nullptr;
        return detached;
    }
};

}   // namespace GP::Events
