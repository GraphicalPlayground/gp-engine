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

private:
    ///
    /// @section Helpers
    ///

    template <typename TCallable>
    FDelegateHandle PushEntry(TCallable&& callable, void* ownerPtr)
    {
        FDelegateHandle handle = FDelegateHandle::Generate();

        FEntry& entry = m_entries.emplace_back();
        entry.handle = handle;
        entry.ownerPtr = ownerPtr;
        entry.binding.Bind(std::forward<TCallable>(callable));

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
        if (&target != &m_entries.back()) { target = GP::Move(m_entries.back()); }
        m_entries.pop_back();
    }

    /// @brief Called after the outermost Broadcast() to physically erase deferred removals.
    void SweepPendingRemovals()
    {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(), [](const FEntry& e) { return e.pendingRemoval; }),
            m_entries.end()
        );
    }
};

}   // namespace GP
