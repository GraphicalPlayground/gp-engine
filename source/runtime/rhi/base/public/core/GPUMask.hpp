// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashOf.hpp"
#include "maths/base/Bitwise.hpp"

namespace gp::rhi
{

/// @brief Compact 32-bit bitmask identifying one or more GPU adapter nodes in multi-GPU configurations.
/// @details Defaults to `gpu0()` on single-GPU systems. Supports bitwise combination, single-index queries, range-based
///          for-loop iteration over active node indices, and deterministic hashing in a 4-byte footprint.
struct GPUMask
{
    /// @brief Maximum number of GPU nodes addressable by a 32-bit mask.
    static constexpr gp::UInt32 kMaxGPUs = 32u;

    /// @brief Forward iterator yielding each set GPU node index in ascending bit order.
    class Iterator
    {
    public:
        /// @brief Constructs an iterator positioned at the first set GPU index in `inMask`.
        /// @param[in] inMask Mask whose active bits will be traversed.
        explicit constexpr Iterator(const GPUMask inMask) noexcept
            : m_mask(inMask.mask)
            , m_firstGPUIndexInMask(
                  inMask.mask != 0u ? static_cast<gp::UInt32>(gp::math::countTrailingZeros(inMask.mask)) : kMaxGPUs
              )
        {}

        /// @brief Constructs an iterator directly at bit position `inFirstGPUIndex` (used for sentinel `end()`).
        /// @param[in] inFirstGPUIndex Bit position in `[0, kMaxGPUs]`.
        explicit constexpr Iterator(const gp::UInt32 inFirstGPUIndex) noexcept
            : m_mask(0u)
            , m_firstGPUIndexInMask(inFirstGPUIndex)
        {}

        /// @brief Advances to the next set GPU bit in the mask.
        /// @return Reference to `*this`.
        constexpr Iterator& operator++() noexcept
        {
            m_mask &= ~(1u << m_firstGPUIndexInMask);
            m_firstGPUIndexInMask =
                (m_mask != 0u) ? static_cast<gp::UInt32>(gp::math::countTrailingZeros(m_mask)) : kMaxGPUs;
            return *this;
        }

        /// @brief Post-increment advancing to the next set GPU bit.
        /// @return Copy of the iterator prior to advancing.
        constexpr Iterator operator++(int) noexcept
        {
            Iterator previous = *this;
            ++(*this);
            return previous;
        }

        /// @brief Returns the current GPU node index.
        /// @return Zero-based GPU index in `[0, kMaxGPUs)`.
        [[nodiscard]] constexpr gp::UInt32 operator*() const noexcept
        {
            return m_firstGPUIndexInMask;
        }

        /// @brief Compares two iterators for inequality by current bit position.
        /// @param[in] rhs Iterator to compare against.
        /// @return True when positioned at different indices.
        [[nodiscard]] constexpr bool operator!=(const Iterator& rhs) const noexcept
        {
            return m_firstGPUIndexInMask != rhs.m_firstGPUIndexInMask;
        }

        /// @brief Compares two iterators for equality by current bit position.
        /// @param[in] rhs Iterator to compare against.
        /// @return True when positioned at the same index.
        [[nodiscard]] constexpr bool operator==(const Iterator& rhs) const noexcept
        {
            return m_firstGPUIndexInMask == rhs.m_firstGPUIndexInMask;
        }

        /// @brief Reports whether the iterator currently points to a valid GPU index.
        /// @return True while `m_firstGPUIndexInMask < kMaxGPUs`.
        [[nodiscard]] explicit constexpr operator bool() const noexcept
        {
            return m_firstGPUIndexInMask < kMaxGPUs;
        }

        /// @brief Reports whether iteration has reached the end sentinel.
        /// @return True when no active GPU indices remain.
        [[nodiscard]] constexpr bool operator!() const noexcept
        {
            return !static_cast<bool>(*this);
        }

    private:
        gp::UInt32 m_mask{ 0u };
        gp::UInt32 m_firstGPUIndexInMask{ kMaxGPUs };
    };

    /// @brief Raw bitmask where bit `i` corresponds to GPU node index `i`.
    gp::UInt32 mask{ 1u };

    /// @brief Constructs a mask selecting GPU node 0.
    constexpr GPUMask() noexcept = default;

