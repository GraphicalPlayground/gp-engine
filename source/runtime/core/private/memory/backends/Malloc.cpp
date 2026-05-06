// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/backends/Malloc.hpp"

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

void* Malloc::mallocZeroed(gp::USize /* size */, gp::UInt32 /* alignment */)
{
    return nullptr;
}

void* Malloc::tryMallocZeroed(gp::USize /* size */, gp::UInt32 /* alignment */)
{
    return nullptr;
}

gp::USize Malloc::adjustSize(gp::USize size, gp::UInt32 /* alignment */) const
{
    return size;   // Default implementation has no way of determining this.
}

gp::USize Malloc::getAllocationSize(void* /* ptr */, gp::USize& /* outSize */) const
{
    return false;   // Default implementation has no way of determining this.
}

void Malloc::trim()
{}

void Malloc::updateStatistics()
{}

MemoryStatistics Malloc::getAllocatorStatistics() const
{
    return {};
}

void Malloc::getAllocatorStatistics(MemoryStatistics& /* outStats */) const
{}

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
