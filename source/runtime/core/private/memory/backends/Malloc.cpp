// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/backends/Malloc.hpp"
#include "platforms/base/PlatformMemory.hpp"

namespace gp::memory
{

void* Malloc::tryAllocate(USize size, UInt32 alignment) noexcept
{
    return allocate(size, alignment);
}

void* Malloc::allocateZeroed(USize size, UInt32 alignment)
{
    void* const ptr = allocate(size, alignment);
    if (ptr)
    {
        platform::Memory::zeroMemory(ptr, size);
    }
    return ptr;
}

void* Malloc::tryAllocateZeroed(USize size, UInt32 alignment) noexcept
{
    void* const ptr = tryAllocate(size, alignment);
    if (ptr)
    {
        platform::Memory::zeroMemory(ptr, size);
    }
    return ptr;
}

void* Malloc::tryReallocate(void* ptr, USize newSize, UInt32 alignment) noexcept
{
    return reallocate(ptr, newSize, alignment);
}

USize Malloc::getAllocationSize(void* /* ptr */)
{
    return 0;
}

bool Malloc::canGetAllocationSize()
{
    return false;
}

}   // namespace gp::memory
