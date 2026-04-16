// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreMinimal.hpp"
#include "memory/Allocator.hpp"
#include "memory/Utilities.hpp"

namespace gp::memory
{

/// @brief Fast monotonic bump allocator over a pre-allocated memory buffer.
/// @details
/// Allocations advance a single offset pointer. Individual deallocations are no-ops; memory is reclaimed only via
/// `reset()`. This makes it ideal for per-frame scratch memory or short-lived scoped allocations.
///
/// Not thread-safe. Synchronization is the caller's responsibility (use TLS or job-system scheduling to guarantee
/// exclusive access).
///
/// Statistics (allocated size and count) are always-on since they are just `USize` increments on the single-threaded
/// hot path.
///
/// Hot data layout (32 bytes on x64): `m_buffer`, `m_capacity`, `m_offset`, `m_allocationCount` All fit within a single
/// cache line for zero-miss bump allocation.
class LinearAllocator final : public Allocator
{
private:
    gp::Byte* m_buffer{ nullptr };
    USize m_capacity{ 0ull };
    USize m_offset{ 0ull };
    USize m_allocatedSize{ 0ull };
    USize m_allocationCount{ 0ull };

public:
    /// @brief Constructs a linear allocator over an externally owned buffer.
    /// @param[in] buffer Pointer to the pre-allocated memory region.
    /// @param[in] capacity Size of the buffer in bytes.
    LinearAllocator(void* buffer, USize capacity) noexcept
        : m_buffer(static_cast<gp::Byte*>(buffer))
        , m_capacity(capacity)
    {}

    ~LinearAllocator() noexcept = default;

public:
    /// @brief Bumps the offset pointer to allocate a block with the given alignment.
    /// @param[in] size The number of bytes to allocate.
    /// @param[in] alignment The required alignment (must be a power of two).
    /// @return Pointer to the allocated block, or nullptr if the buffer is exhausted.
    GP_NODISCARD void* allocate(USize size, USize alignment = alignof(gp::MaxAlign)) noexcept override
    {
        const USize alignedOffset = memory::alignUp(m_offset, alignment);
        const USize newOffset = alignedOffset + size;

        if GP_UNLIKELY (newOffset > m_capacity) { return nullptr; }

        m_offset = newOffset;
        m_allocatedSize += size;
        ++m_allocationCount;

        return m_buffer + alignedOffset;
    }

    /// @brief No-op. Individual deallocation is not supported by a linear allocator.
    /// @details
    /// Memory is only reclaimed via reset(). The size parameter is accepted for interface compliance but ignored.
    void deallocate(GP_MAYBE_UNUSED void* ptr, GP_MAYBE_UNUSED USize size) noexcept override
    {
        // Intentional no-op. Use reset() to reclaim all memory.
    }

    /// @brief Resets the allocator to its initial state, reclaiming all memory.
    /// @details
    /// After reset, the full buffer is available for new allocations. All previously returned pointers become invalid.
    void reset() noexcept override
    {
        m_offset = 0ull;
        m_allocatedSize = 0ull;
        m_allocationCount = 0ull;
    }

    /// @brief Checks whether the given pointer falls within this allocator's buffer.
    /// @param[in] ptr Pointer to query.
    /// @return True if ptr is within [m_buffer, m_buffer + m_capacity).
    GP_NODISCARD bool owns(const void* ptr) const noexcept override
    {
        const auto* p = static_cast<const gp::Byte*>(ptr);
        return p >= m_buffer && p < m_buffer + m_capacity;
    }

    /// @brief Returns the current byte offset into the buffer.
    /// @return Number of bytes consumed (including alignment padding).
    GP_NODISCARD GP_FORCEINLINE USize getOffset() const noexcept { return m_offset; }

    /// @brief Returns the number of bytes remaining in the buffer.
    /// @return Available bytes before the allocator is exhausted.
    GP_NODISCARD GP_FORCEINLINE USize getRemaining() const noexcept { return m_capacity - m_offset; }

    GP_NODISCARD USize getAllocatedSize() const noexcept override { return m_allocatedSize; }

    GP_NODISCARD USize getAllocationCount() const noexcept override { return m_allocationCount; }

    GP_NODISCARD const char* getDebugName() const noexcept override { return "LinearAllocator"; }
};

}   // namespace gp::memory
