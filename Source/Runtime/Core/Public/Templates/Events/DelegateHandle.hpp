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

}   // namespace GP
