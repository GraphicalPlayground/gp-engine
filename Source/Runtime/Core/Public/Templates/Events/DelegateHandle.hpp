// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Memory/AtomicOps.hpp"

namespace GP
{

/// @brief Opaque, value-semantic handle identifying a single binding within a multicast delegate or event.
///
/// Returned by TMulticastDelegate::Add() and TEvent::Add(). Pass to Remove() or TEvent::Remove() to unsubscribe.
/// Handles must be manually invalidated or stored by the subscriber; the delegate does NOT invalidate them on
/// destruction.
///
/// @warning A valid handle does not guarantee the underlying binding still exists. Always call Remove() before either
///          the delegate or the subscriber is destroyed, whichever happens first.
struct FDelegateHandle
{
public:
    using FId = UInt64;

public:
    static constexpr FId kInvalidId = 0ULL;

private:
    FId m_id{ kInvalidId };

public:
    ///
    /// @section Constructors / Destructor
    ///

    constexpr FDelegateHandle() noexcept = default;
    constexpr FDelegateHandle(const FDelegateHandle&) noexcept = default;
    constexpr FDelegateHandle(FDelegateHandle&&) noexcept = default;
    ~FDelegateHandle() noexcept = default;

private:
    explicit constexpr FDelegateHandle(FId id) noexcept
        : m_id(id)
    {}

public:
    ///
    /// @section Operators
    ///

    constexpr FDelegateHandle& operator=(const FDelegateHandle&) noexcept = default;
    constexpr FDelegateHandle& operator=(FDelegateHandle&&) noexcept = default;

    GP_NODISCARD constexpr bool operator==(const FDelegateHandle&) const noexcept = default;
    GP_NODISCARD constexpr bool operator!=(const FDelegateHandle&) const noexcept = default;

public:
    /// @brief Returns true if this handle was generated and has not been invalidated.
    /// @note This does NOT confirm the binding still exists inside a delegate.
    GP_NODISCARD constexpr bool IsValid() const noexcept { return m_id != kInvalidId; }

    /// @brief Resets this handle to the invalid state without affecting the delegate.
    constexpr void Invalidate() noexcept { m_id = kInvalidId; }

public:
    /// @brief Generates a globally unique, monotonically-increasing handle.
    ///
    /// Thread-safe. Uses relaxed atomic increment; IDs are unique but ordering
    /// across threads is not guaranteed. ID 0 is permanently reserved as invalid.
    ///
    /// @return A new FDelegateHandle with a unique non-zero ID.
    GP_NODISCARD static FDelegateHandle Generate() noexcept
    {
        static TAtomic<FId> s_counter{ 1ULL };
        return FDelegateHandle{ s_counter.fetch_add(1ULL, std::memory_order_relaxed) };
    }
};

/// @brief RAII wrapper around FDelegateHandle that automatically removes the
///        subscription when it goes out of scope.
///
/// This is the preferred way to hold event subscriptions. Using a raw
/// FDelegateHandle requires a manual Remove() call on every exit path; any
/// missed call silently leaves a dangling subscriber that will invoke freed
/// memory or a destroyed object during the next Broadcast().
///
/// @par Cost
/// Internally stores one raw pointer, one stateless function pointer (generated
/// at compile time, never heap-allocated), and one FDelegateHandle (UInt64).
/// Total size is 24 bytes on LP64 — no std::function, no heap allocation.
///
/// @par Non-copyable, movable
/// A subscription has exactly one owner. Move into a member variable or an
/// std::vector to keep the subscription alive for the object's lifetime.
///
/// @par Example — lifetime tied to a component
/// @code
///   class FHUDWidget
///   {
///   public:
///       void Init(IWindow& window)
///       {
///           // Subscription is removed automatically when FHUDWidget is destroyed.
///           m_onResized = window.GetEvents().onResized.AddScoped(
///               [this](FWindowResizedEvent const& e) { Rebuild(e.newSize); }
///           );
///       }
///
///   private:
///       FScopedDelegateHandle m_onResized;
///   };
/// @endcode
///
/// @par Early removal
/// @code
///   m_onResized.Reset();   // Unsubscribes immediately and invalidates the handle.
/// @endcode
///
/// @par Detach without removal (escape hatch)
/// @code
///   FDelegateHandle raw = m_onResized.Release();
///   // Caller is now responsible for calling event.Remove(raw).
/// @endcode
struct FScopedDelegateHandle
{
public:
    /// @brief Stateless type-erased trampoline: (source ptr, handle) -> removed.
    /// Generated once per TEventSource instantiation as a captureless lambda
    /// converted to a function pointer. Zero heap allocation.
    using FRemoveFn = bool (*)(void* source, FDelegateHandle handle) noexcept;

private:
    void* m_source{ nullptr };         //<! Type-erased pointer to the event source.
    FRemoveFn m_removeFn{ nullptr };   //<! Stateless compile-time trampoline; never null when valid.
    FDelegateHandle m_handle;          //<! Raw handle; invalid when not managing a subscription.

public:
    constexpr FScopedDelegateHandle() noexcept = default;

    /// @brief Internal constructor called by TEventSource / TEventView AddScoped().
    ///        Do not construct directly.
    /// @param source    Type-erased pointer to the owning event source.
    /// @param removeFn  Compile-time-generated trampoline that calls Remove().
    /// @param handle    The raw subscription handle to manage.
    FScopedDelegateHandle(void* source, FRemoveFn removeFn, FDelegateHandle handle) noexcept
        : m_source(source)
        , m_removeFn(removeFn)
        , m_handle(handle)
    {}

    ~FScopedDelegateHandle() noexcept { Reset(); }

    FScopedDelegateHandle(const FScopedDelegateHandle&) = delete;
    FScopedDelegateHandle& operator=(const FScopedDelegateHandle&) = delete;

    FScopedDelegateHandle(FScopedDelegateHandle&& other) noexcept
        : m_source(other.m_source)
        , m_removeFn(other.m_removeFn)
        , m_handle(other.m_handle)
    {
        other.Nullify();
    }

    FScopedDelegateHandle& operator=(FScopedDelegateHandle&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_source = other.m_source;
            m_removeFn = other.m_removeFn;
            m_handle = other.m_handle;
            other.Nullify();
        }
        return *this;
    }

public:
    /// @brief Returns true if this handle is actively managing a live subscription.
    GP_NODISCARD GP_FORCEINLINE bool IsValid() const noexcept { return m_handle.IsValid() && m_source != nullptr; }

    /// @brief Removes the subscription immediately and invalidates this handle.
    ///        Safe to call multiple times; subsequent calls are no-ops.
    void Reset() noexcept
    {
        if (!IsValid()) { return; }

        m_removeFn(m_source, m_handle);
        Nullify();
    }

    /// @brief Detaches from the subscription without removing it.
    /// Transfers removal responsibility to the caller. The returned raw handle
    /// must eventually be passed to the source's Remove() method.
    /// @return The raw FDelegateHandle previously managed by this object.
    GP_NODISCARD FDelegateHandle Release() noexcept
    {
        FDelegateHandle h = m_handle;
        Nullify();
        return h;
    }

private:
    void Nullify() noexcept
    {
        m_source = nullptr;
        m_removeFn = nullptr;
        m_handle.Invalidate();
    }
};

}   // namespace GP
