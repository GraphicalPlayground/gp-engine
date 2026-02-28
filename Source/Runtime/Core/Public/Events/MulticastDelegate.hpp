// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Events/DelegateHandle.hpp"
#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <type_traits>
#include <vector>

namespace GP::Events
{

/// \brief Primary template — intentionally left undefined.
/// \note Only the void-returning partial specialization TMulticastDelegate<void(Args...)> is provided. Multicast
/// dispatch does not support return value aggregation.
template <typename Signature>
class TMulticastDelegate;

/// \brief A one-to-many, type-safe event dispatcher.
///
/// TMulticastDelegate allows any number of callables (free functions, lambdas, member functions) to be registered and
/// subsequently invoked in registration order via Broadcast().
///
/// ### Thread Safety
/// TMulticastDelegate itself is NOT thread-safe. External synchronization is required when Add(), Remove(), or
/// Broadcast() may be called concurrently from multiple threads. For a thread-safe variant, use a mutex-guarded wrapper
/// or the TThreadSafeEventBus.
///
/// ### Re-Entrancy
/// It is safe to call Add() or Remove() from within a Broadcast() callback. Modifications are deferred until the
/// outermost Broadcast() returns, preventing iterator invalidation.
///
/// \par Usage Example
/// \code
/// TMulticastDelegate<void(const FWindowResizedEvent&)> OnResized;
///
/// FDelegateHandle h = OnResized.AddLambda([](const FWindowResizedEvent& e) {
///     GP_LOG("Window resized to {}x{}", e.width, e.height);
/// });
///
/// OnResized.Broadcast(event);   // invokes all registered callables
/// OnResized.Remove(h);          // safe to call at any time, including during Broadcast
/// \endcode
///
/// \tparam Args Parameter types forwarded to each registered callable.
template <typename... Args>
class TMulticastDelegate<void(Args...)>
{
public:
    using FSignature = void(Args...);
    using FCallable = std::move_only_function<FSignature>;

private:
    /// \brief Internal struct representing a single binding of a callable to the delegate, along with its handle and
    /// pending removal state.
    struct FBinding
    {
        FDelegateHandle handle;        //<! Unique identifier for this binding, used for removal.
        FCallable callable;            //<! The callable to invoke for this binding.
        bool pendingRemoval = false;   //<! Flag indicating whether this binding is pending removal.
    };

private:
    std::vector<FBinding> m_bindings;           //<! Active bindings currently registered to this delegate.
    std::vector<FBinding> m_pendingAdditions;   //<! Bindings added during a Broadcast() that are pending addition.
    UInt32 m_broadcastDepth = 0;                //<! Tracks the depth of nested calls to manage deferred modifications.
    bool m_hasPendingRemovals = false;          //<! Flag indicating whether there are any bindings pending removal.
    bool m_hasPendingAdditions = false;         //<! Flag indicating whether there are any bindings pending addition.

public:
    /// \brief Default constructor.
    TMulticastDelegate() = default;

    /// \brief Default destructor.
    ~TMulticastDelegate() = default;

    /// \brief Move constructor. Transfers ownership of all bindings to the new delegate.
    TMulticastDelegate(TMulticastDelegate&&) = default;

    /// \brief Copy constructor is deleted to prevent accidental copying of delegates.
    TMulticastDelegate(const TMulticastDelegate&) = delete;

public:
    /// \brief Move assignment operator. Transfers ownership of all bindings to the new delegate, leaving the old
    /// delegate in a valid but empty state.
    TMulticastDelegate& operator=(TMulticastDelegate&&) = default;

    /// \brief Copy assignment operator is deleted to prevent accidental copying of delegates.
    TMulticastDelegate& operator=(const TMulticastDelegate&) = delete;

public:
    /// \brief Registers a free function pointer as a listener.
    /// \param func A non-null pointer to a free function matching the delegate's signature.
    /// \return A valid FDelegateHandle that can be passed to Remove() to unregister the listener.
    GP_NODISCARD FDelegateHandle AddStatic(void (*func)(Args...))
    {
        GP_ASSERT(func != nullptr);
        return AddCallable(FCallable{ func });
    }

    /// \brief Registers any callable (lambda, functor, std::bind result) as a listener.
    /// \tparam TCallable Any callable type invocable with (Args...).
    /// \param callable The callable to register. Moved into internal storage.
    /// \return A valid FDelegateHandle that can be passed to Remove() to unregister the listener.
    template <typename TCallable>
    requires std::invocable<TCallable, Args...> GP_NODISCARD FDelegateHandle AddLambda(TCallable&& callable)
    {
        return AddCallable(FCallable{ std::forward<TCallable>(callable) });
    }

