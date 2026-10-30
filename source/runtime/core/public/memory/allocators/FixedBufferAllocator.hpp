// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/AllocatorTypes.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"

namespace gp::memory
{

/// @brief Compile-time fixed-capacity stack/object-embedded bump arena allocator with zero heap fallback.
/// @details Embeds a contiguous `InCapacityBytes` byte array directly inside the allocator instance
/// (`sizeof(FixedBufferAllocator)`
///          includes the buffer). Ideal for deterministic stack-local scratch buffers or fixed-budget subsystems where
///          heap allocation is strictly forbidden. Returns `nullptr` if an allocation exceeds remaining capacity.
/// @tparam InCapacityBytes Total inline buffer size in bytes (must be `> 0`).
/// @tparam InAlignment Alignment of the embedded buffer in bytes (power of two; defaults to `kMinimumAlignment`).
template <gp::USize InCapacityBytes, gp::UInt32 InAlignment = kMinimumAlignment>
class FixedBufferAllocator
{
    static_assert(InCapacityBytes > 0u, "FixedBufferAllocator capacity must be greater than zero.");
    static_assert(
        InAlignment >= kMinimumAlignment && gp::math::isPowerOfTwo(InAlignment),
        "FixedBufferAllocator alignment must be a power of two >= kMinimumAlignment."
    );

public:
    static constexpr gp::USize kCapacityBytes = InCapacityBytes;
    static constexpr gp::UInt32 kBufferAlignment = InAlignment;

private:
    alignas(InAlignment) gp::UInt8 m_buffer[InCapacityBytes]{};
    gp::USize m_offset{ 0u };
    AllocatorStats m_stats{ .allocatedBytes = 0u,
                            .peakAllocatedBytes = 0u,
                            .capacityBytes = InCapacityBytes,
                            .activeAllocations = 0ull,
                            .totalAllocations = 0ull,
                            .totalDeallocations = 0ull };

public:
    /// @brief Constructs an empty fixed-buffer allocator with cursor at offset `0`.
    constexpr FixedBufferAllocator() noexcept = default;

    /// @brief Copying or moving a fixed-buffer allocator is disabled because external pointers reference `m_buffer`
    /// directly.
    FixedBufferAllocator(const FixedBufferAllocator&) = delete;
    FixedBufferAllocator& operator=(const FixedBufferAllocator&) = delete;
    FixedBufferAllocator(FixedBufferAllocator&&) = delete;
    FixedBufferAllocator& operator=(FixedBufferAllocator&&) = delete;

    /// @brief Destructor.
    constexpr ~FixedBufferAllocator() noexcept = default;

    /// @brief Allocates a memory block of `sizeInBytes` aligned to `max(kMinimumAlignment, alignment)` from the
    /// embedded buffer.
    /// @param[in] sizeInBytes Number of bytes to allocate.
    /// @param[in] alignment Required byte alignment (must be a power of two).
    /// @return Pointer to the allocated region inside `m_buffer`, or `nullptr` if capacity is exceeded or `sizeInBytes
    /// == 0`.
    [[nodiscard]] void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        if (sizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

        const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
        GP_ASSERT(
            math::isPowerOfTwo(effectiveAlignment), "FixedBufferAllocator::allocate requires a power-of-two alignment."
        );

        const gp::UInt64 baseAddress = reinterpret_cast<gp::UInt64>(m_buffer);
        const gp::UInt64 currentAddress = baseAddress + static_cast<gp::UInt64>(m_offset);
        const gp::UInt64 alignedAddress = memory::align(currentAddress, static_cast<gp::UInt64>(effectiveAlignment));
        const gp::USize padding = static_cast<gp::USize>(alignedAddress - currentAddress);
        const gp::USize totalRequired = padding + sizeInBytes;

        if (totalRequired > (InCapacityBytes - m_offset)) [[unlikely]]
        {
            return nullptr;
        }

        m_offset += totalRequired;
        m_stats.allocatedBytes = m_offset;
        m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_offset);
        ++m_stats.activeAllocations;
        ++m_stats.totalAllocations;

