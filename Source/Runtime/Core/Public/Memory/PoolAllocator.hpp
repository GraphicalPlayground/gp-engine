// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/MemoryUtilities.hpp"

namespace GP::Memory
{

/// \brief A pool allocator that manages fixed-size blocks from a contiguous buffer.
///
/// All allocations return blocks of exactly BlockSize bytes. A free-list of available blocks is maintained inline (the
/// free block's memory stores a pointer to the next free block). Allocation and deallocation are both O(1).
///
/// **How AAA engines use this**:
///   - Unreal: TMemoryPool — used for particle systems, component storage
///   - Frostbite: Pool allocators for entity components
///   - idTech: Zone allocators for fixed-size game objects
///
/// \note Pool allocators eliminate fragmentation entirely for fixed-size objects. They are the highest-performance
/// allocator for cases where all allocations are the same size — which is surprisingly common in game engines
/// (particles, nodes, entities, draw commands).
///
/// The free-list trick: each free block stores a pointer to the next free block inside the block's own memory. This
/// means the free-list requires zero extra memory overhead. When a block is allocated, its free-list pointer is
/// overwritten by the user's data — that's fine, because it's no longer free.
///
/// \tparam BlockSize Size of each block in bytes. Must be >= sizeof(void*).
/// \tparam BlockCount Number of blocks in the pool.
/// \tparam Alignment Alignment of each block.
template <SizeT BlockSize, SizeT BlockCount, SizeT Alignment = alignof(std::max_align_t)>
class TPoolAllocator
{
    static_assert(
        BlockSize >= sizeof(void*), "Block size must be at least sizeof(void*) to store the free-list pointer"
    );
    static_assert(BlockCount > 0, "Must have at least one block");
    static_assert(IsPowerOfTwo(Alignment), "Alignment must be a power of two");

public:
    using SizeType = SizeT;   //<! Size type for allocations and deallocations.

public:
    /// \brief The effective block size after alignment padding.
    static constexpr SizeT AlignedBlockSize = AlignUp(BlockSize, Alignment);

    /// \brief Total memory footprint of the pool.
    static constexpr SizeT TotalBufferSize = AlignedBlockSize * BlockCount;

private:
    alignas(Alignment) Byte m_buffer[TotalBufferSize];   //<! The contiguous memory buffer for the pool.
    void* m_freeListHead = nullptr;                      //<! Pointer to the head of the free-list chain.
    SizeType m_allocatedCount = 0;                       //<! Number of currently allocated blocks.

public:
    /// \brief Construct the pool and initialize the free-list.
    TPoolAllocator() { InitFreeList(); }

    /// \brief Non-copyable to prevent multiple allocators managing the same pool, which would lead to double-frees and
    /// memory corruption.
    TPoolAllocator(const TPoolAllocator&) = delete;

    /// \brief Move constructor is deleted to prevent moving the allocator, which would invalidate pointers to allocated
    /// blocks.
    TPoolAllocator(TPoolAllocator&&) = delete;

    /// \brief Destructor is defaulted since the pool's memory is managed by a fixed-size buffer and doesn't require
    /// dynamic deallocation.
    ~TPoolAllocator() = default;

public:
    /// \brief Non-copyable assignment operator to prevent multiple allocators managing the same pool, which would lead
    /// to double-frees and memory corruption.
    TPoolAllocator& operator=(const TPoolAllocator&) = delete;

    /// \brief Move assignment operator is deleted to prevent moving the allocator, which would invalidate pointers to
    /// allocated blocks.
    TPoolAllocator& operator=(TPoolAllocator&&) = delete;

public:
    /// \brief Allocate one block from the pool. O(1).
    /// \param size Ignored, always returns BlockSize bytes. Present to satisfy IsAllocator.
    /// \param align Ignored, blocks are pre-aligned.
    /// \return Pointer to the allocated block, or nullptr if the pool is exhausted.
    GP_NODISCARD void* Allocate(SizeType size = BlockSize, SizeType align = Alignment)
    {
        GP_UNUSED(size);
        GP_UNUSED(align);
        GP_ASSERT(size <= BlockSize, "PoolAllocator: requested size %zu exceeds block size %zu", size, BlockSize);

        if (m_freeListHead == nullptr)
        {
            GP_ASSERT(false, "PoolAllocator: pool exhausted (%zu blocks)", BlockCount);
            return nullptr;
        }

        // Pop the first free block off the free-list.
        void* block = m_freeListHead;
        m_freeListHead = *static_cast<void**>(m_freeListHead);
        ++m_allocatedCount;

        return block;
    }

    /// \brief Return a block to the pool. O(1).
    /// \param ptr Pointer to the block to free. Must have been returned by this allocator.
    void Deallocate(void* ptr)
    {
        if (ptr == nullptr) { return; }

        GP_ASSERT(Owns(ptr), "PoolAllocator: pointer does not belong to this pool");

        // Push the block onto the front of the free-list.
        *static_cast<void**>(ptr) = m_freeListHead;
        m_freeListHead = ptr;
        --m_allocatedCount;
    }

    /// \brief Reset the pool, marking all blocks as free. O(N). Does NOT call destructors.
    void Reset()
    {
        m_allocatedCount = 0;
        InitFreeList();
    }

    /// \brief Get the size of each block in bytes.
    /// \return Block size in bytes.
    GP_NODISCARD SizeType GetBlockSize() const noexcept { return BlockSize; }

    /// \brief Get the total number of blocks in the pool.
    /// \return Total block count.
    GP_NODISCARD SizeType GetBlockCount() const noexcept { return BlockCount; }

    /// \brief Get the number of currently allocated blocks.
    /// \return Allocated block count.
    GP_NODISCARD SizeType GetAllocatedCount() const noexcept { return m_allocatedCount; }

    /// \brief Get the number of free blocks available for allocation.
    /// \return Free block count.
    GP_NODISCARD SizeType GetFreeCount() const noexcept { return BlockCount - m_allocatedCount; }

    /// \brief Check if the pool is completely full (no free blocks).
    /// \return True if the pool is full, false otherwise.
    GP_NODISCARD bool IsFull() const noexcept { return m_allocatedCount == BlockCount; }

    /// \brief Check if the pool is completely empty (no allocated blocks).
    /// \return True if the pool is empty, false otherwise.
    GP_NODISCARD bool IsEmpty() const noexcept { return m_allocatedCount == 0; }

    /// \brief Check if a pointer belongs to this pool and is properly aligned.
    /// \param ptr Pointer to check.
    /// \return True if the pointer is owned by this pool, false otherwise.
    GP_NODISCARD bool Owns(const void* ptr) const noexcept
    {
        auto p = static_cast<const Byte*>(ptr);
        auto start = static_cast<const Byte*>(static_cast<const void*>(m_buffer));
        return p >= start && p < start + TotalBufferSize && ((p - start) % AlignedBlockSize == 0);
    }

private:
    /// \brief Build the initial free-list chain through all blocks.
    void InitFreeList()
    {
        Byte* start = reinterpret_cast<Byte*>(m_buffer);
        m_freeListHead = start;

        for (SizeT i = 0; i < BlockCount - 1; ++i)
        {
            Byte* current = start + i * AlignedBlockSize;
            Byte* next = current + AlignedBlockSize;
            *reinterpret_cast<void**>(current) = next;
        }

        // Last block points to nullptr (end of free-list).
        Byte* last = start + (BlockCount - 1) * AlignedBlockSize;
        *reinterpret_cast<void**>(last) = nullptr;
    }
};

}   // namespace GP::Memory
