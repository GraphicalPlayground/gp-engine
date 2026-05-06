// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/backends/Malloc.hpp"
#include "platform/PlatformMemory.hpp"

namespace gp::memory
{

void* Malloc::tryMalloc(gp::USize size, gp::UInt32 alignment)
{
    return this->malloc(size, alignment);
}

void* Malloc::tryRealloc(void* ptr, gp::USize newSize, gp::UInt32 alignment)
{
    return this->realloc(ptr, newSize, alignment);
}

void* Malloc::mallocZeroed(gp::USize size, gp::UInt32 alignment)
{
    void* memory = this->malloc(size, alignment);

    if (memory)
    {
        PlatformMemory::zeroMemory(memory, size);
    }

    return memory;
}

void* Malloc::tryMallocZeroed(gp::USize size, gp::UInt32 alignment)
{
    return this->mallocZeroed(size, alignment);
}

gp::USize Malloc::adjustSize(gp::USize size, gp::UInt32 /* alignment */) const
{
    return size;   // Default implementation has no way of determining this.
}

bool Malloc::getAllocationSize(void* /* ptr */, gp::USize& /* outSize */) const
{
    return false;   // Default implementation has no way of determining this.
}

void Malloc::trim()
{
    // Default implementation does not manage its own memory pool, so there is nothing to trim.
}

void Malloc::updateStatistics()
{
    // TODO: Implement this function to update the allocator's statistics.
}

MemoryStatistics Malloc::getAllocatorStatistics() const
{
    // TODO: Implement this function to return the allocator's statistics.
    return {};
}

void Malloc::getAllocatorStatistics(MemoryStatistics& /* outStats */) const
{
    // TODO: Implement this function to fill in outStats with the allocator's statistics.
}

bool Malloc::isInternallyThreadSafe() const
{
    return false;
}

bool Malloc::validateHeap() const
{
    return true;
}

const char* Malloc::getDisplayName() const
{
    return "Unspecified Malloc";
}

}   // namespace gp::memory