    /// @brief Constructs a mask from a raw bitmask value.
    /// @param[in] inMask Bitmask of active GPU nodes.
    explicit constexpr GPUMask(const gp::UInt32 inMask) noexcept
        : mask(inMask)
    {}

    /// @brief Returns a mask selecting only GPU node 0.
    /// @return `GPUMask` with bit 0 set.
    [[nodiscard]] static constexpr GPUMask gpu0() noexcept
    {
        return GPUMask(1u);
    }

    /// @brief Returns a mask selecting all active GPU nodes (bit 0 in single-GPU mode).
    /// @return Active-GPU mask.
    [[nodiscard]] static constexpr GPUMask all() noexcept
    {
        return GPUMask(1u);
    }

    /// @brief Builds a single-GPU mask for adapter index `gpuIndex`.
    /// @param[in] gpuIndex Zero-based GPU node index in `[0, kMaxGPUs)`.
    /// @return `GPUMask` with only bit `gpuIndex` set.
    [[nodiscard]] static constexpr GPUMask fromIndex(const gp::UInt32 gpuIndex) noexcept
    {
        GP_CHECK(gpuIndex < kMaxGPUs);
        return GPUMask(1u << gpuIndex);
    }

    /// @brief Returns a mask containing all active GPUs at or above `gpuIndex`.
    /// @param[in] gpuIndex First GPU node index to keep.
    /// @return Filtered active-GPU mask.
    [[nodiscard]] static constexpr GPUMask filterGPUsBefore(const gp::UInt32 gpuIndex) noexcept
    {
        if (gpuIndex >= kMaxGPUs)
        {
            return GPUMask(0u);
        }
        return GPUMask(~((1u << gpuIndex) - 1u)) & all();
    }

    /// @brief Returns the underlying 32-bit bitmask representation.
    /// @return Raw bitmask value.
    [[nodiscard]] constexpr gp::UInt32 getNative() const noexcept
    {
        return mask;
    }

    /// @brief Returns the bitmask formatted for diagnostic display and logging.
    /// @return Raw bitmask value.
    [[nodiscard]] constexpr gp::UInt32 getForDisplay() const noexcept
    {
        return mask;
    }

    /// @brief Reports whether GPU node `gpuIndex` is included in this mask.
    /// @param[in] gpuIndex Zero-based GPU node index.
    /// @return True when bit `gpuIndex` is set.
    [[nodiscard]] constexpr bool contains(const gp::UInt32 gpuIndex) const noexcept
    {
        return (gpuIndex < kMaxGPUs) && ((mask & (1u << gpuIndex)) != 0u);
    }

    /// @brief Reports whether every GPU node in `other` is also present in this mask.
    /// @param[in] other Subset mask to test.
    /// @return True when `(mask & other.mask) == other.mask`.
    [[nodiscard]] constexpr bool containsAll(const GPUMask other) const noexcept
    {
        return (mask & other.mask) == other.mask;
    }

    /// @brief Reports whether this mask shares at least one GPU node with `other`.
    /// @param[in] other Mask to test overlap against.
    /// @return True when `(mask & other.mask) != 0`.
    [[nodiscard]] constexpr bool intersects(const GPUMask other) const noexcept
    {
        return (mask & other.mask) != 0u;
    }

    /// @brief Reports whether this mask contains at least one GPU node.
    /// @return True when `mask != 0`.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return mask != 0u;
    }

    /// @brief Reports whether this mask selects exactly one GPU node.
    /// @return True when `mask` has a single set bit.
    [[nodiscard]] constexpr bool hasSingleIndex() const noexcept
    {
        return (mask != 0u) && ((mask & (mask - 1u)) == 0u);
    }

    /// @brief Converts a single-GPU mask into its zero-based GPU node index.
    /// @note Asserts `hasSingleIndex()` is true.
    /// @return Zero-based GPU node index.
    [[nodiscard]] constexpr gp::UInt32 toIndex() const noexcept
    {
        GP_CHECK(hasSingleIndex());
        return static_cast<gp::UInt32>(gp::math::countTrailingZeros(mask));
    }

    /// @brief Returns the lowest-numbered GPU node index set in this mask.
    /// @return Zero-based index of the first set bit, or `0` if empty.
    [[nodiscard]] constexpr gp::UInt32 getFirstIndex() const noexcept
    {
        if (mask == 0u)
        {
            return 0u;
        }
        return static_cast<gp::UInt32>(gp::math::countTrailingZeros(mask));
    }

