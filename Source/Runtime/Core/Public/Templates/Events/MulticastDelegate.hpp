// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Sequential/Array.hpp"
#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Core/Utility.hpp"
#include "Templates/Events/DelegateHandle.hpp"
#include "Templates/Events/DelegateInternals.hpp"

namespace GP
{

/// @brief Multicast delegate: stores zero or more void-returning bindings and
///        invokes all of them in subscription order on Broadcast().
///
/// Each subscription returns an FDelegateHandle used to remove the binding at
/// any time, including from within a Broadcast() handler (re-entrant removal is
/// safe; the binding is marked and swept after the outermost Broadcast() returns).
///
/// Bindings added during a Broadcast() are NOT invoked in that same broadcast
/// cycle; they become active for the next call.
///
/// @note  Only void-returning bindings are supported. Use TDelegate (single-cast)
///        when a return value is required.
///
/// @note  Not thread-safe by default. All Add / Remove / Broadcast calls must
///        happen on the same thread, or a mutex must be provided externally.
///
/// Example:
/// @code
///   TMulticastDelegate<void(Int32, FString const&)> onPlayerDamaged;
///
///   FDelegateHandle hud = onPlayerDamaged.Add([](Int32 dmg, FString const& src) { ... });
///   FDelegateHandle sfx = onPlayerDamaged.AddMethod(&audioMgr, &FAudioManager::OnDamage);
///
///   onPlayerDamaged.Broadcast(25, FString{"Explosion"});
///   onPlayerDamaged.Remove(hud);
/// @endcode
///
/// @param Signature Full function signature; must be \c void(TArgs...).
template <typename Signature>
class TMulticastDelegate;

template <typename... TArgs>
class TMulticastDelegate<void(TArgs...)>
{
private:
    using FBinding = Detail::TBinding<void, TArgs...>;

private:
    /// @brief Internal storage for each subscribed binding, including its handle and owner pointer.
    struct FEntry
    {
        FBinding binding;               ///< Type-erased callable (SBO storage).
        FDelegateHandle handle;         ///< Unique subscription handle.
        void* ownerPtr{ nullptr };      ///< Raw instance pointer, for RemoveAll().
        bool pendingRemoval{ false };   ///< True when removal is deferred during Broadcast.
    };

private:
    TArray<FEntry> m_entries;      //<! All registered entries (active + pending removal).
    Int32 m_broadcastDepth{ 0 };   //<! Incremented per active Broadcast() stack frame.

public:
    ///
    /// @section Constructors / Destructor
    ///

    TMulticastDelegate() noexcept = default;

    TMulticastDelegate(const TMulticastDelegate&) = default;
    TMulticastDelegate(TMulticastDelegate&&) noexcept = default;
    TMulticastDelegate& operator=(const TMulticastDelegate&) = default;
    TMulticastDelegate& operator=(TMulticastDelegate&&) noexcept = default;

    ~TMulticastDelegate() noexcept = default;

public:
    /// @brief Subscribes a free or static member function.
    /// @param fn Non-null function pointer.
    /// @return Subscription handle.
    FDelegateHandle AddFunction(void (*fn)(TArgs...))
    {
        GP_ASSERT(fn != nullptr, "TMulticastDelegate::AddFunction - function pointer must not be null.");
        return PushEntry(fn, nullptr);
    }

    /// \brief Subscribes a mutable member function on a specific object instance.
    ///
    /// \tparam T         Instance type.
    /// \param  instance  Non-null pointer to the listening object.
    /// \param  method    Pointer to the member function.
    /// \return           Subscription handle.
    template <typename T>
    FDelegateHandle AddMethod(T* instance, void (T::*method)(TArgs...))
    {
        GP_ASSERT(instance != nullptr, "TMulticastDelegate::AddMethod — instance pointer must not be null.");
        GP_ASSERT(method != nullptr, "TMulticastDelegate::AddMethod — method pointer must not be null.");

        using FWrapper = Detail::TMemberBinding<T, void (T::*)(TArgs...), void, TArgs...>;
        return PushEntry(FWrapper{ instance, method }, static_cast<void*>(instance));
    }

    /// @brief Subscribes a const-qualified member function on a specific object instance.
    /// @tparam T Instance type.
    /// @param instance Non-null const pointer to the listening object.
    /// @param method Pointer to the const member function.
    /// @return Subscription handle.
    template <typename T>
    FDelegateHandle AddMethod(const T* instance, void (T::*method)(TArgs...) const)
    {
        GP_ASSERT(instance != nullptr, "TMulticastDelegate::AddMethod - instance pointer must not be null.");
        GP_ASSERT(method != nullptr, "TMulticastDelegate::AddMethod - method pointer must not be null.");

        using FWrapper = Detail::TConstMemberBinding<T, void (T::*)(TArgs...) const, void, TArgs...>;
        // const_cast is safe here: the pointer is stored for identity comparison only, never dereferenced mutably.
        return PushEntry(FWrapper{ instance, method }, const_cast<void*>(static_cast<const void*>(instance)));
    }

