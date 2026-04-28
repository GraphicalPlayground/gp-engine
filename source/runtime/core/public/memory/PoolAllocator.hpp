// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/Allocator.hpp"
#include "memory/Utilities.hpp"

namespace gp::memory
{

/// @brief Fixed-size block allocator using an intrusive free-list.
/// @details
/// Manages a pre-allocated buffer divided into equal-sized blocks. Allocation and deallocation are O(1), a single
/// pointer pop/push on an embedded singly-linked list. Freed blocks are recycled immediately with zero fragmentation.
///
/// The minimum block size is sizeof(void*), since each free block stores the intrusive next-pointer. Blocks are aligned
/// to the requested alignment at construction time.
///
/// Not thread-safe. Synchronization is the caller's responsibility.
///
/// Hot data layout (first 32 bytes on x64): `m_freeList`, `m_buffer`, `m_blockSize`, `m_BlockAlignment` Single cache
/// line for the pop (allocate) fast path.
class PoolAllocator final : public Allocator
{
private:
    /// @brief Intrusive node embedded in every free block.
    struct FreeNode
    {
        FreeNode* next;
    };

private:
    FreeNode* m_freeList{ nullptr };
    gp::Byte* m_buffer{ nullptr };
    USize m_blockSize{ 0ull };
    USize m_blockAlignment{ 0ull };
    USize m_blockCount{ 0ull };
    USize m_allocatedCount{ 0ull };

public:
    /// @brief Constructs a pool allocator over an externally owned buffer.
    /// @param[in] buffer Pointer to the pre-allocated memory region.
    /// @param[in] bufferSize Total size of the buffer in bytes.
    /// @param[in] blockSize Size of each allocation block in bytes. Clamped to a minimum of sizeof(void*) to hold the
    ///                      free-list pointer.
    /// @param[in] blockAlignment Alignment of each block (must be a power of two).
    PoolAllocator(
        void* buffer, USize bufferSize, USize blockSize, USize blockAlignment = alignof(gp::MaxAlign)
    ) noexcept
        : m_freeList(nullptr)
        , m_buffer(static_cast<gp::Byte*>(buffer))
        , m_blockSize(blockSize < sizeof(FreeNode) ? sizeof(FreeNode) : blockSize)
        , m_blockAlignment(blockAlignment < alignof(FreeNode) ? alignof(FreeNode) : blockAlignment)
    {
        // Ensure the effective block stride is aligned
        const USize stride = alignUp(m_blockSize, m_blockAlignment);

        // Find the first aligned address within the buffer
        const USize bufferAddr = reinterpret_cast<USize>(m_buffer);
        const USize alignedStart = alignUp(bufferAddr, m_blockAlignment);
        const USize startOffset = alignedStart - bufferAddr;

        // Calculate how many blocks fit
        if (startOffset < bufferSize)
        {
            m_blockCount = (bufferSize - startOffset) / stride;
        }

        // Build the free list in reverse order so the first block is at the head
        gp::Byte* blockPtr = m_buffer + startOffset + (m_blockCount > 0 ? (m_blockCount - 1) * stride : 0);
        for (USize i = 0; i < m_blockCount; ++i)
        {
            auto* node = reinterpret_cast<FreeNode*>(blockPtr);
            node->next = m_freeList;
            m_freeList = node;
            blockPtr -= stride;
        }
    }

    ~PoolAllocator() override = default;

public:
    /// @brief Pops a block from the free-list.
    /// @param[in] size Must be <= block_size. Ignored in release; asserted in debug.
    /// @param[in] alignment Must be <= block_alignment. Ignored in release; asserted in debug.
    /// @return Pointer to the allocated block, or nullptr if the pool is exhausted.
    GP_NODISCARD void*
        allocate(GP_MAYBE_UNUSED USize size, GP_MAYBE_UNUSED USize alignment = alignof(gp::MaxAlign)) noexcept override
    {
        GP_ASSERT(size <= m_blockSize, "pool_allocator: requested size exceeds block size");
        GP_ASSERT(alignment <= m_blockAlignment, "pool_allocator: requested alignment exceeds block alignment");

        if GP_UNLIKELY (!m_freeList)
        {
            return nullptr;
        }

        FreeNode* node = m_freeList;
        m_freeList = node->next;
        ++m_allocatedCount;

        return static_cast<void*>(node);
    }

    /// @brief Pushes a block back onto the free-list.
    /// @param[in] ptr Pointer to a block previously returned by allocate().
    /// @param[in] size Ignored (blocks are fixed-size).
    void deallocate(void* ptr, GP_MAYBE_UNUSED USize size) noexcept override
    {
        if (!ptr)
        {
            return;
        }

        GP_ASSERT(owns(ptr), "pool_allocator: deallocating a pointer not owned by this pool");

        auto* node = static_cast<FreeNode*>(ptr);
        node->next = m_freeList;
        m_freeList = node;
        --m_allocatedCount;
    }

    /// @brief Rebuilds the free-list, reclaiming all blocks.
    /// @details All previously returned pointers become invalid.
    void reset() noexcept override
    {
        m_freeList = nullptr;
        m_allocatedCount = 0;

        const USize stride = alignUp(m_blockSize, m_blockAlignment);
        const USize bufferAddr = reinterpret_cast<USize>(m_buffer);
        const USize alignedStart = alignUp(bufferAddr, m_blockAlignment);
        const USize startOffset = alignedStart - bufferAddr;

        gp::Byte* blockPtr = m_buffer + startOffset + (m_blockCount > 0 ? (m_blockCount - 1) * stride : 0);
        for (USize i = 0; i < m_blockCount; ++i)
        {
            auto* node = reinterpret_cast<FreeNode*>(blockPtr);
            node->next = m_freeList;
            m_freeList = node;
            blockPtr -= stride;
        }
    }

    /// @brief Checks whether the given pointer falls within this pool's buffer.
    /// @param[in] ptr Pointer to query.
    /// @return True if ptr is within the buffer region.
    GP_NODISCARD bool owns(const void* ptr) const noexcept override
    {
        const auto* p = static_cast<const gp::Byte*>(ptr);
        return p >= m_buffer && p < m_buffer + getBufferSize();
    }

    /// @brief Returns the configured block size (may be larger than the requested size).
    GP_NODISCARD GP_FORCEINLINE USize getBlockSize() const noexcept
    {
        return m_blockSize;
    }

    /// @brief Returns the total number of blocks in the pool.
    GP_NODISCARD GP_FORCEINLINE USize getBlockCount() const noexcept
    {
        return m_blockCount;
    }

    /// @brief Returns the number of blocks currently available (not allocated).
    GP_NODISCARD GP_FORCEINLINE USize getFreeCount() const noexcept
    {
        return m_blockCount - m_allocatedCount;
    }

    GP_NODISCARD USize getAllocatedSize() const noexcept override
    {
        return m_allocatedCount * m_blockSize;
    }

    GP_NODISCARD USize getAllocationCount() const noexcept override
    {
        return m_allocatedCount;
    }

    GP_NODISCARD const char* getDebugName() const noexcept override
    {
        return "PoolAllocator";
    }

private:
    /// @brief Computes the total used buffer size from block geometry.
    GP_NODISCARD GP_FORCEINLINE USize getBufferSize() const noexcept
    {
        const USize stride = alignUp(m_blockSize, m_blockAlignment);
        const USize bufferAddr = reinterpret_cast<USize>(m_buffer);
        const USize startOffset = alignUp(bufferAddr, m_blockAlignment) - bufferAddr;
        return startOffset + m_blockCount * stride;
    }
};

}   // namespace gp::memory
