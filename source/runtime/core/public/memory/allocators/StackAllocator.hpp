// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/AllocatorTypes.hpp"
#include "memory/allocators/IAllocator.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/MemoryForward.hpp"

namespace gp::memory
{

/// @brief LIFO (Last-In, First-Out) stack arena allocator supporting individual top-of-stack deallocations and marks.
/// @details Unlike `LinearAllocator`, `StackAllocator` prepends a compact 8-byte `Header` (`previousOffset` and
///          `allocationSize`) immediately prior to each aligned user pointer. This enables O(1) `deallocate(ptr)`
///          without requiring the caller to pass `oldSizeInBytes`, provided deallocations occur in reverse allocation
///          order, while also supporting bulk unwinding via `ScopedMemoryMark<StackAllocator>` and `reset()`.
class GP_CORE_API StackAllocator final : public IAllocator
{
private:
    gp::UInt8* m_buffer{ nullptr };
    Malloc* m_backingMalloc{ nullptr };
    const char* m_poolName{ "StackAllocator" };
    gp::USize m_capacity{ 0u };
    gp::USize m_offset{ 0u };
    AllocatorStats m_stats{};
    bool m_ownsBuffer{ false };

public:
    /// @brief Compact 8-byte metadata header stored immediately before each aligned allocation address.
    struct Header
    {
        /// @brief Arena cursor offset prior to this allocation (including any alignment padding).
        gp::UInt32 previousOffset{ 0u };

        /// @brief Size in bytes of the user payload for this allocation.
        gp::UInt32 allocationSize{ 0u };
    };

    /// @brief Constructs an uninitialized stack allocator with zero capacity.
    constexpr StackAllocator() noexcept = default;

    /// @brief Constructs a stack allocator backed by a contiguous slab from `backingMalloc`.
    /// @param[in] capacityInBytes Total size in bytes of the contiguous stack buffer.
    /// @param[in] backingMalloc Optional `Malloc` backend (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for Tracy profiler telemetry.
    explicit StackAllocator(
        const gp::USize capacityInBytes, Malloc* backingMalloc = nullptr, const char* poolName = "StackAllocator"
    ) noexcept;

    /// @brief Constructs a stack allocator operating over a caller-owned external memory buffer (zero heap allocs).
    /// @param[in] externalBuffer Pointer to a caller-managed contiguous buffer.
    /// @param[in] capacityInBytes Size in bytes of `externalBuffer`.
    /// @param[in] poolName Optional static string name for Tracy profiler plot telemetry.
    StackAllocator(
        void* externalBuffer, const gp::USize capacityInBytes, const char* poolName = "StackAllocator.External"
    ) noexcept;

    /// @brief Copying a stack allocator is disabled.
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    /// @brief Move constructor; transfers buffer ownership and stack cursor from `other`.
    /// @param[in,out] other Source stack allocator, left in an empty state.
    StackAllocator(StackAllocator&& other) noexcept;

    /// @brief Move assignment operator; releases any owned buffer and steals state from `other`.
    /// @param[in,out] other Source stack allocator, left in an empty state.
    /// @return Reference to `*this`.
    StackAllocator& operator=(StackAllocator&& other) noexcept;

    /// @brief Destructor; releases the backing slab if owned.
    virtual ~StackAllocator() noexcept override;

    /// @brief Initializes or re-initializes the stack arena with a slab from `backingMalloc`.
    /// @param[in] capacityInBytes Total capacity in bytes to allocate.
    /// @param[in] backingMalloc Optional `Malloc` backend (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if initialization succeeded; false otherwise.
    bool initialize(
        const gp::USize capacityInBytes, Malloc* backingMalloc = nullptr, const char* poolName = "StackAllocator"
    ) noexcept;

    /// @brief Initializes or re-initializes the stack arena over an external caller-owned buffer.
    /// @param[in] externalBuffer Pointer to the external memory block.
    /// @param[in] capacityInBytes Size in bytes of `externalBuffer`.
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if `externalBuffer != nullptr` and `capacityInBytes > sizeof(Header)`; false otherwise.
    bool initialize(
        void* externalBuffer, const gp::USize capacityInBytes, const char* poolName = "StackAllocator.External"
    ) noexcept;

    /// @brief Releases the backing slab (if owned) and resets state.
    void shutdown() noexcept;

    /// @copydoc IAllocator::allocate()
    /// @note Prepends an 8-byte `Header` before the aligned payload address and advances `m_offset` in O(1) time.
    [[nodiscard]] virtual void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept override;

    /// @copydoc IAllocator::reallocate()
    /// @note Resizes in place in O(1) time if `ptr` is the top allocation on the stack and remains aligned;
    ///       otherwise allocates a new block at the top of the stack and copies the payload.
    [[nodiscard]] virtual void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept override;

    /// @copydoc IAllocator::deallocate()
    /// @note Reads the preceding `Header` and pops the stack cursor back to `previousOffset` if `ptr` is the active
    ///       top allocation. Out-of-order frees leave the cursor unchanged until an enclosing mark or `reset()`.
    virtual void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept override;

    /// @copydoc IAllocator::getActualAllocationSize()
    [[nodiscard]] virtual gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept override;

    /// @copydoc IAllocator::owns()
    [[nodiscard]] virtual bool owns(const void* ptr) const noexcept override;

    /// @copydoc IAllocator::getName()
    [[nodiscard]] virtual const char* getName() const noexcept override;

    /// @brief Captures a checkpoint of the current stack cursor for subsequent `rewind()`.
    /// @return `MemoryMark` representing the current stack state.
    [[nodiscard]] constexpr MemoryMark mark() const noexcept
    {
        return MemoryMark{ .offset = m_offset,
                           .activeAllocations = m_stats.activeAllocations,
                           .allocatedBytes = m_stats.allocatedBytes };
    }

    /// @brief Rewinds the stack cursor back to a previously captured `MemoryMark` in O(1) time.
    /// @param[in] savedMark Checkpoint previously returned by `mark()`.
    void rewind(const MemoryMark& savedMark) noexcept;

    /// @brief Reclaims all allocations in the stack arena in O(1) time by resetting `m_offset` to `0`.
    void reset() noexcept;

    /// @brief Retrieves the total capacity of the stack arena in bytes.
    /// @return Capacity in bytes.
    [[nodiscard]] constexpr gp::USize getCapacity() const noexcept
    {
        return m_capacity;
    }

    /// @brief Retrieves the current byte offset of the stack cursor (including headers and alignment padding).
    /// @return Used bytes from the start of the buffer.
    [[nodiscard]] constexpr gp::USize getUsedBytes() const noexcept
    {
        return m_offset;
    }

    /// @brief Retrieves the remaining unallocated byte capacity from the current cursor to the end of the buffer.
    /// @return Remaining bytes available.
    [[nodiscard]] constexpr gp::USize getRemainingBytes() const noexcept
    {
        return m_capacity >= m_offset ? (m_capacity - m_offset) : 0u;
    }

    /// @brief Retrieves the current telemetry statistics for this stack arena.
    /// @return Const reference to `AllocatorStats`.
    [[nodiscard]] constexpr const AllocatorStats& getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Checks whether the stack arena holds a valid backing buffer.
    /// @return True if initialized with a non-null buffer and positive capacity; false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_buffer != nullptr && m_capacity > sizeof(Header);
    }
};

static_assert(
    gp::concepts::IsMarkableAllocator<StackAllocator>, "StackAllocator must satisfy gp::concepts::IsMarkableAllocator."
);

}   // namespace gp::memory