    /// @brief Subscribes any callable: lambda, functor, std::function, etc. This is the primary general-purpose
    ///        overload.
    /// @tparam TCallable Any callable type matching \c void(TArgs...).
    /// @param callable Callable to copy/move into the delegate.
    /// @return Subscription handle.
    template <typename TCallable>
    FDelegateHandle Add(TCallable&& callable)
    {
        return PushEntry(GP::Forward<TCallable>(callable), nullptr);
    }

    /// @brief Removes the binding identified by \p handle.
    ///        Safe to call re-entrantly from within a Broadcast() handler. If inside a
    ///        broadcast, removal is deferred until the outermost Broadcast() returns.
    /// @param handle Handle returned by a previous Add*() call.
    /// @return True if a matching active binding was found.
    bool Remove(FDelegateHandle handle)
    {
        if (!handle.IsValid()) { return false; }

        for (FEntry& entry: m_entries)
        {
            if (entry.handle != handle || entry.pendingRemoval) { continue; }

            MarkOrErase(entry);
            return true;
        }
        return false;
    }

    /// @brief Removes all bindings registered via AddMethod() on \p rawInstance.
    ///        Equivalent to calling Remove() for every handle associated with \p rawInstance.
    ///        O(N), prefers Remove(FDelegateHandle) when binding count is large.
    /// @param rawInstance Pointer to the object whose method bindings should be removed.
    /// @return Number of bindings removed (or marked for deferred removal).
    Int32 RemoveAll(void* rawInstance)
    {
        GP_ASSERT(rawInstance != nullptr, "TMulticastDelegate::RemoveAll - rawInstance must not be null.");

        Int32 removed = 0;
        for (FEntry& entry: m_entries)
        {
            if (!entry.pendingRemoval && entry.ownerPtr == rawInstance)
            {
                MarkOrErase(entry);
                ++removed;
            }
        }
        return removed;
    }

    /// @brief Returns true if no active bindings exist.
    GP_NODISCARD bool IsEmpty() const noexcept
    {
        for (const FEntry& entry: m_entries)
        {
            if (!entry.pendingRemoval) { return false; }
        }
        return true;
    }

    /// @brief Returns the number of active (non-pending-removal) bindings.
    GP_NODISCARD Int32 GetBindingCount() const noexcept
    {
        Int32 count = 0;
        for (const FEntry& entry: m_entries)
        {
            if (!entry.pendingRemoval) { ++count; }
        }
        return count;
    }

    /// @brief Returns true if the given handle refers to an active binding.
    GP_NODISCARD bool Contains(FDelegateHandle handle) const noexcept
    {
        if (!handle.IsValid()) { return false; }

        for (const FEntry& entry: m_entries)
        {
            if (entry.handle == handle && !entry.pendingRemoval) { return true; }
        }
        return false;
    }

    /// @brief Removes all bindings immediately.
    /// @warning Must NOT be called from within a Broadcast() handler.
    void Clear()
    {
        GP_ASSERT(
            m_broadcastDepth == 0, "TMulticastDelegate::Clear - must not be called from within a Broadcast() handler."
        );
        m_entries.Clear();
    }

    /// @brief Invokes all active bindings in subscription order.
    ///        Re-entrancy contract:
    ///            - Handlers may safely call Add*() or Remove() / RemoveAll() on this delegate.
    ///            - Removals during broadcast are deferred; applied after the outermost call returns.
    ///            - Additions during broadcast are visible in subsequent broadcasts, not this one.
    /// @param args  Arguments forwarded to each binding.
    void Broadcast(TArgs... args)
    {
        ++m_broadcastDepth;

        // Capture the current count so any entries appended during iteration are excluded.
        const SizeT snapshotCount = m_entries.Size();

        for (SizeT i = 0; i < snapshotCount; ++i)
        {
            m_entries[i].binding.Invoke(GP::Forward<TArgs>(args)...);
        }

        --m_broadcastDepth;

        if (m_broadcastDepth == 0) { SweepPendingRemovals(); }
    }

private:
    ///
    /// @section Helpers
    ///

    template <typename TCallable>
    FDelegateHandle PushEntry(TCallable&& callable, void* ownerPtr)
    {
        FDelegateHandle handle = FDelegateHandle::Generate();

        FEntry& entry = m_entries.EmplaceBack();
        entry.handle = handle;
        entry.ownerPtr = ownerPtr;
        entry.binding.Bind(GP::Forward<TCallable>(callable));

        return handle;
    }

    /// @brief Immediately erases or defers removal depending on broadcast depth.
    void MarkOrErase(FEntry& entry)
    {
        if (m_broadcastDepth > 0) { entry.pendingRemoval = true; }
        else { SwapErase(entry); }
    }

    /// @brief O(1) unordered removal: swap target with last element, then pop.
    void SwapErase(FEntry& target)
    {
        if (&target != &m_entries.Back()) { target = GP::Move(m_entries.Back()); }
        m_entries.PopBack();
    }

    /// @brief Called after the outermost Broadcast() to physically erase deferred removals.
    void SweepPendingRemovals()
    {
        SizeT write = 0;
        const SizeT total = m_entries.Size();
        for (SizeT i = 0; i < total; ++i)
        {
            if (!m_entries[i].pendingRemoval)
            {
                if (write != i) { m_entries[write] = GP::Move(m_entries[i]); }
                ++write;
            }
        }
        m_entries.Resize(write);
    }
};

}   // namespace GP
