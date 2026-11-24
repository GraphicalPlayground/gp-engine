// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/PoolAllocationData.hpp"
#include "allocators/MemoryPool.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/MemoryBase.hpp"

namespace gp::rhi::allocator
{

void PoolAllocationData::reset() noexcept
{
    m_size = 0u;
    m_alignment = 0u;
    setAllocationType(AllocationType::Unknown);
    m_poolIndex = static_cast<gp::UInt16>(MemoryPool::kInvalidPoolIndex);
    m_offset = 0u;
    m_locked = 0u;

    m_owner = nullptr;
    m_previousAllocation = nullptr;
    m_nextAllocation = nullptr;
    m_aliasAllocation = nullptr;
}

void PoolAllocationData::initAsHead(const gp::Int16 poolIndex) noexcept
{
    reset();

    setAllocationType(AllocationType::Head);
    m_nextAllocation = this;
    m_previousAllocation = this;
    m_poolIndex = static_cast<gp::UInt16>(poolIndex);
}

void PoolAllocationData::initAsFree(
    const gp::Int16 poolIndex, const gp::UInt32 size, const gp::UInt32 alignment, const gp::UInt32 offset
) noexcept
{
    reset();

    m_size = size;
    m_alignment = alignment;
    setAllocationType(AllocationType::Free);
    m_offset = offset;
    m_poolIndex = static_cast<gp::UInt16>(poolIndex);
}

void PoolAllocationData::initAsAllocated(
    const gp::UInt32 size,
    const gp::UInt32 poolAlignment,
    const gp::UInt32 allocationAlignment,
    PoolAllocationData* const freeBlock
) noexcept
{
    GP_CHECK(freeBlock != nullptr && freeBlock->isFree());

    reset();

    m_size = size;
    m_alignment = allocationAlignment;
    setAllocationType(AllocationType::Allocated);
    m_offset = MemoryPool::getAlignedOffset(freeBlock->m_offset, poolAlignment, allocationAlignment);
    m_poolIndex = freeBlock->m_poolIndex;
    m_locked = 1u;

    const gp::UInt32 alignedSize = MemoryPool::getAlignedSize(size, poolAlignment, allocationAlignment);
    freeBlock->m_size -= alignedSize;
    freeBlock->m_offset += alignedSize;
    freeBlock->addBefore(this);
}

void PoolAllocationData::moveFrom(PoolAllocationData& allocated, const bool locked) noexcept
{
    GP_CHECK(allocated.isAllocated());

    reset();

    m_size = allocated.m_size;
    m_alignment = allocated.m_alignment;
    m_type = allocated.m_type;
    m_offset = allocated.m_offset;
    m_poolIndex = allocated.m_poolIndex;
    m_locked = locked ? 1u : 0u;

    // Relink into the pool block list when moving a primary allocation.
    if (allocated.m_previousAllocation != nullptr)
    {
        allocated.m_previousAllocation->m_nextAllocation = this;
        allocated.m_nextAllocation->m_previousAllocation = this;
        m_previousAllocation = allocated.m_previousAllocation;
        m_nextAllocation = allocated.m_nextAllocation;

        // Point any attached aliases to the new primary block.
        for (PoolAllocationData* alias = allocated.getFirstAlias(); alias != nullptr; alias = alias->getNext())
        {
            alias->m_aliasAllocation = this;
        }
        m_aliasAllocation = allocated.m_aliasAllocation;
    }
    else
    {
        // No pool list links means this node is an alias attached to a shared multi-GPU backing allocation.
        // Swap the old alias node out of its parent list and register this node in its place.
        PoolAllocationData* const aliasParent = allocated.m_aliasAllocation;
        GP_CHECK(aliasParent != nullptr);

        allocated.removeAlias();
        aliasParent->addAlias(this);
    }

    allocated.reset();
}

void PoolAllocationData::markFree(const gp::UInt32 poolAlignment, const gp::UInt32 allocationAlignment) noexcept
{
    GP_CHECK(isAllocated());

    setAllocationType(AllocationType::Free);
    m_size = MemoryPool::getAlignedSize(m_size, poolAlignment, allocationAlignment);
    m_offset = gp::memory::alignDown(m_offset, poolAlignment);
    m_alignment = poolAlignment;
}

void PoolAllocationData::merge(PoolAllocationData* const other) noexcept
{
    GP_CHECK(other != nullptr);
    GP_CHECK(isFree() && other->isFree());
    GP_CHECK((m_offset + m_size) == other->m_offset);
    GP_CHECK(getPoolIndex() == other->getPoolIndex());

    m_size += other->m_size;

    other->removeFromLinkedList();
    other->reset();
}

void PoolAllocationData::removeFromLinkedList() noexcept
{
    GP_CHECK(isFree());
    m_previousAllocation->m_nextAllocation = m_nextAllocation;
    m_nextAllocation->m_previousAllocation = m_previousAllocation;
}

void PoolAllocationData::addBefore(PoolAllocationData* const other) noexcept
{
    m_previousAllocation->m_nextAllocation = other;
    other->m_previousAllocation = m_previousAllocation;

    m_previousAllocation = other;
    other->m_nextAllocation = this;
}

void PoolAllocationData::addAfter(PoolAllocationData* const other) noexcept
{
    m_nextAllocation->m_previousAllocation = other;
    other->m_nextAllocation = m_nextAllocation;

    m_nextAllocation = other;
    other->m_previousAllocation = this;
}

// A primary allocation stores the head of its alias list in m_aliasAllocation.
// Aliases link to siblings via m_nextAllocation/m_previousAllocation and point back to the primary via
// m_aliasAllocation.
void PoolAllocationData::addAlias(PoolAllocationData* const other) noexcept
{
    if (m_aliasAllocation != nullptr)
    {
        m_aliasAllocation->m_previousAllocation = other;
    }

    other->m_nextAllocation = m_aliasAllocation;
    other->m_previousAllocation = nullptr;
    other->m_aliasAllocation = this;

    m_aliasAllocation = other;
}

void PoolAllocationData::removeAlias() noexcept
{
    if (m_aliasAllocation != nullptr)
    {
        if (m_previousAllocation != nullptr)
        {
            m_previousAllocation->m_nextAllocation = m_nextAllocation;
        }
        else
        {
            // Removing the head of the alias list.
            m_aliasAllocation->m_aliasAllocation = m_nextAllocation;
        }

        if (m_nextAllocation != nullptr)
        {
            m_nextAllocation->m_previousAllocation = m_previousAllocation;
        }

        m_nextAllocation = nullptr;
        m_previousAllocation = nullptr;
        m_aliasAllocation = nullptr;
    }
}

}   // namespace gp::rhi::allocator
