// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/MemoryOverrides.hpp"

GP_NODISCARD void* operator new(std::size_t size)
{
    return gp::memory::getGlobalMalloc()->allocate(size);
}

GP_NODISCARD void* operator new[](std::size_t size)
{
    return gp::memory::getGlobalMalloc()->allocate(size);
}

GP_NODISCARD void* operator new(std::size_t size, std::align_val_t alignment)
{
    return gp::memory::getGlobalMalloc()->allocate(size, static_cast<gp::UInt32>(alignment));
}

GP_NODISCARD void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return gp::memory::getGlobalMalloc()->allocate(size, static_cast<gp::UInt32>(alignment));
}

void operator delete(void* ptr) noexcept
{
    if (ptr != nullptr)
    {
        gp::memory::getGlobalMalloc()->deallocate(ptr);
    }
}

void operator delete[](void* ptr) noexcept
{
    if (ptr != nullptr)
    {
        gp::memory::getGlobalMalloc()->deallocate(ptr);
    }
}

void operator delete(void* ptr, std::size_t /* size */) noexcept
{
    if (ptr != nullptr)
    {
        gp::memory::getGlobalMalloc()->deallocate(ptr);
    }
}

void operator delete[](void* ptr, std::size_t /* size */) noexcept
{
    if (ptr != nullptr)
    {
        gp::memory::getGlobalMalloc()->deallocate(ptr);
    }
}

void operator delete(void* ptr, std::align_val_t /* alignment */) noexcept
{
    if (ptr != nullptr)
    {
        gp::memory::getGlobalMalloc()->deallocate(ptr);
    }
}

void operator delete[](void* ptr, std::align_val_t /* alignment */) noexcept
{
    if (ptr != nullptr)
    {
        gp::memory::getGlobalMalloc()->deallocate(ptr);
    }
}
