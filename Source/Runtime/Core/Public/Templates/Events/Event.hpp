// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/Utility.hpp"
#include "Templates/Events/DelegateHandle.hpp"
#include "Templates/Events/EventEnums.hpp"
#include "Templates/Events/MulticastDelegate.hpp"
#include <array>

namespace GP
{

// Forward declaration
template <typename Signature>
class TEvent;

/// @brief Subscribe-only, non-owning view into a TEvent.
///
/// sizeof(TEventView) == sizeof(void*). Passing or storing a TEventView is
/// as cheap as passing a pointer. No virtual dispatch, no heap allocation.
///
/// Broadcast() is intentionally absent - only the class that owns the
/// TEvent may fire it.
///
/// @par Preferred subscription
/// @code
///   FScopedDelegateHandle h = view.AddScoped(
///       [](FWindowResizedEvent const& e) { ... }
///   );
///   // Subscription removed automatically when h is destroyed.
/// @endcode
///
/// @par Priority subscription
/// @code
///   // Save system must run before the logging system.
///   FScopedDelegateHandle save = view.AddScoped(OnSave, EEventPriority::Critical);
///   FScopedDelegateHandle log  = view.AddScoped(OnLog,  EEventPriority::Low);
/// @endcode
///
/// @par Operators
/// @code
///   view += [](auto& e) { ... };    // fire-and-forget (Normal priority)
///   view -= handle;                 // remove by handle
///   view -= static_cast<void*>(p); // RemoveAll for instance
/// @endcode
///
/// @tparam Signature  Must be void(TArgs...).
template <typename Signature>
class TEventView;

template <typename... TArgs>
class TEventView<void(TArgs...)>
{
private:
    using FSource = TEvent<void(TArgs...)>;

private:
    FSource* m_source{ nullptr };   //<! Non-owning pointer to the event source.

public:
    /// @brief Constructs a view bound to \p source. Prefer TEvent::GetView() over direct construction.
    explicit TEventView(FSource& source) noexcept
        : m_source(&source)
    {}

    constexpr TEventView() noexcept = default;
    TEventView(const TEventView&) noexcept = default;
    TEventView(TEventView&&) noexcept = default;
    TEventView& operator=(const TEventView&) noexcept = default;
    TEventView& operator=(TEventView&&) noexcept = default;
    ~TEventView() noexcept = default;

public:
    /// @brief Fire-and-forget subscription at Normal priority. Handle is discarded.
    ///        Use AddScoped() when the subscriber has a bounded lifetime.
    template <typename TCallable>
    TEventView& operator+=(TCallable&& callable)
    {
        GP_ASSERT(IsValid(), "TEventView::operator+= - view is not bound to a source.");
        GP_UNUSED(m_source->Add(GP::Forward<TCallable>(callable), EEventPriority::Normal));
        return *this;
    }

    /// @brief Removes the binding identified by \p handle.
    TEventView& operator-=(FDelegateHandle handle)
    {
        GP_ASSERT(IsValid(), "TEventView::operator-= - view is not bound to a source.");
        m_source->Remove(handle);
        return *this;
    }

    /// @brief Removes all method bindings registered for \p rawInstance.
    TEventView& operator-=(void* rawInstance)
    {
        GP_ASSERT(IsValid(), "TEventView::operator-= - view is not bound to a source.");
        m_source->RemoveAll(rawInstance);
        return *this;
    }

public:
    GP_NODISCARD GP_FORCEINLINE bool IsValid() const noexcept { return m_source != nullptr; }

    /// @brief Subscribes a callable; auto-removes when the returned handle is destroyed.
    template <typename TCallable>
    GP_NODISCARD FScopedDelegateHandle AddScoped(TCallable&& callable, EEventPriority priority = EEventPriority::Normal)
    {
        GP_ASSERT(IsValid(), "TEventView::AddScoped - view is not bound to a source.");
        return m_source->AddScoped(std::forward<TCallable>(callable), priority);
    }

    /// @brief Subscribes a mutable member function; auto-removes when the returned handle is destroyed.
    template <typename T>
    GP_NODISCARD FScopedDelegateHandle
        AddMethodScoped(T* instance, void (T::*method)(TArgs...), EEventPriority priority = EEventPriority::Normal)
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethodScoped - view is not bound to a source.");
        return m_source->AddMethodScoped(instance, method, priority);
    }