        return reinterpret_cast<void*>(alignedAddress);
    }

    /// @brief Resizes an allocation in place if it sits at the top of the bump cursor, or allocates a new region and
    /// copies bytes.
    /// @param[in] ptr Previously allocated pointer (or `nullptr`).
    /// @param[in] oldSizeInBytes Exact byte size of the existing allocation at `ptr`.
    /// @param[in] newSizeInBytes Desired new size in bytes.
    /// @param[in] alignment Required byte alignment.
    /// @return Pointer to the resized region, or `nullptr` if `newSizeInBytes == 0` or capacity is exhausted.
    [[nodiscard]] void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (newSizeInBytes == 0u)
        {
            deallocate(ptr, oldSizeInBytes, alignment);
            return nullptr;
        }

        if (ptr == nullptr)
        {
            return allocate(newSizeInBytes, alignment);
        }

        GP_ASSERT(owns(ptr), "FixedBufferAllocator::reallocate called with a pointer outside this allocator's buffer.");

        const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
        const gp::UIntPtr baseAddress = reinterpret_cast<gp::UIntPtr>(m_buffer);
        const gp::UIntPtr ptrAddress = reinterpret_cast<gp::UIntPtr>(ptr);
        const gp::USize ptrOffset = static_cast<gp::USize>(ptrAddress - baseAddress);

        if (oldSizeInBytes > 0u && (ptrOffset + oldSizeInBytes) == m_offset &&
            memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
        {
            if (newSizeInBytes <= oldSizeInBytes)
            {
                m_offset = ptrOffset + newSizeInBytes;
                m_stats.allocatedBytes = m_offset;
                return ptr;
            }

            const gp::USize additionalBytes = newSizeInBytes - oldSizeInBytes;
            if (additionalBytes <= (InCapacityBytes - m_offset))
            {
                m_offset += additionalBytes;
                m_stats.allocatedBytes = m_offset;
                m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_offset);
                return ptr;
            }

            return nullptr;
        }

        if (newSizeInBytes <= oldSizeInBytes && memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
        {
            return ptr;
        }

        void* newPtr = allocate(newSizeInBytes, alignment);
        if (newPtr != nullptr && oldSizeInBytes > 0u)
        {
            const gp::USize bytesToCopy = math::min(oldSizeInBytes, newSizeInBytes);
            memory::copyMemory(newPtr, ptr, bytesToCopy);
        }

        return newPtr;
    }

