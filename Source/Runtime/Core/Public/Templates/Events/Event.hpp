// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/Utility.hpp"
#include "Templates/Events/MulticastDelegate.hpp"

namespace GP
{

/// @brief Owner-locked publish-subscribe event built on TMulticastDelegate.
///
/// Enforces a strict Publish-Subscribe ownership model:
///  - \b Subscribers (anyone): may call Add*() and Remove() / RemoveAll() via
///    the public interface.
///  - \b Publisher (owner only): may call Broadcast() because \c TOwner is
///    declared as a \c friend.
///
/// This mirrors the encapsulation contract seen in Unreal Engine's
/// \c DECLARE_EVENT macros and Decima's signal system: the owning class controls
/// when an event fires; external code only observes it.
///
/// Declare an event as a private member of the broadcasting class and expose
/// a const reference or a public accessor returning a subscription-only view.
///
/// Example (inside the owner):
/// @code
///   class FPhysicsWorld
///   {
///   public:
///       // Expose subscription interface; callers cannot Broadcast.
///       TEvent<FPhysicsWorld, void(FCollisionInfo const&)>& OnCollision()
///       {
///           return m_onCollision;
///       }
///
///   private:
///       void SimulationStep()
///       {
///           // Only FPhysicsWorld can call Broadcast.
///           m_onCollision.Broadcast(collisionInfo);
///       }
///
///       TEvent<FPhysicsWorld, void(FCollisionInfo const&)> m_onCollision;
///   };
///
///   // Subscriber:
///   FDelegateHandle handle = physicsWorld.OnCollision().Add(
///       [](FCollisionInfo const& info) { /* ... */ }
///   );
/// @endcode
///
/// @tparam TOwner The class that is permitted to call Broadcast().
/// @tparam Signature Full function signature; must be \c void(TArgs...).
template <typename TOwner, typename Signature>
class TEvent;

template <typename TOwner, typename... TArgs>
class TEvent<TOwner, void(TArgs...)>
{
private:
    // Only the designated owner class may call Broadcast().
    friend TOwner;

private:
    using FDelegate = TMulticastDelegate<void(TArgs...)>;

private:
    FDelegate m_delegate;   //<! Internal multicast delegate handling all subscription logic.

public:
    ///
    /// @section Constructors / Destructor
    ///

    TEvent() noexcept = default;
    ~TEvent() noexcept = default;

    // Events are non-copyable and non-movable: ownership semantics and friend
    // access are tied to the declaring class instance.
    TEvent(const TEvent&) = delete;
    TEvent(TEvent&&) noexcept = delete;
    TEvent& operator=(const TEvent&) = delete;
    TEvent& operator=(TEvent&&) noexcept = delete;

public:
    /// @brief Subscribes a free or static member function.
    /// @param fn Non-null function pointer.
    /// @return Subscription handle.
    FDelegateHandle AddFunction(void (*fn)(TArgs...)) { return m_delegate.AddFunction(fn); }

    /// @brief Subscribes a mutable member function on a specific object instance.
    /// @tparam T Instance type.
    /// @param instance Non-null pointer to the listening object.
    /// @param method Pointer to the member function.
    /// @return Subscription handle.
    template <typename T>
    FDelegateHandle AddMethod(T* instance, void (T::*method)(TArgs...))
    {
        return m_delegate.AddMethod(instance, method);
    }

    /// @brief Subscribes a const-qualified member function on a specific object instance.
    /// @tparam T Instance type.
    /// @param instance Non-null const pointer to the listening object.
    /// @param method Pointer to the const member function.
    /// @return Subscription handle.
    template <typename T>
    FDelegateHandle AddMethod(const T* instance, void (T::*method)(TArgs...) const)
    {
        return m_delegate.AddMethod(instance, method);
    }

    /// \brief Subscribes any callable: lambda, functor, std::function, etc.
    /// \tparam TCallable  Any callable type matching \c void(TArgs...).
    /// \param callable Callable to copy/move into the event.
    /// \return Subscription handle.
    template <typename TCallable>
    FDelegateHandle Add(TCallable&& callable)
    {
        return m_delegate.Add(GP::Forward<TCallable>(callable));
    }

    /// @brief Removes the binding identified by \p handle.
    /// Safe to call from within a Broadcast() handler.
    /// @param handle Handle returned by a previous Add*() call.
    /// @return True if a matching active binding was found.
    bool Remove(FDelegateHandle handle) { return m_delegate.Remove(handle); }

    /// @brief Removes all bindings registered via AddMethod() on \p rawInstance.
    /// @param rawInstance Pointer to the object whose method bindings should be removed.
    /// @return Number of bindings removed.
    Int32 RemoveAll(void* rawInstance) { return m_delegate.RemoveAll(rawInstance); }

    /// @brief Returns true if no active bindings exist.
    GP_NODISCARD bool IsEmpty() const noexcept { return m_delegate.IsEmpty(); }

    /// @brief Returns the number of active bindings.
    GP_NODISCARD Int32 GetBindingCount() const noexcept { return m_delegate.GetBindingCount(); }

    /// @brief Returns true if the given handle refers to an active binding.
    GP_NODISCARD bool Contains(FDelegateHandle handle) const noexcept { return m_delegate.Contains(handle); }

private:
    /// @brief Fires the event, invoking all active subscribers in order.
    /// Must be called by the owner class only. External code has no access.
    /// @param args  Arguments forwarded to each binding.
    void Broadcast(TArgs... args) { m_delegate.Broadcast(GP::Forward<TArgs>(args)...); }

    /// @brief Removes all bindings. Owner-only operation.
    /// @warning Must NOT be called from within a Broadcast() handler.
    void Clear() { m_delegate.Clear(); }
};

}   // namespace GP