    /// @brief Subscribes a const member function; auto-removes when the returned handle is destroyed.
    template <typename T>
    GP_NODISCARD FScopedDelegateHandle AddMethodScoped(
        const T* instance, void (T::*method)(TArgs...) const, EEventPriority priority = EEventPriority::Normal
    )
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethodScoped - view is not bound to a source.");
        return m_source->AddMethodScoped(instance, method, priority);
    }

    /// @brief Subscribes a callable. Prefer AddScoped() for bounded-lifetime subscribers.
    template <typename TCallable>
    GP_NODISCARD FDelegateHandle Add(TCallable&& callable, EEventPriority priority = EEventPriority::Normal)
    {
        GP_ASSERT(IsValid(), "TEventView::Add - view is not bound to a source.");
        return m_source->Add(std::forward<TCallable>(callable), priority);
    }

    /// @brief Subscribes a mutable member function.
    template <typename T>
    GP_NODISCARD FDelegateHandle
        AddMethod(T* instance, void (T::*method)(TArgs...), EEventPriority priority = EEventPriority::Normal)
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethod - view is not bound to a source.");
        return m_source->AddMethod(instance, method, priority);
    }

    /// @brief Subscribes a const member function.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddMethod(
        const T* instance, void (T::*method)(TArgs...) const, EEventPriority priority = EEventPriority::Normal
    )
    {
        GP_ASSERT(IsValid(), "TEventView::AddMethod - view is not bound to a source.");
        return m_source->AddMethod(instance, method, priority);
    }

    /// @brief Subscribes a free or static member function.
    GP_NODISCARD FDelegateHandle AddFunction(void (*fn)(TArgs...), EEventPriority priority = EEventPriority::Normal)
    {
        GP_ASSERT(IsValid(), "TEventView::AddFunction - view is not bound to a source.");
        return m_source->AddFunction(fn, priority);
    }

    bool Remove(FDelegateHandle handle)
    {
        GP_ASSERT(IsValid(), "TEventView::Remove");
        return m_source->Remove(handle);
    }

    Int32 RemoveAll(void* rawInstance)
    {
        GP_ASSERT(IsValid(), "TEventView::RemoveAll");
        return m_source->RemoveAll(rawInstance);
    }

    GP_NODISCARD bool IsEmpty() const noexcept { return !m_source || m_source->IsEmpty(); }

    GP_NODISCARD Int32 GetBindingCount() const noexcept { return m_source ? m_source->GetBindingCount() : 0; }

    GP_NODISCARD bool Contains(FDelegateHandle h) const noexcept { return m_source && m_source->Contains(h); }
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
    // TODO: Change the std::array to our custom TStaticArray when it completed
    std::array<FDelegate, kEventPriorityCount> m_buckets;   //<! One delegate bucket per priority level.

public:
    TEvent() noexcept = default;

    TEvent(const TEvent&) = delete;
    TEvent& operator=(const TEvent&) = delete;

    TEvent(TEvent&&) noexcept = default;
    TEvent& operator=(TEvent&&) noexcept = default;

    ~TEvent() noexcept = default;

public:
    /// @brief Fire-and-forget subscription at Normal priority. Handle is discarded.
    ///        Use AddScoped() when the subscriber has a bounded lifetime.
    template <typename TCallable>
    TEvent& operator+=(TCallable&& callable)
    {
        Bucket(EEventPriority::Normal).Add(std::forward<TCallable>(callable));
        return *this;
    }

    /// @brief Removes the binding identified by \p handle.
    TEvent& operator-=(FDelegateHandle handle)
    {
        Remove(handle);
        return *this;
    }

    /// @brief Removes all method bindings registered for \p rawInstance.
    TEvent& operator-=(void* rawInstance)
    {
        RemoveAll(rawInstance);
        return *this;
    }

    /// @brief Broadcasts via operator() for ergonomic owner-side syntax.
    void operator()(TArgs... args) { Broadcast(args...); }