    /// @brief Rewinds the bump cursor if `ptr` is the most recent allocation at the top of the buffer; otherwise no-op.
    /// @param[in] ptr Pointer to deallocate (may be `nullptr`).
    /// @param[in] oldSizeInBytes Size in bytes of the allocation at `ptr`.
    /// @param[in] alignment Alignment used when allocating `ptr`.
    void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 /*alignment*/ = kDefaultAlignment
    ) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }

        GP_ASSERT(owns(ptr), "FixedBufferAllocator::deallocate called with a pointer outside this allocator's buffer.");

        ++m_stats.totalDeallocations;

        if (oldSizeInBytes > 0u)
        {
            const gp::UIntPtr baseAddress = reinterpret_cast<gp::UIntPtr>(m_buffer);
            const gp::UIntPtr ptrAddress = reinterpret_cast<gp::UIntPtr>(ptr);
            const gp::USize ptrOffset = static_cast<gp::USize>(ptrAddress - baseAddress);

            if ((ptrOffset + oldSizeInBytes) == m_offset)
            {
                m_offset = ptrOffset;
                m_stats.allocatedBytes = m_offset;
                if (m_stats.activeAllocations > 0ull)
                {
                    --m_stats.activeAllocations;
                }
            }
        }
    }

    /// @brief Computes the usable allocation size for `requestedSize` and `alignment`.
    /// @param[in] requestedSize Requested byte count.
    /// @param[in] alignment Requested byte alignment.
    /// @return Aligned byte count, or `0` if `requestedSize == 0` or exceeds `InCapacityBytes`.
    [[nodiscard]] constexpr gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        if (requestedSize == 0u || requestedSize > InCapacityBytes)
        {
            return 0u;
        }

        const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
        return static_cast<gp::USize>(
            memory::align(static_cast<gp::UInt64>(requestedSize), static_cast<gp::UInt64>(effectiveAlignment))
        );
    }

    /// @brief Checks whether `ptr` points within the embedded `m_buffer`.
    /// @param[in] ptr Pointer to test.
    /// @return True if `ptr` lies within `[m_buffer, m_buffer + InCapacityBytes)`.
    [[nodiscard]] bool owns(const void* ptr) const noexcept
    {
        if (ptr == nullptr)
        {
            return false;
        }

        const gp::UIntPtr address = reinterpret_cast<gp::UIntPtr>(ptr);
        const gp::UIntPtr baseAddress = reinterpret_cast<gp::UIntPtr>(m_buffer);
        return address >= baseAddress && address < (baseAddress + InCapacityBytes);
    }

    /// @brief Captures a checkpoint of the current bump cursor for subsequent `rewind()`.
    /// @return `MemoryMark` representing the current cursor state.
    [[nodiscard]] constexpr MemoryMark mark() const noexcept
    {
        return MemoryMark{ .offset = m_offset,
                           .activeAllocations = m_stats.activeAllocations,
                           .allocatedBytes = m_stats.allocatedBytes };
    }

    /// @brief Rewinds the bump cursor back to `savedMark` in O(1) time.
    /// @param[in] savedMark Checkpoint previously captured by `mark()`.
    constexpr void rewind(const MemoryMark& savedMark) noexcept
    {
        GP_ASSERT(
            savedMark.offset <= m_offset, "FixedBufferAllocator::rewind cannot advance forward past the current cursor."
        );

        m_offset = savedMark.offset;
        m_stats.allocatedBytes = savedMark.allocatedBytes;
        m_stats.activeAllocations = savedMark.activeAllocations;
    }

    /// @brief Reclaims all allocations in the embedded buffer in O(1) time by resetting `m_offset` to `0`.
    constexpr void reset() noexcept
    {
        m_offset = 0u;
        m_stats.allocatedBytes = 0u;
        m_stats.activeAllocations = 0ull;
    }

    /// @brief Retrieves the total compile-time capacity of the embedded buffer in bytes.
    /// @return `InCapacityBytes`.
    [[nodiscard]] static consteval gp::USize getCapacity() noexcept
    {
        return InCapacityBytes;
    }

    /// @brief Retrieves the number of bytes currently consumed from the embedded buffer.
    /// @return Used bytes.
    [[nodiscard]] constexpr gp::USize getUsedBytes() const noexcept
    {
        return m_offset;
    }

    /// @brief Retrieves the remaining unallocated bytes in the embedded buffer.
    /// @return Remaining bytes.
    [[nodiscard]] constexpr gp::USize getRemainingBytes() const noexcept
    {
        return InCapacityBytes >= m_offset ? (InCapacityBytes - m_offset) : 0u;
    }

    /// @brief Retrieves the current telemetry statistics for this fixed buffer.
    /// @return Const reference to `AllocatorStats`.
    [[nodiscard]] constexpr const AllocatorStats& getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Retrieves a const pointer to the start of the embedded byte buffer.
    /// @return Pointer to `m_buffer`.
    [[nodiscard]] constexpr const gp::UInt8* getData() const noexcept
    {
        return m_buffer;
    }
};

static_assert(
    gp::concepts::IsMarkableAllocator<FixedBufferAllocator<256u>>,
    "FixedBufferAllocator must satisfy gp::concepts::IsMarkableAllocator."
);

}   // namespace gp::memory
