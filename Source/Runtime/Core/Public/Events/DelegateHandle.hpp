// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <atomic>

namespace GP::Events
{

/// \brief An opaque handle returned when binding a callable to a TMulticastDelegate.
///
/// The handle is used to uniquely identify a binding so it can be removed at a later point without needing a reference
/// to the original callable. Handles are non-copyable by design to prevent accidental double-removal; use
/// FScopedDelegateHandle for RAII ownership.
///
/// \note ID 0 is reserved as the "invalid" sentinel value and is never assigned to a live binding.
/// \note FDelegateHandle is a trivially-copyable value type safe to pass and store by value.
class FDelegateHandle final
{
public:
    static constexpr UInt64 InvalidID = 0ULL;   //<! The sentinel value representing an invalid or uninitialized handle.

public:
    UInt64 id{ InvalidID };   //<! The unique identifier for this delegate handle.

public:
    /// \brief Constructs an invalid (null) handle.
    constexpr FDelegateHandle() noexcept = default;

    /// \brief Constructs a handle with an explicit ID.
    /// \param inID The raw ID to assign. Must not be InvalidID for a valid handle.
    constexpr explicit FDelegateHandle(UInt64 inID) noexcept
        : id(inID)
    {}

public:
    /// \brief Equality operators compare handle IDs for equivalence. Two handles are equal if they have the same ID.
    GP_NODISCARD constexpr bool operator==(const FDelegateHandle&) const noexcept = default;

    /// \brief Inequality operator is the logical negation of equality.
    GP_NODISCARD constexpr bool operator!=(const FDelegateHandle&) const noexcept = default;

    /// \brief Allows FDelegateHandle to be used as a key in ordered containers.
    /// \param other The other handle to compare against.
    /// \return True if this handle's ID is less than the other handle's ID,
    GP_NODISCARD constexpr bool operator<(const FDelegateHandle& other) const noexcept { return id < other.id; }

public:
    /// \brief Returns true if this handle refers to a live binding.
    /// \return True if the handle is valid, false if it is invalid (i.e., has an ID of InvalidID).
    GP_NODISCARD constexpr bool IsValid() const noexcept { return id != InvalidID; }

    /// \brief Invalidates this handle, making it safe to discard.
    /// \note This does NOT remove the binding from the delegate; call TMulticastDelegate::Remove() first.
    constexpr void Reset() noexcept { id = InvalidID; }

public:
    /// \brief Generates a new globally-unique delegate handle.
    /// \note Uses a thread-safe monotonic counter. IDs are never reused within a process lifetime.
    /// \return A freshly generated, guaranteed-unique FDelegateHandle.
    GP_NODISCARD static FDelegateHandle Generate() noexcept
    {
        // Relaxed ordering is sufficient: uniqueness is all we need, not happens-before.
        static std::atomic<UInt64> s_counter{ 1ULL };
        return FDelegateHandle{ s_counter.fetch_add(1ULL, std::memory_order_relaxed) };
    }
};

}   // namespace GP::Events