    /// \brief Registers a non-const member function on a raw object pointer.
    /// \tparam T The class that owns the member function.
    /// \param object A non-null pointer to the object. Lifetime must exceed the binding.
    /// \param func A non-null pointer to a non-const member function.
    /// \return A valid FDelegateHandle for later removal.
    /// \warning The delegate does NOT manage the object's lifetime. Ensure the object is alive for as long as the
    /// binding is registered.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddMember(T* object, void (T::*func)(Args...))
    {
        GP_ASSERT(object != nullptr);
        GP_ASSERT(func != nullptr);
        return AddCallable(FCallable{ [object, func](Args&&... args) { (object->*func)(std::forward<Args>(args)...); } }
        );
    }

    /// \brief Registers a const member function on a raw object pointer.
    /// \tparam T The class that owns the member function.
    /// \param object A non-null pointer to the const object. Lifetime must exceed the binding.
    /// \param func A non-null pointer to a const member function.
    /// \return A valid FDelegateHandle for later removal.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddMember(const T* object, void (T::*func)(Args...) const)
    {
        GP_ASSERT(object != nullptr);
        GP_ASSERT(func != nullptr);
        return AddCallable(FCallable{ [object, func](Args&&... args) { (object->*func)(std::forward<Args>(args)...); } }
        );
    }

    /// \brief Registers a non-const member function on a shared_ptr-owned object, holding only a weak reference.
    /// If the tracked object has been destroyed when Broadcast() fires, the invocation is silently skipped.
    /// The binding remains in the list; call Remove() with the returned handle to clean it up explicitly.
    /// \tparam T The class that owns the member function.
    /// \param sp A non-null shared_ptr to the target object.
    /// \param func A non-null pointer to a non-const member function.
    /// \return A valid FDelegateHandle for later removal.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddWeakSP(const std::shared_ptr<T>& sp, void (T::*func)(Args...))
    {
        GP_ASSERT(sp != nullptr);
        GP_ASSERT(func != nullptr);
        std::weak_ptr<T> wp = sp;
        return AddCallable(FCallable{ [wp, func](Args&&... args)
                                      {
                                          if (auto locked = wp.lock())
                                          {
                                              (locked.get()->*func)(std::forward<Args>(args)...);
                                          }
                                      } });
    }

    /// \brief Registers a const member function on a shared_ptr-owned object, holding only a weak reference.
    /// If the tracked object has been destroyed when Broadcast() fires, the invocation is silently skipped.
    /// \tparam T The class that owns the member function.
    /// \param sp A non-null shared_ptr to the target object.
    /// \param func A non-null pointer to a const member function.
    /// \return A valid FDelegateHandle for later removal.
    template <typename T>
    GP_NODISCARD FDelegateHandle AddWeakSP(const std::shared_ptr<T>& sp, void (T::*func)(Args...) const)
    {
        GP_ASSERT(sp != nullptr);
        GP_ASSERT(func != nullptr);
        std::weak_ptr<T> wp = sp;
        return AddCallable(FCallable{ [wp, func](Args&&... args)
                                      {
                                          if (auto locked = wp.lock())
                                          {
                                              (locked.get()->*func)(std::forward<Args>(args)...);
                                          }
                                      } });
    }

    /// \brief Removes the binding identified by the given handle. If called during a Broadcast(), the removal is
    /// deferred until the outermost Broadcast() completes. The callable will NOT be invoked for the current broadcast
    /// even if the removal is deferred.
    /// \param handle The handle returned by a previous Add* call.
    /// \return True if a binding with the given handle was found and removed, false otherwise.
    bool Remove(FDelegateHandle handle)
    {
        if (!handle.IsValid()) { return false; }

        for (FBinding& binding: m_bindings)
        {
            if (binding.handle == handle)
            {
                // Flag immediately so it is not invoked in the current broadcast pass.
                binding.pendingRemoval = true;
                m_hasPendingRemovals = true;

                if (m_broadcastDepth == 0) { FlushPendingRemovals(); }

                return true;
            }
        }

        // The binding might be in the pending-addition list if Add() was called during a broadcast.
        if (m_hasPendingAdditions)
        {
            const auto it = std::ranges::find(m_pendingAdditions, handle, &FBinding::handle);
            if (it != m_pendingAdditions.end())
            {
                m_pendingAdditions.erase(it);
                return true;
            }
        }

        return false;
    }

    /// \brief Removes all registered bindings. If called during a Broadcast(), all removals are deferred until the
    /// outermost Broadcast() completes.
    void RemoveAll()
    {
        if (m_broadcastDepth > 0)
        {
            for (FBinding& binding: m_bindings) { binding.pendingRemoval = true; }

            m_pendingAdditions.clear();
            m_hasPendingRemovals = true;
            m_hasPendingAdditions = false;
        }
        else
        {
            m_bindings.clear();
            m_pendingAdditions.clear();
            m_hasPendingRemovals = false;
            m_hasPendingAdditions = false;
        }
    }

