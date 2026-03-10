// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/Utility.hpp"
#include "Templates/Events/DelegateHandle.hpp"
#include "Templates/Events/MulticastDelegate.hpp"

namespace GP
{

// Forward declaration
template <typename Signature>
class TEvent;

/// @brief Non-owning, subscribe-only reference into a TEvent.
///
/// TEventView wraps a single pointer to a TMulticastDelegate and exposes only
/// the subscription half of the API: Add, AddScoped, AddMethod, AddFunction,
/// Remove, RemoveAll, and query methods. Broadcast() is intentionally absent.
///
/// @par Zero overhead
/// sizeof(TEventView) == sizeof(void*). Passing or storing a TEventView is as
/// cheap as passing a pointer. No virtual dispatch, no heap allocation.
///
/// @par Lifetime
/// A TEventView is only valid while its TEvent is alive. Never store a
/// TEventView that may outlive the source.
///
/// @par How to obtain a TEventView
/// Call TEvent::GetView(), or access the named view members on
/// FWindowEvents (and similar event-hub structs).
///
/// @code
///   // In IWindow:
///   virtual FWindowEvents& GetEvents() noexcept = 0;
///
///   // Subscriber:
///   FScopedDelegateHandle h = window.GetEvents().onResized.AddScoped(
///       [&](FWindowResizedEvent const& e) { swapchain.Resize(e.newSize); }
///   );
/// @endcode
///
/// @tparam Signature  Must be void(TArgs...).
template <typename Signature>
class TEventView;

template <typename... TArgs>
class TEventView<void(TArgs...)>
{
private:
    using FDelegate = TMulticastDelegate<void(TArgs...)>;

private:
    FDelegate* m_delegate{ nullptr };   //<! Non-owning pointer to the backing delegate.

public:
    /// @brief Constructs a view bound to \p delegate.
    ///        Prefer TEvent::GetView() over direct construction.
    explicit TEventView(FDelegate& delegate) noexcept
        : m_delegate(&delegate)
    {}

    /// @brief Implicitly convertible from TEvent& for convenient GetView() implementation.
    TEventView(class TEvent<void(TArgs...)>& delegate) noexcept
        : m_delegate(&delegate.m_delegate)
    {}

    constexpr TEventView() noexcept = default;
    TEventView(const TEventView&) noexcept = default;
    TEventView(TEventView&&) noexcept = default;
    TEventView& operator=(const TEventView&) noexcept = default;
    TEventView& operator=(TEventView&&) noexcept = default;
    ~TEventView() noexcept = default;

public:
    /// @brief Subscribes a callable and returns a reference to this view for chaining. Preferred over Add().
    /// @tparam TCallable Any callable matching void(TArgs...).
    template <typename TCallable>
    TEventView& operator+=(TCallable&& callable)
    {
        GP_ASSERT(IsValid(), "TEventView::operator+= - view is not bound to an event source.");
        m_delegate->Add(GP::Forward<TCallable>(callable));
        return *this;
    }

    ///  @brief Removes the binding identified by \p handle.
    TEventView& operator-=(FDelegateHandle handle)
    {
        GP_ASSERT(IsValid(), "TEventView::operator-= - view is not bound to an event source.");
        m_delegate->Remove(handle);
        return *this;
    }

    /// @brief Removes all method bindings registered for \p rawInstance. Equivalent to RemoveAll(rawInstance).
    TEventView& operator-=(void* rawInstance)
    {
        GP_ASSERT(IsValid(), "TEventView::operator-= - view is not bound to an event source.");
        m_delegate->RemoveAll(rawInstance);
        return *this;
    }

public:
    /// @brief Returns true if this view is bound to an event source.
    GP_NODISCARD GP_FORCEINLINE bool IsValid() const noexcept { return m_delegate != nullptr; }

