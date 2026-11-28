// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::allocator
{

/// @brief Intrusive block descriptor representing a free, allocated, or sentinel node inside a `MemoryPool`.
/// @details Blocks form a circular doubly-linked list anchored at `MemoryPool::m_headBlock`. Each allocated block may
///          also anchor a secondary doubly-linked list of alias descriptors via `m_aliasAllocation`.
struct GP_RHI_API PoolAllocationData
{
public:
    /// @brief Constructs an uninitialized allocation node in the `Unknown` state.
    PoolAllocationData() noexcept = default;

    /// @brief Resets all fields, links, flags, and ownership to the default unknown state.
    void reset() noexcept;

    /// @brief Initializes this node as the circular sentinel head of a pool's block list.
    /// @param[in] poolIndex Zero-based index of the owning `MemoryPool`.
    void initAsHead(gp::Int16 poolIndex) noexcept;

    /// @brief Initializes this node as a free region covering `[offset, offset + size)`.
    /// @param[in] poolIndex Zero-based index of the owning `MemoryPool`.
    /// @param[in] size Free block size in bytes.
    /// @param[in] alignment Base alignment in bytes for the free block.
    /// @param[in] offset Byte offset from the start of the pool.
    void initAsFree(gp::Int16 poolIndex, gp::UInt32 size, gp::UInt32 alignment, gp::UInt32 offset) noexcept;

    /// @brief Carves an allocated block from the front of `freeBlock` and inserts this node before it.
    /// @details The new allocation starts locked (`isLocked() == true`) so defragmentation cannot relocate it while the
    ///          caller populates initial data. `freeBlock`'s size shrinks and its offset advances by the aligned size.
    /// @param[in] size Requested allocation size in bytes.
    /// @param[in] poolAlignment Pool-level alignment granularity in bytes.
    /// @param[in] allocationAlignment Requested byte alignment for this allocation.
    /// @param[in,out] freeBlock Free block to sub-allocate from. Must be in the `Free` state.
    void initAsAllocated(
        gp::UInt32 size, gp::UInt32 poolAlignment, gp::UInt32 allocationAlignment, PoolAllocationData* freeBlock
    ) noexcept;

    /// @brief Transfers allocation state, linked-list position, and alias links from `allocated` into this node.
    /// @details Supports both primary allocations linked in a pool's block list and alias entries linked under a parent
    ///          allocation. Resets `allocated` after the transfer.
    /// @param[in,out] allocated Source allocated node to move from. Must be in the `Allocated` state.
    /// @param[in] locked Lock state assigned to this node after the move.
    void moveFrom(PoolAllocationData& allocated, bool locked) noexcept;

    /// @brief Converts an allocated block into a free block snapped to `poolAlignment`.
    /// @param[in] poolAlignment Pool-level alignment granularity in bytes.
    /// @param[in] allocationAlignment Original alignment requested when this block was allocated.
    void markFree(gp::UInt32 poolAlignment, gp::UInt32 allocationAlignment) noexcept;

    /// @brief Prepends `other` to this allocation's intrusive alias list.
    /// @note Callers should synchronize access under the owning pool lock.
    /// @param[in,out] other Alias node to attach to this primary allocation.
    void addAlias(PoolAllocationData* other) noexcept;

    /// @brief Unlinks this node from its parent allocation's alias list.
    void removeAlias() noexcept;

    /// @brief Returns the first alias node attached to this allocation, or `nullptr` if none exist.
    /// @return Head of the alias linked list, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT PoolAllocationData* getFirstAlias() const noexcept
    {
        return m_aliasAllocation;
    }

    /// @brief Reports whether this node is the pool list's sentinel head block.
    /// @return True if this node has type `Head`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isHead() const noexcept
    {
        return getAllocationType() == AllocationType::Head;
    }

    /// @brief Reports whether this node represents a free memory range.
    /// @return True if this node has type `Free`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isFree() const noexcept
    {
        return getAllocationType() == AllocationType::Free;
    }

    /// @brief Reports whether this node represents a live allocated memory range.
    /// @return True if this node has type `Allocated`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isAllocated() const noexcept
    {
        return getAllocationType() == AllocationType::Allocated;
    }

    /// @brief Returns the logical size in bytes stored in this block.
    /// @return Block size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSize() const noexcept
    {
        return m_size;
    }

    /// @brief Returns the byte alignment stored for this block.
    /// @return Alignment in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getAlignment() const noexcept
    {
        return m_alignment;
    }

    /// @brief Returns the byte offset of this block from the start of its owning pool.
    /// @return Byte offset within the pool.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getOffset() const noexcept
    {
        return m_offset;
    }

    /// @brief Returns the signed index of the owning `MemoryPool`, or `-1` when unassigned.
    /// @return Owning pool index.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Int16 getPoolIndex() const noexcept
    {
        return static_cast<gp::Int16>(m_poolIndex);
    }

    /// @brief Returns the next node in the pool's block list (or the next sibling in an alias list).
    /// @return Pointer to the next `PoolAllocationData` node, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT PoolAllocationData* getNext() const noexcept
    {
        return m_nextAllocation;
    }

    /// @brief Returns the previous node in the pool's block list (or the previous sibling in an alias list).
    /// @return Pointer to the previous `PoolAllocationData` node, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT PoolAllocationData* getPrev() const noexcept
    {
        return m_previousAllocation;
    }

    /// @brief Associates an opaque resource owner with this allocation.
    /// @param[in] owner Non-null or null owner pointer; requires the current owner to be `nullptr`.
    GP_FORCEINLINE_HINT void setOwner(PoolResource* const owner) noexcept
    {
        GP_CHECK(m_owner == nullptr);
        m_owner = owner;
    }

    /// @brief Returns the opaque resource owner attached to this allocation, or `nullptr`.
    /// @return Owning `PoolResource` pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT PoolResource* getOwner() const noexcept
    {
        return m_owner;
    }

    /// @brief Reassigns ownership when one resource hands its live backing memory to another before destruction.
    /// @param[in] owner New non-null owner pointer; requires the current owner to also be non-null.
    GP_FORCEINLINE_HINT void transferOwner(PoolResource* const owner) noexcept
    {
        GP_CHECK(m_owner != nullptr && owner != nullptr);
        m_owner = owner;
    }

    /// @brief Clears the owner pointer while keeping the allocation live.
    /// @details Used when a backing memory block outlives the resource wrapper that created it.
    GP_FORCEINLINE_HINT void clearOwner() noexcept
    {
        m_owner = nullptr;
    }

    /// @brief Reports whether this block is currently pinned against defragmentation.
    /// @return True if locked, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isLocked() const noexcept
    {
        return m_locked == 1u;
    }

    /// @brief Clears the lock bit so defragmentation is permitted to relocate this block.
    /// @note Does not require taking the owning pool lock.
    GP_FORCEINLINE_HINT void unlock() noexcept
    {
        GP_CHECK(m_locked == 1u);
        m_locked = 0u;
    }

private:
    friend class MemoryPool;
    friend class PoolAllocator;

    /// @brief Sets the lock bit; callers should hold the owning pool lock.
    GP_FORCEINLINE_HINT void lock() noexcept
    {
        GP_CHECK(m_locked == 0u);
        m_locked = 1u;
    }

    /// @brief Absorbs a contiguous subsequent free block into this block and resets `other`.
    /// @param[in,out] other Contiguous free block immediately following this block.
    void merge(PoolAllocationData* other) noexcept;

    /// @brief Unlinks this free node from its doubly-linked pool list.
    void removeFromLinkedList() noexcept;

    /// @brief Inserts `other` immediately before this node in the circular doubly-linked list.
    /// @param[in,out] other Node to insert before `this`.
    void addBefore(PoolAllocationData* other) noexcept;

    /// @brief Inserts `other` immediately after this node in the circular doubly-linked list.
    /// @param[in,out] other Node to insert after `this`.
    void addAfter(PoolAllocationData* other) noexcept;

    /// @brief Internal classification of a `PoolAllocationData` node.
    enum class AllocationType : gp::UInt8
    {
        Unknown = 0u,
        Free,
        Allocated,
        Head
    };

    /// @brief Returns the internal node classification stored in the bitfield.
    /// @return Current `AllocationType` enumerator.
    [[nodiscard]] GP_FORCEINLINE_HINT AllocationType getAllocationType() const noexcept
    {
        return static_cast<AllocationType>(m_type);
    }

    /// @brief Updates the internal node classification bitfield.
    /// @param[in] type New `AllocationType` enumerator to store.
    GP_FORCEINLINE_HINT void setAllocationType(const AllocationType type) noexcept
    {
        m_type = static_cast<gp::UInt32>(type);
    }

    gp::UInt32 m_size{ 0u };
    gp::UInt32 m_alignment{ 0u };
    gp::UInt32 m_offset{ 0u };
    gp::UInt32 m_poolIndex : 16 { 0xFFFFu };
    gp::UInt32 m_type : 4 { 0u };
    /// @brief Pins the block against defragmentation while initial data is written, CPU-mapped, or mid-relocation.
    gp::UInt32 m_locked : 1 { 0u };
    gp::UInt32 m_unused : 11 { 0u };

    PoolAllocationData* m_previousAllocation{ nullptr };
    PoolAllocationData* m_nextAllocation{ nullptr };
    PoolAllocationData* m_aliasAllocation{ nullptr };

    PoolResource* m_owner{ nullptr };
};

}   // namespace gp::rhi::allocator
