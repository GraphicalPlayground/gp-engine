// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashMix.hpp"
#include <compare>

namespace gp
{

/// @brief Sentinel slot index indicating a null or absent list node link.
inline constexpr gp::UInt32 kInvalidListNodeIndex = 0xFFFFFFFFu;

/// @brief Maximum number of node slots addressable by a 32-bit `ListNodeHandle` index.
inline constexpr gp::UInt32 kMaxListSlotCapacity = kInvalidListNodeIndex;

/// @brief Empty tag type distinguishing `gp::DoubleLinkedList<InT>` handles from other list containers.
/// @tparam InT Element type stored by the owning `DoubleLinkedList`.
template <typename InT>
struct DoubleLinkedListNodeTag
{};

/// @brief Empty tag type distinguishing `gp::LinkedList<InT>` handles from other list containers.
/// @tparam InT Element type stored by the owning `LinkedList`.
template <typename InT>
struct LinkedListNodeTag
{};

/// @brief Generational 64-bit handle referencing a node inside a slot-pooled list container.
/// @details Packs a 32-bit slot index alongside a 32-bit generation counter. Slot indices remain stable across
///          slot-pool growth and each removal advances the slot generation, catching stale or double-removed handles
///          in O(1) time before dereferencing a recycled slot.
/// @tparam InTag Tag type differentiating handles across distinct list container families and element types.
template <typename InTag>
struct ListNodeHandle
{
    /// @brief Sentinel value denoting an invalid or uninitialized slot index.
    static constexpr gp::UInt32 kInvalidIndex = kInvalidListNodeIndex;

    /// @brief Zero-based index into the owning container's slot buffer, or `kInvalidIndex` when invalid.
    gp::UInt32 index{ kInvalidIndex };

    /// @brief Odd, non-zero generation counter matching the target slot at the time of insertion (`0` when invalid).
    gp::UInt32 generation{ 0u };

public:
    /// @brief Returns a canonical invalid handle.
    /// @return Default-constructed invalid `ListNodeHandle`.
    [[nodiscard]] static constexpr ListNodeHandle invalid() noexcept
    {
        return ListNodeHandle{};
    }

    /// @brief Checks whether this handle carries a non-sentinel index and non-zero odd generation.
    /// @note A valid handle structure does not prove the node is still alive; pass the handle to `isValidHandle()` on
    ///       the owning list to verify liveness against the current slot generation.
    /// @return True if structurally valid, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return index != kInvalidIndex && generation != 0u && ((generation & 1u) != 0u);
    }

    /// @brief Implicit-safe boolean conversion checking structural handle validity.
    /// @return True if `isValid()` is true, false otherwise.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Resets this handle to the canonical invalid state.
    constexpr void invalidate() noexcept
    {
        index = kInvalidIndex;
        generation = 0u;
    }

    /// @brief Computes an avalanche-mixed 64-bit hash over `(index, generation)`.
    /// @return 64-bit hash suitable for `gp::Map`, `gp::Set`, and `gp::hashing::hash()`.
    [[nodiscard]] constexpr gp::UInt64 hash() const noexcept
    {
        const gp::UInt64 packed = (static_cast<gp::UInt64>(generation) << 32u) | static_cast<gp::UInt64>(index);
        return gp::hashing::avalancheMix64(packed);
    }

    /// @brief Streams `index` and `generation` into any `gp::hashing` accumulator.
    /// @tparam InHasher Concrete streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(index);
        hasher.append(generation);
    }

    /// @brief Compares two handles for exact index and generation equality.
    /// @param[in] other Right-hand handle to compare against.
    /// @return True if both `index` and `generation` match, false otherwise.
    [[nodiscard]] constexpr bool operator==(const ListNodeHandle& other) const noexcept = default;

    /// @brief Three-way comparison ordering handles lexicographically by `index`, then `generation`.
    /// @param[in] other Right-hand handle to compare against.
    /// @return `std::strong_ordering` result.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const ListNodeHandle& other) const noexcept = default;
};

static_assert(
    sizeof(ListNodeHandle<DoubleLinkedListNodeTag<gp::Int32>>) == 8u, "ListNodeHandle must be exactly 8 bytes."
);
static_assert(
    alignof(ListNodeHandle<DoubleLinkedListNodeTag<gp::Int32>>) == 4u, "ListNodeHandle must be 4-byte aligned."
);
static_assert(
    gp::concepts::IsTriviallyCopyable<ListNodeHandle<DoubleLinkedListNodeTag<gp::Int32>>>,
    "ListNodeHandle must be trivially copyable."
);
static_assert(
    gp::concepts::IsStandardLayout<ListNodeHandle<DoubleLinkedListNodeTag<gp::Int32>>>,
    "ListNodeHandle must have standard layout."
);
static_assert(
    gp::concepts::IsRelocatable<ListNodeHandle<DoubleLinkedListNodeTag<gp::Int32>>>,
    "ListNodeHandle must be bitwise relocatable."
);

}   // namespace gp