    /// @brief Subscribes a callable and returns an RAII handle that auto-removes when it goes out of scope. Preferred
    ///        over Add().
    /// @tparam TCallable Any callable matching void(TArgs...).
    /// @param callable Callable to store. Copied or moved into the event.
    /// @return RAII scoped handle; subscription lives exactly as long as the handle.
    template <typename TCallable>
    GP_NODISCARD FScopedDelegateHandle AddScoped(TCallable&& callable)
    {
        GP_ASSERT(IsValid(), "TEventView::AddScoped - view is not bound to an event source.");

        FDelegateHandle handle = m_delegate->Add(GP::Forward<TCallable>(callable));

        return FScopedDelegateHandle(
            m_delegate,
            [](void* src, FDelegateHandle h) noexcept -> bool { return static_cast<FDelegate*>(src)->Remove(h); },
            handle
        );
    }

    /// @brief Subscribes a callable and returns a raw handle. Caller is
    ///        responsible for calling Remove(). Prefer AddScoped() instead.
    ///
    /// \tparam TCallable Any callable matching void(TArgs...).
    /// \param  callable Callable to store.
    /// \return Raw subscription handle.
    template <typename TCallable>
    GP_NODISCARD FDelegateHandle Add(TCallable&& callable)
    {
        GP_ASSERT(IsValid(), "TEventView::Add - view is not bound to an event source.");
        return m_delegate->Add(GP::Forward<TCallable>(callable));
    }

    /// @brief Subscribes a mutable member function. Returns a raw handle.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddMethod(T* instance, void (T::*method)(TArgs...))
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethod - view is not bound to an event source.");
        return m_delegate->AddMethod(instance, method);
    }

    /// @brief Subscribes a mutable member function. Returns an RAII scoped handle.
    template <typename T>
    GP_NODISCARD FScopedDelegateHandle AddMethodScoped(T* instance, void (T::*method)(TArgs...))
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethodScoped - view is not bound to an event source.");

        FDelegateHandle handle = m_delegate->AddMethod(instance, method);

        return FScopedDelegateHandle(
            m_delegate,
            [](void* src, FDelegateHandle h) noexcept -> bool { return static_cast<FDelegate*>(src)->Remove(h); },
            handle
        );
    }

    /// @brief Subscribes a const member function. Returns a raw handle.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddMethod(const T* instance, void (T::*method)(TArgs...) const)
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethod - view is not bound to an event source.");
        return m_delegate->AddMethod(instance, method);
    }

    /// @brief Subscribes a const member function. Returns an RAII scoped handle.
    template <typename T>
    GP_NODISCARD FScopedDelegateHandle AddMethodScoped(const T* instance, void (T::*method)(TArgs...) const)
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethodScoped - view is not bound to an event source.");

        FDelegateHandle handle = m_delegate->AddMethod(instance, method);

        return FScopedDelegateHandle(
            m_delegate,
            [](void* src, FDelegateHandle h) noexcept -> bool { return static_cast<FDelegate*>(src)->Remove(h); },
            handle
        );
    }

    /// @brief Subscribes a free or static member function. Returns a raw handle.
    GP_NODISCARD FDelegateHandle AddFunction(void (*fn)(TArgs...))
    {
        GP_ASSERT(IsValid(), "TEventView::AddFunction - view is not bound to an event source.");
        return m_delegate->AddFunction(fn);
    }

    /// @brief Removes the subscription identified by \p handle.
    ///        Safe to call re-entrantly from within a Broadcast().
    /// \return True if a matching active binding was found.
    bool Remove(FDelegateHandle handle)
    {
        GP_ASSERT(IsValid(), "TEventView::Remove - view is not bound to an event source.");
        return m_delegate->Remove(handle);
    }

    /// @brief Removes all method bindings registered via AddMethod() on \p rawInstance.
    /// \return Number of bindings removed.
    Int32 RemoveAll(void* rawInstance)
    {
        GP_ASSERT(IsValid(), "TEventView::RemoveAll - view is not bound to an event source.");
        return m_delegate->RemoveAll(rawInstance);
    }

    GP_NODISCARD bool IsEmpty() const noexcept { return !m_delegate || m_delegate->IsEmpty(); }

    GP_NODISCARD Int32 GetBindingCount() const noexcept { return m_delegate ? m_delegate->GetBindingCount() : 0; }

    GP_NODISCARD bool Contains(FDelegateHandle h) const noexcept { return m_delegate && m_delegate->Contains(h); }
};