    /// @brief Returns the highest-numbered GPU node index set in this mask.
    /// @return Zero-based index of the last set bit, or `0` if empty.
    [[nodiscard]] constexpr gp::UInt32 getLastIndex() const noexcept
    {
        if (mask == 0u)
        {
            return 0u;
        }
        return static_cast<gp::UInt32>(gp::math::floorLog2(mask));
    }

    /// @brief Returns the number of GPU nodes enabled in this mask.
    /// @return Count of set bits in `mask`.
    [[nodiscard]] constexpr gp::UInt32 getNumGPUs() const noexcept
    {
        return static_cast<gp::UInt32>(gp::math::countBits(mask));
    }

    /// @brief Returns the number of active GPU nodes enabled in this mask (alias for `getNumGPUs()`).
    /// @return Count of set bits in `mask`.
    [[nodiscard]] constexpr gp::UInt32 getNumActive() const noexcept
    {
        return getNumGPUs();
    }

    /// @brief Computes the complement of this mask within `activeGPUs` (defaults to `all()`).
    /// @details If this mask covers every bit in `activeGPUs`, writes `gpu0()` to `outInverse` and returns `false`.
    /// @param[out] outInverse Receives the inverted mask within `activeGPUs`, or `gpu0()` when the complement is empty.
    /// @param[in] activeGPUs Active GPU universe mask to invert against.
    /// @return True when the complement is non-empty; false otherwise.
    [[nodiscard]] constexpr bool invert(GPUMask& outInverse, const GPUMask activeGPUs = all()) const noexcept
    {
        const gp::UInt32 inverted = (~mask) & activeGPUs.mask;
        if (inverted == 0u)
        {
            outInverse = gpu0();
            return false;
        }
        outInverse = GPUMask(inverted);
        return true;
    }

    /// @brief Computes the union of two GPU masks.
    /// @param[in] rhs Right-hand mask.
    /// @return Combined mask containing nodes from either input.
    [[nodiscard]] constexpr GPUMask operator|(const GPUMask rhs) const noexcept
    {
        return GPUMask(mask | rhs.mask);
    }

    /// @brief Computes the intersection of two GPU masks.
    /// @param[in] rhs Right-hand mask.
    /// @return Mask containing only nodes present in both inputs.
    [[nodiscard]] constexpr GPUMask operator&(const GPUMask rhs) const noexcept
    {
        return GPUMask(mask & rhs.mask);
    }

    /// @brief Unions `rhs` into this mask in place.
    /// @param[in] rhs Right-hand mask.
    /// @return Reference to `*this`.
    constexpr GPUMask& operator|=(const GPUMask rhs) noexcept
    {
        mask |= rhs.mask;
        return *this;
    }

    /// @brief Intersects `rhs` into this mask in place.
    /// @param[in] rhs Right-hand mask.
    /// @return Reference to `*this`.
    constexpr GPUMask& operator&=(const GPUMask rhs) noexcept
    {
        mask &= rhs.mask;
        return *this;
    }

    /// @brief Default equality comparison.
    /// @param[in] rhs Mask to compare against.
    /// @return True when raw bitmasks match.
    [[nodiscard]] constexpr bool operator==(const GPUMask& rhs) const noexcept = default;

    /// @brief Computes a 64-bit hash of this GPU mask.
    /// @return Hash digest derived from `mask`.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        return gp::hashing::hashOf(mask);
    }

    /// @brief Appends `mask` to a streaming hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target hasher accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(mask);
    }

    /// @brief Returns an iterator positioned at the first set GPU index for range-based for loops.
    /// @param[in] inMask Mask to iterate over.
    /// @return Begin `Iterator`.
    [[nodiscard]] friend constexpr Iterator begin(const GPUMask inMask) noexcept
    {
        return Iterator(inMask);
    }

    /// @brief Returns the past-the-end sentinel iterator for range-based for loops.
    /// @param[in] inMask Ignored mask parameter for ADL dispatch.
    /// @return End `Iterator`.
    [[nodiscard]] friend constexpr Iterator end([[maybe_unused]] const GPUMask inMask) noexcept
    {
        return Iterator(kMaxGPUs);
    }
};

}   // namespace gp::rhi
