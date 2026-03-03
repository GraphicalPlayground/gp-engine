// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/Allocators/DefaultAllocator.hpp"
#include <cstdlib>
#include <new>

namespace GP
{

GP_NODISCARD void* FDefaultAllocator::Allocate(SizeT size, SizeT alignment) noexcept
{
    if (size == 0) { return nullptr; }
    alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;

#if GP_PLATFORM_WINDOWS
    void* ptr = _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    if (::posix_memalign(&ptr, alignment, size) != 0) { ptr = nullptr; }
#endif

#if GP_DEBUG
    if (ptr)
    {
        m_totalAllocated += size;
        ++m_allocationCount;
    }
#endif
    return ptr;
}

void FDefaultAllocator::Deallocate(void* ptr, SizeT size) noexcept
{
    if (!ptr) { return; }
#if GP_DEBUG
    m_totalAllocated -= size;
    --m_allocationCount;
#endif
#if GP_PLATFORM_WINDOWS
    _aligned_free(ptr);
#else
    ::free(ptr);
#endif
}

GP_NODISCARD void* FDefaultAllocator::Reallocate(void* ptr, SizeT oldSize, SizeT newSize, SizeT alignment) noexcept
{
#if GP_PLATFORM_WINDOWS
    GP_UNUSED(oldSize);
    alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;
    void* newPtr = _aligned_realloc(ptr, newSize, alignment);
    #if GP_DEBUG
    if (newPtr)
    {
        m_totalAllocated += newSize;
        m_totalAllocated -= oldSize;
    }
    #endif
    return newPtr;
#else
    // Fallback to default implementation which allocates new memory, copies old data, and deallocates old memory.
    return IAllocator::Reallocate(ptr, oldSize, newSize, alignment);
#endif
}

GP_NODISCARD SizeT FDefaultAllocator::GetTotalAllocated() const noexcept { return m_totalAllocated; }

GP_NODISCARD SizeT FDefaultAllocator::GetAllocationCount() const noexcept { return m_allocationCount; }

GP_NODISCARD const char* FDefaultAllocator::GetDebugName() const noexcept { return "DefaultAllocator"; }

GP_NODISCARD FDefaultAllocator& FDefaultAllocator::Get() noexcept
{
    static FDefaultAllocator s_instance;
    return s_instance;
}

}   // namespace GP