/// @brief Owning event source: holds bindings, fires events, and vends
///        subscribe-only TEventView references.
///
/// @par Access control model
/// Access control is enforced through encapsulation, not friendship:
///  - The owner (e.g. FWindowEvents, FGameSession) declares TEvent members
///    as \b private.
///  - External code receives a \c TEventView (via a public accessor or directly
///    from a hub struct) that has no Broadcast() method.
///  - Only the declaring class-which holds the TEvent-can call Broadcast().
///
/// This eliminates the need for \c TEvent<TOwner, Sig> and its \c friend TOwner
/// constraint, which required a concrete owner class and produced the
/// FWindowEventBroadcaster workaround.
///
/// @par Prefer AddScoped() over Add()
/// AddScoped() returns an FScopedDelegateHandle that auto-removes on destruction,
/// preventing the most common event system bug: dangling subscriptions after the
/// subscriber is destroyed.
///
/// @par Example - declaration
/// @code
///   class FGameSession
///   {
///   public:
///       TEventView<void(FPlayerDamagedEvent const&)> OnPlayerDamaged() noexcept
///       {
///           return m_onPlayerDamaged.GetView();
///       }
///
///   private:
///       void DealDamage(FPlayer& target, Int32 amount)
///       {
///           target.ApplyDamage(amount);
///           m_onPlayerDamaged.Broadcast({ &target, amount }); // private: owner only
///       }
///
///       TEvent<void(FPlayerDamagedEvent const&)> m_onPlayerDamaged;
///   };
///
///   // Subscriber:
///   FScopedDelegateHandle h = session.OnPlayerDamaged().AddScoped(
///       [](FPlayerDamagedEvent const& e) { UpdateHUD(e); }
///   );
/// @endcode
///
/// @tparam Signature  Must be void(TArgs...).
template <typename Signature>
class TEvent;

template <typename... TArgs>
class TEvent<void(TArgs...)>
{
private:
    using FDelegate = TMulticastDelegate<void(TArgs...)>;

private:
    FDelegate m_delegate;   //<! Backing multicast delegate; all state lives here.

public:
    TEvent() noexcept = default;

    // Non-copyable: event sources own unique binding lists.
    TEvent(const TEvent&) = delete;
    TEvent& operator=(const TEvent&) = delete;

    // Movable.
    TEvent(TEvent&&) noexcept = default;
    TEvent& operator=(TEvent&&) noexcept = default;

    ~TEvent() noexcept = default;

public:
    /// @brief Subscribes a callable and returns a reference to this view for chaining. Preferred over Add().
    /// @tparam TCallable Any callable matching void(TArgs...).
    template <typename TCallable>
    TEvent& operator+=(TCallable&& callable)
    {
        m_delegate.Add(GP::Forward<TCallable>(callable));
        return *this;
    }

    /// @brief Removes the binding identified by \p handle.
    TEvent& operator-=(FDelegateHandle handle)
    {
        m_delegate.Remove(handle);
        return *this;
    }

    /// @brief Removes all method bindings registered for \p rawInstance.
    TEventSource& operator-=(void* rawInstance)
    {
        m_delegate.RemoveAll(rawInstance);
        return *this;
    }

    /// @brief Broadcasts the event. Equivalent to Broadcast(args...).
    ///        Owner-only by convention; subscribers see TEventView which omits this operator.
    void operator()(TArgs... args) { m_delegate.Broadcast(GP::Forward<TArgs>(args)...); }

public:
    /// @brief Returns a subscribe-only TEventView bound to this source.
    /// The view is valid as long as this TEvent is alive. Return it
    /// from public accessor methods to give subscribers Add/Remove access
    /// without exposing Broadcast().
    GP_NODISCARD GP_FORCEINLINE TEventView<void(TArgs...)> GetView() noexcept
    {
        return TEventView<void(TArgs...)>{ m_delegate };
    }

