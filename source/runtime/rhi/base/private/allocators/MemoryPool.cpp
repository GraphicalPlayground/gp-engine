// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/MemoryPool.hpp"
#include "allocators/PoolAllocator.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/MemoryBase.hpp"
#include "profiling/Profiler.hpp"
#include "templates/NumericLimits.hpp"

namespace gp::rhi::allocator
{

namespace
{

constexpr gp::Int32 kDesiredAllocationPoolSize = 32;
constexpr gp::Int32 kInvalidFreeBlockIndex = -1;

[[nodiscard]] bool
    sortBySizePredicate(const PoolAllocationData* const lhs, const PoolAllocationData* const rhs) noexcept
{
    return lhs->getSize() < rhs->getSize();
}

[[nodiscard]] bool
    sortByOffsetPredicate(const PoolAllocationData* const lhs, const PoolAllocationData* const rhs) noexcept
{
    return lhs->getOffset() < rhs->getOffset();
}

}   // namespace

gp::UInt32 MemoryPool::getAlignedSize(
    const gp::UInt32 sizeInBytes, const gp::UInt32 poolAlignment, const gp::UInt32 allocationAlignment
) noexcept
{
    GP_CHECK(allocationAlignment <= poolAlignment);

    // Add one allocation alignment quantum when pool alignment is not an even multiple of allocation alignment.
    const gp::UInt32 size =
        ((poolAlignment % allocationAlignment) != 0u) ? (sizeInBytes + allocationAlignment) : sizeInBytes;
    return gp::memory::alignArbitrary(size, poolAlignment);
}

gp::UInt32 MemoryPool::getAlignedOffset(
    const gp::UInt32 offset, const gp::UInt32 poolAlignment, const gp::UInt32 allocationAlignment
) noexcept
{
    gp::UInt32 alignedOffset = offset;

    if ((poolAlignment % allocationAlignment) != 0u)
    {
        const gp::UInt32 alignmentRest = alignedOffset % allocationAlignment;
        if (alignmentRest > 0u)
        {
            alignedOffset += (allocationAlignment - alignmentRest);
        }
    }

    return alignedOffset;
}

MemoryPool::MemoryPool(
    const gp::Int16 poolIndex,
    const gp::UInt64 poolSize,
    const gp::UInt32 poolAlignment,
    const PoolResourceTypes supportedResourceTypes,
    const PoolFreeListOrder freeListOrder
) noexcept
    : m_poolIndex(poolIndex)
    , m_poolSize(poolSize)
    , m_poolAlignment(poolAlignment)
    , m_supportedResourceTypes(supportedResourceTypes)
    , m_freeListOrder(freeListOrder)
    , m_freeSize(0u)
    , m_alignmentWaste(0u)
    , m_allocatedBlocks(0u)
{
    // PoolAllocationData stores sizes and offsets in 32-bit integers.
    GP_CHECK(m_poolSize < gp::NumericLimits<gp::UInt32>::max());

    // Keep the head block unlinked until initFreeBlocks() runs so validate() and destroy() can detect an active list.
    m_headBlock.reset();
}

MemoryPool::~MemoryPool() noexcept
{
    destroy();
}

void MemoryPool::init() noexcept
{
    m_freeSize = m_poolSize;

    m_allocationDataPool.reserve(static_cast<gp::USize>(kDesiredAllocationPoolSize));
    for (gp::Int32 index = 0; index < kDesiredAllocationPoolSize; ++index)
    {
        PoolAllocationData* const allocationData = new PoolAllocationData();
        allocationData->reset();
        m_allocationDataPool.pushBack(allocationData);
    }

    // Asynchronously pre-allocated pools receive their pool index later when claimed.
    if (m_poolIndex != kInvalidPoolIndex)
    {
        initFreeBlocks();
    }
}

void MemoryPool::initFreeBlocks() noexcept
{
    GP_CHECK(m_poolIndex != kInvalidPoolIndex);

    m_headBlock.initAsHead(m_poolIndex);
    PoolAllocationData* const freeBlock = getNewAllocationData();
    freeBlock->initAsFree(m_poolIndex, static_cast<gp::UInt32>(m_poolSize), m_poolAlignment, 0u);
    m_headBlock.addAfter(freeBlock);
    m_freeBlocks.pushBack(freeBlock);

    validate();
}

void MemoryPool::destroy() noexcept
{
    // Validate before releasing nodes so the linked list still points at live memory.
    validate();

    for (PoolAllocationData* const freeBlock: m_freeBlocks)
    {
        releaseAllocationData(freeBlock);
    }
    m_freeBlocks.clear();

    for (PoolAllocationData* const allocationData: m_allocationDataPool)
    {
        delete allocationData;
    }
    m_allocationDataPool.clear();

    // Unlink so a second destroy() call does not walk released blocks.
    m_headBlock.reset();
}

bool MemoryPool::tryAllocate(
    const gp::UInt32 sizeInBytes,
    const gp::UInt32 allocationAlignment,
    const PoolResourceTypes allocationResourceType,
    PoolAllocationData& allocationData
) noexcept
{
    GP_CHECK(isResourceTypeSupported(allocationResourceType));

    const gp::Int32 freeBlockIndex = findFreeBlock(sizeInBytes, allocationAlignment);
    if (freeBlockIndex != kInvalidFreeBlockIndex)
    {
        const gp::UInt32 alignedSize = getAlignedSize(sizeInBytes, m_poolAlignment, allocationAlignment);

        PoolAllocationData* const freeBlock = m_freeBlocks[static_cast<gp::USize>(freeBlockIndex)];
        GP_CHECK(freeBlock->getSize() >= alignedSize);

        // Remove before shrinking because the remaining block may need to move to a new sorted position.
        m_freeBlocks.removeAt(static_cast<gp::USize>(freeBlockIndex));

        allocationData.initAsAllocated(sizeInBytes, m_poolAlignment, allocationAlignment, freeBlock);
        GP_CHECK((allocationData.getOffset() % allocationAlignment) == 0u);

        GP_CHECK(m_freeSize >= alignedSize);
        m_freeSize -= alignedSize;
        m_alignmentWaste += (alignedSize - sizeInBytes);
        ++m_allocatedBlocks;

        if (freeBlock->getSize() == 0u)
        {
            freeBlock->removeFromLinkedList();
            releaseAllocationData(freeBlock);

            validate();
        }
        else
        {
            addToFreeBlocks(freeBlock);
        }

        return true;
    }

    return false;
}

void MemoryPool::deallocate(PoolAllocationData& allocationData) noexcept
{
    GP_CHECK(allocationData.isLocked());
    GP_CHECK(m_poolIndex == allocationData.getPoolIndex());

    const gp::UInt32 allocationAlignment = allocationData.getAlignment();
    const bool locked = false;
    const gp::UInt64 allocationSize = allocationData.getSize();

    PoolAllocationData* const freeBlock = getNewAllocationData();
    freeBlock->moveFrom(allocationData, locked);
    freeBlock->markFree(m_poolAlignment, allocationAlignment);

    m_freeSize += freeBlock->getSize();
    m_alignmentWaste -= (freeBlock->getSize() - allocationSize);
    --m_allocatedBlocks;

    addToFreeBlocks(freeBlock);
}

void MemoryPool::tryClear(
    const ContextArray& contexts,
    PoolAllocator* const allocator,
    const gp::UInt32 maxCopySize,
    gp::UInt32& copySize,
    const gp::Vector<MemoryPool*>& targetPools
) noexcept
{
    GP_PROFILE_SCOPE_N("MemoryPool::tryClear");

    PoolAllocationData* blockToMove = m_headBlock.getPrev();
    while (blockToMove != &m_headBlock && copySize < maxCopySize)
    {
        PoolAllocationData* const nextBlockToMove = blockToMove->getPrev();

        if (blockToMove->isAllocated() && !blockToMove->isLocked() && blockToMove->getOwner() != nullptr)
        {
            const gp::UInt32 sizeToAllocate = blockToMove->getSize();
            const gp::UInt32 allocationAlignment = blockToMove->getAlignment();

            PoolAllocationData tempTargetAllocation;

            for (MemoryPool* const targetPool: targetPools)
            {
                if (targetPool->tryAllocate(
                        sizeToAllocate, allocationAlignment, m_supportedResourceTypes, tempTargetAllocation
                    ))
                {
                    if (allocator->handleDefragRequest(contexts, blockToMove, tempTargetAllocation))
                    {
                        // Charge the copy budget only when the backend accepted the move.
                        copySize += blockToMove->getSize();
                    }

                    break;
                }
            }
        }

        blockToMove = nextBlockToMove;
    }

    validate();
}

gp::Int32 MemoryPool::findFreeBlock(const gp::UInt32 sizeInBytes, const gp::UInt32 allocationAlignment) const noexcept
{
    const gp::UInt32 alignedSize = getAlignedSize(sizeInBytes, m_poolAlignment, allocationAlignment);

    if (m_freeSize < alignedSize || m_freeBlocks.isEmpty())
    {
        return kInvalidFreeBlockIndex;
    }

    if (m_freeListOrder == PoolFreeListOrder::SortBySize)
    {
        // Fast path when the smallest free block already fits.
        if (m_freeBlocks[0u]->getSize() >= alignedSize)
        {
            return 0;
        }

        gp::USize low = 0u;
        gp::USize high = m_freeBlocks.size();
        while (low < high)
        {
            const gp::USize mid = low + ((high - low) / 2u);
            if (m_freeBlocks[mid]->getSize() < alignedSize)
            {
                low = mid + 1u;
            }
            else
            {
                high = mid;
            }
        }

        if (low < m_freeBlocks.size())
        {
            return static_cast<gp::Int32>(low);
        }
    }
    else
    {
        for (gp::USize freeBlockIndex = 0u; freeBlockIndex < m_freeBlocks.size(); ++freeBlockIndex)
        {
            if (m_freeBlocks[freeBlockIndex]->getSize() >= alignedSize)
            {
                return static_cast<gp::Int32>(freeBlockIndex);
            }
        }
    }

    return kInvalidFreeBlockIndex;
}

void MemoryPool::removeFromFreeBlocks(PoolAllocationData* const freeBlock) noexcept
{
    for (gp::USize freeBlockIndex = 0u; freeBlockIndex < m_freeBlocks.size(); ++freeBlockIndex)
    {
        if (m_freeBlocks[freeBlockIndex] == freeBlock)
        {
            m_freeBlocks.removeAt(freeBlockIndex);
            break;
        }
    }
}

PoolAllocationData* MemoryPool::addToFreeBlocks(PoolAllocationData* const freeBlock) noexcept
{
    GP_CHECK(freeBlock != nullptr && freeBlock->isFree());
    GP_CHECK(gp::memory::isAligned(freeBlock->getSize(), m_poolAlignment));
    GP_CHECK(gp::memory::isAligned(freeBlock->getOffset(), m_poolAlignment));

    PoolAllocationData* mergedBlock = freeBlock;

    // Coalesce with the preceding block if it is free and unlocked.
    if (mergedBlock->getPrev()->isFree() && !mergedBlock->getPrev()->isLocked())
    {
        PoolAllocationData* const prevFree = mergedBlock->getPrev();
        prevFree->merge(mergedBlock);
        removeFromFreeBlocks(prevFree);
        releaseAllocationData(mergedBlock);

        mergedBlock = prevFree;
    }

    // Coalesce with the following block if it is free and unlocked.
    if (mergedBlock->getNext()->isFree() && !mergedBlock->getNext()->isLocked())
    {
        PoolAllocationData* const nextFree = mergedBlock->getNext();
        mergedBlock->merge(nextFree);
        removeFromFreeBlocks(nextFree);
        releaseAllocationData(nextFree);
    }

    gp::USize insertIndex = 0u;
    switch (m_freeListOrder)
    {
    case PoolFreeListOrder::SortBySize:
    {
        insertIndex = m_freeBlocks.lowerBound(mergedBlock, sortBySizePredicate);
        break;
    }
    case PoolFreeListOrder::SortByOffset:
    {
        insertIndex = m_freeBlocks.lowerBound(mergedBlock, sortByOffsetPredicate);
        break;
    }
    default:
        break;
    }

    m_freeBlocks.insertAt(insertIndex, mergedBlock);

    validate();

    return mergedBlock;
}

PoolAllocationData* MemoryPool::getNewAllocationData() noexcept
{
    if (!m_allocationDataPool.isEmpty())
    {
        PoolAllocationData* const data = m_allocationDataPool.back();
        m_allocationDataPool.popBack();
        return data;
    }

    return new PoolAllocationData();
}

void MemoryPool::releaseAllocationData(PoolAllocationData* const data) noexcept
{
    data->reset();
    if (m_allocationDataPool.size() >= static_cast<gp::USize>(kDesiredAllocationPoolSize))
    {
        delete data;
    }
    else
    {
        m_allocationDataPool.pushBack(data);
    }
}

void MemoryPool::validate() noexcept
{
    if (!PoolAllocator::isLinkedListValidationEnabled())
    {
        return;
    }

    if (m_poolIndex == kInvalidPoolIndex)
    {
        return;
    }

    // Skip when free blocks have not been initialized yet or were already cleared by destroy().
    if (m_headBlock.getNext() == nullptr)
    {
        return;
    }

    gp::UInt64 totalFreeSize = 0u;
    gp::UInt64 totalAllocatedSize = 0u;
    gp::UInt64 currentOffset = 0u;

    PoolAllocationData* currentBlock = m_headBlock.getNext();
    while (currentBlock != &m_headBlock)
    {
        GP_CHECK(currentBlock == currentBlock->getNext()->getPrev());
        GP_CHECK(currentBlock == currentBlock->getPrev()->getNext());

        const gp::UInt32 alignedSize =
            getAlignedSize(currentBlock->getSize(), m_poolAlignment, currentBlock->getAlignment());
        const gp::UInt32 alignedOffset = gp::memory::alignDown(currentBlock->getOffset(), m_poolAlignment);

        GP_CHECK(alignedOffset == currentOffset);

        if (currentBlock->isAllocated())
        {
            totalAllocatedSize += alignedSize;
        }
        else if (currentBlock->isFree())
        {
            totalFreeSize += alignedSize;
        }
        else
        {
            GP_CHECK(false);
        }

        currentOffset += alignedSize;
        currentBlock = currentBlock->getNext();
    }

    GP_CHECK(totalFreeSize + totalAllocatedSize == m_poolSize);
    GP_CHECK(totalFreeSize == m_freeSize);
    GP_CHECK(totalAllocatedSize == getUsedSize());

    totalFreeSize = 0u;
    for (gp::USize freeBlockIndex = 0u; freeBlockIndex < m_freeBlocks.size(); ++freeBlockIndex)
    {
        PoolAllocationData* const freeBlock = m_freeBlocks[freeBlockIndex];
        GP_CHECK(!freeBlock->getPrev()->isFree() || freeBlock->getPrev()->isLocked());
        GP_CHECK(!freeBlock->getNext()->isFree() || freeBlock->getNext()->isLocked());
        GP_CHECK(
            m_freeListOrder != PoolFreeListOrder::SortBySize || freeBlockIndex == (m_freeBlocks.size() - 1u) ||
            freeBlock->getSize() <= m_freeBlocks[freeBlockIndex + 1u]->getSize()
        );
        GP_CHECK(gp::memory::isAligned(freeBlock->getOffset(), m_poolAlignment));
        totalFreeSize += freeBlock->getSize();
    }

    GP_CHECK(totalFreeSize == m_freeSize);
}

}   // namespace gp::rhi::allocator