    /// \brief Invokes all registered callables in registration order. Re-entrancy is fully supported: Add() and
    /// Remove() calls made by listeners during a Broadcast() are deferred and applied once the outermost Broadcast()
    /// returns.
    /// \param args Arguments forwarded to each registered callable.
    void Broadcast(Args... args)
    {
        if (m_bindings.empty() && m_pendingAdditions.empty()) { return; }

        ++m_broadcastDepth;

        // Capture the current size to avoid iterating over bindings added during this pass.
        // Newly added bindings (deferred) will be merged after the broadcast completes.
        const UInt32 bindingCount = static_cast<UInt32>(m_bindings.size());
        for (UInt32 i = 0; i < bindingCount; ++i)
        {
            FBinding& binding = m_bindings[i];
            if (!binding.pendingRemoval) { binding.callable(args...); }
        }

        --m_broadcastDepth;

        // Only the outermost Broadcast call processes deferred modifications.
        if (m_broadcastDepth == 0) { FlushPendingOperations(); }
    }

    /// \brief Returns true if at least one callable is currently registered (and not pending removal).
    /// \return True if there is at least one active binding, false otherwise.
    GP_NODISCARD bool IsBound() const noexcept
    {
        return std::ranges::any_of(m_bindings, [](const FBinding& b) { return !b.pendingRemoval; });
    }

    /// \brief Returns the number of currently active (non-pending-removal) bindings.
    /// \return The count of active bindings.
    GP_NODISCARD UInt32 GetBindingCount() const noexcept
    {
        return static_cast<UInt32>(
            std::ranges::count_if(m_bindings, [](const FBinding& b) { return !b.pendingRemoval; })
        );
    }

    /// \brief Returns true if the delegate has no active bindings.
    /// \return True if there are no active bindings, false otherwise.
    GP_NODISCARD bool IsEmpty() const noexcept { return !IsBound(); }

    /// \brief Returns true if a Broadcast() is currently in progress on this delegate.
    /// \return True if currently broadcasting, false otherwise.
    GP_NODISCARD bool IsBroadcasting() const noexcept { return m_broadcastDepth > 0; }

    /// \brief Returns true if a binding identified by the given handle is currently registered and not pending removal.
    /// Bindings added during an ongoing Broadcast() (i.e., pending additions) are also checked.
    /// \param handle The handle to look up.
    /// \return True if the handle refers to an active or pending-addition binding, false otherwise.
    GP_NODISCARD bool Contains(FDelegateHandle handle) const noexcept
    {
        if (!handle.IsValid()) { return false; }

        const bool inActive = std::ranges::any_of(
            m_bindings, [handle](const FBinding& b) { return b.handle == handle && !b.pendingRemoval; }
        );
        if (inActive) { return true; }

        if (m_hasPendingAdditions)
        {
            return std::ranges::any_of(m_pendingAdditions, [handle](const FBinding& b) { return b.handle == handle; });
        }

        return false;
    }

private:
    /// \brief Core registration path used by all public Add* methods.
    /// \note If a Broadcast is in progress, the new binding is deferred until it completes.
    /// \param callable The callable to register. Moved into internal storage.
    /// \return A valid FDelegateHandle for later removal.
    FDelegateHandle AddCallable(FCallable&& callable)
    {
        const FDelegateHandle handle = FDelegateHandle::Generate();

        if (m_broadcastDepth > 0)
        {
            // Deferred: will be merged into m_bindings once the broadcast unwinds.
            m_pendingAdditions.push_back({ handle, std::move(callable) });
            m_hasPendingAdditions = true;
        }
        else { m_bindings.push_back({ handle, std::move(callable) }); }

        return handle;
    }

    /// \brief Erases bindings flagged for removal. Called when m_broadcastDepth == 0.
    void FlushPendingRemovals()
    {
        if (!m_hasPendingRemovals) { return; }

        std::erase_if(m_bindings, [](const FBinding& b) { return b.pendingRemoval; });
        m_hasPendingRemovals = false;
    }

    /// \brief Merges pending additions and flushes removals. Called when broadcast depth drops to 0.
    void FlushPendingOperations()
    {
        FlushPendingRemovals();

        if (m_hasPendingAdditions)
        {
            m_bindings.insert(
                m_bindings.end(),
                std::make_move_iterator(m_pendingAdditions.begin()),
                std::make_move_iterator(m_pendingAdditions.end())
            );
            m_pendingAdditions.clear();
            m_hasPendingAdditions = false;
        }
    }
};

}   // namespace GP::Events