    /// @brief Subscribes a callable and returns an RAII scoped handle. Preferred.
    template <typename TCallable>
    GP_NODISCARD FScopedDelegateHandle AddScoped(TCallable&& callable)
    {
        FDelegateHandle handle = m_delegate.Add(GP::Forward<TCallable>(callable));

        return FScopedDelegateHandle(
            &m_delegate,
            [](void* src, FDelegateHandle h) noexcept -> bool { return static_cast<FDelegate*>(src)->Remove(h); },
            handle
        );
    }

    /// @brief Subscribes a callable and returns a raw handle. Prefer AddScoped().
    template <typename TCallable>
    GP_NODISCARD FDelegateHandle Add(TCallable&& callable)
    {
        return m_delegate.Add(GP::Forward<TCallable>(callable));
    }

    /// @brief Subscribes a mutable member function. Returns a raw handle.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddMethod(T* instance, void (T::*method)(TArgs...))
    {
        return m_delegate.AddMethod(instance, method);
    }

    /// @brief Subscribes a mutable member function. Returns an RAII scoped handle.
    template <typename T>
    GP_NODISCARD FScopedDelegateHandle AddMethodScoped(T* instance, void (T::*method)(TArgs...))
    {
        FDelegateHandle handle = m_delegate.AddMethod(instance, method);

        return FScopedDelegateHandle(
            &m_delegate,
            [](void* src, FDelegateHandle h) noexcept -> bool { return static_cast<FDelegate*>(src)->Remove(h); },
            handle
        );
    }

    /// @brief Subscribes a const member function. Returns a raw handle.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddMethod(const T* instance, void (T::*method)(TArgs...) const)
    {
        return m_delegate.AddMethod(instance, method);
    }

    /// @brief Subscribes a const member function. Returns an RAII scoped handle.
    template <typename T>
    GP_NODISCARD FScopedDelegateHandle AddMethodScoped(const T* instance, void (T::*method)(TArgs...) const)
    {
        FDelegateHandle handle = m_delegate.AddMethod(instance, method);

        return FScopedDelegateHandle(
            &m_delegate,
            [](void* src, FDelegateHandle h) noexcept -> bool { return static_cast<FDelegate*>(src)->Remove(h); },
            handle
        );
    }

    /// @brief Subscribes a free or static member function. Returns a raw handle.
    GP_NODISCARD FDelegateHandle AddFunction(void (*fn)(TArgs...)) { return m_delegate.AddFunction(fn); }

    bool Remove(FDelegateHandle handle) { return m_delegate.Remove(handle); }

    Int32 RemoveAll(void* rawInstance) { return m_delegate.RemoveAll(rawInstance); }

    /// @brief Removes all bindings. Must not be called during Broadcast().
    void Clear() { m_delegate.Clear(); }

    /// @brief Fires the event, invoking all active bindings in subscription order.
    /// @warning Call only from the class that declares this TEvent as a
    ///          private member. Subscribers receive a TEventView which does not
    ///          expose this method.
    /// Re-entrancy: safe. Removals/additions within handlers are deferred until
    /// the outermost Broadcast() returns.
    /// @param args Arguments forwarded to each binding.
    void Broadcast(TArgs... args) { m_delegate.Broadcast(GP::Forward<TArgs>(args)...); }

    GP_NODISCARD bool IsEmpty() const noexcept { return m_delegate.IsEmpty(); }

    GP_NODISCARD Int32 GetBindingCount() const noexcept { return m_delegate.GetBindingCount(); }

    GP_NODISCARD bool Contains(FDelegateHandle h) const noexcept { return m_delegate.Contains(h); }
};

}   // namespace GP