public:
    GP_NODISCARD GP_FORCEINLINE TEventView<void(TArgs...)> GetView() noexcept
    {
        return TEventView<void(TArgs...)>{ *this };
    }

    template <typename TCallable>
    GP_NODISCARD FScopedDelegateHandle AddScoped(TCallable&& callable, EEventPriority priority = EEventPriority::Normal)
    {
        FDelegateHandle handle = Bucket(priority).Add(GP::Forward<TCallable>(callable));
        return MakeScopedHandle(handle);
    }

    template <typename T>
    GP_NODISCARD FScopedDelegateHandle
        AddMethodScoped(T* instance, void (T::*method)(TArgs...), EEventPriority priority = EEventPriority::Normal)
    {
        FDelegateHandle handle = Bucket(priority).AddMethod(instance, method);
        return MakeScopedHandle(handle);
    }

    template <typename T>
    GP_NODISCARD FScopedDelegateHandle AddMethodScoped(
        const T* instance, void (T::*method)(TArgs...) const, EEventPriority priority = EEventPriority::Normal
    )
    {
        FDelegateHandle handle = Bucket(priority).AddMethod(instance, method);
        return MakeScopedHandle(handle);
    }

    template <typename TCallable>
    GP_NODISCARD FDelegateHandle Add(TCallable&& callable, EEventPriority priority = EEventPriority::Normal)
    {
        return Bucket(priority).Add(GP::Forward<TCallable>(callable));
    }

    template <typename T>
    GP_NODISCARD FDelegateHandle
        AddMethod(T* instance, void (T::*method)(TArgs...), EEventPriority priority = EEventPriority::Normal)
    {
        return Bucket(priority).AddMethod(instance, method);
    }

    template <typename T>
    GP_NODISCARD FDelegateHandle AddMethod(
        const T* instance, void (T::*method)(TArgs...) const, EEventPriority priority = EEventPriority::Normal
    )
    {
        return Bucket(priority).AddMethod(instance, method);
    }

    GP_NODISCARD FDelegateHandle AddFunction(void (*fn)(TArgs...), EEventPriority priority = EEventPriority::Normal)
    {
        return Bucket(priority).AddFunction(fn);
    }

    bool Remove(FDelegateHandle handle)
    {
        for (FDelegate& bucket: m_buckets)
        {
            if (bucket.Remove(handle)) { return true; }
        }
        return false;
    }

    Int32 RemoveAll(void* rawInstance)
    {
        Int32 total = 0;
        for (FDelegate& bucket: m_buckets) { total += bucket.RemoveAll(rawInstance); }
        return total;
    }

    void Clear()
    {
        for (FDelegate& bucket: m_buckets) { bucket.Clear(); }
    }

    /// @brief Invokes all listeners in priority order.
    ///        Owner-only by convention; external code receives TEventView which omits this.
    void Broadcast(TArgs... args)
    {
        // All event signatures are void(FXxx const&) - passing by const ref is safe
        // across all bucket iterations; no double-forward issue.
        for (FDelegate& bucket: m_buckets) { bucket.Broadcast(args...); }
    }

    GP_NODISCARD bool IsEmpty() const noexcept
    {
        for (const FDelegate& bucket: m_buckets)
        {
            if (!bucket.IsEmpty()) { return false; }
        }
        return true;
    }

    GP_NODISCARD Int32 GetBindingCount() const noexcept
    {
        Int32 total = 0;
        for (const FDelegate& bucket: m_buckets) { total += bucket.GetBindingCount(); }
        return total;
    }

    GP_NODISCARD bool Contains(FDelegateHandle handle) const noexcept
    {
        for (const FDelegate& bucket: m_buckets)
        {
            if (bucket.Contains(handle)) { return true; }
        }
        return false;
    }

private:
    GP_FORCEINLINE FDelegate& Bucket(EEventPriority priority) noexcept { return m_buckets[ToIndex(priority)]; }

    GP_NODISCARD FScopedDelegateHandle MakeScopedHandle(FDelegateHandle handle)
    {
        return FScopedDelegateHandle(
            this,
            [](void* src, FDelegateHandle h) noexcept -> bool { return static_cast<TEvent*>(src)->Remove(h); },
            handle
        );
    }
};

}   // namespace GP
