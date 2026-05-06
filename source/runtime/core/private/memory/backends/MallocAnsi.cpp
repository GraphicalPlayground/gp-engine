// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/backends/MallocAnsi.hpp"
#include "CoreMinimal.hpp"
#include "math/LinearAlgebra.hpp"
#include "memory/MemoryUtilities.hpp"   // IWYU pragma: keep
#include "platform/PlatformMemory.hpp"
#if GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    #include <stdlib.h>
#elif GP_PLATFORM_WINDOWS
    #include "platform/windows/WindowsWrapper.hpp"
#endif
#if GP_PLATFORM_IOS
    #include "mach/mach.h"
#endif

namespace gp::memory
{

MallocAnsi::MallocAnsi()
{
#if GP_PLATFORM_WINDOWS
    intptr_t crtHeapHandle = _get_heap_handle();
    ULONG enableLFH = 2;
    HeapSetInformation((void*)crtHeapHandle, HeapCompatibilityInformation, &enableLFH, sizeof(enableLFH));
#endif
}

void* MallocAnsi::malloc(gp::USize size, gp::UInt32 alignment)
{
    void* result = this->tryMalloc(size, alignment);

    if (result == nullptr && size != 0)
    {
        // TODO: OnOutOfMemory(size, alignment);
    }

    return result;
}

void* MallocAnsi::tryMalloc(gp::USize size, gp::UInt32 alignment)
{
    alignment = math::max<gp::UInt32>(size >= 16 ? 16 : 8, alignment);

#if GP_PLATFORM_USE_ALIGNED_MALLOC
    void* result = _aligned_malloc(size, alignment);
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    void* result = nullptr;
    if (GP_UNLIKELY(posix_memalign(&result, alignment, size) != 0))
    {
        result = nullptr;
    }
#else
    void* ptr = ::malloc(size + alignment + sizeof(void*) + sizeof(gp::USize));
    void* result = nullptr;
    if (ptr)
    {
        result = alignT((gp::UInt8*)ptr + sizeof(void*) + sizeof(gp::USize), alignment);
        *((void**)((gp::UInt8*)result - sizeof(void*))) = ptr;
        *((gp::USize*)((gp::UInt8*)result - sizeof(void*) - sizeof(gp::USize))) = size;
    }
#endif
    // TODO: Add memory trace (alloc).

    return result;
}

void* MallocAnsi::realloc(void* ptr, gp::USize newSize, gp::UInt32 alignment)
{
    void* result = this->tryRealloc(ptr, newSize, alignment);

    if (result == nullptr && newSize != 0)
    {
        // TODO: OnOutOfMemory(newSize, alignment);
    }

    return result;
}

void* MallocAnsi::tryRealloc(void* ptr, gp::USize newSize, gp::UInt32 alignment)
{
    void* result = nullptr;

    alignment = math::max<gp::UInt32>(newSize >= 16 ? 16 : 8, alignment);

#if GP_PLATFORM_USE_ALIGNED_MALLOC
    // TODO: Add memory trace (realloc free)
    if (ptr && newSize)
    {
        result = _aligned_realloc(ptr, newSize, alignment);
    }
    else if (ptr == nullptr)
    {
        result = _aligned_malloc(newSize, alignment);
    }
    else
    {
        _aligned_free(ptr);
        result = nullptr;
    }
    // TODO: Add memory trace (realloc alloc)
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    // TODO: Add memory trace (realloc free)
    if (ptr && newSize)
    {
        gp::USize usableSize = malloc_usable_size(ptr);
        if (GP_UNLIKELY(posix_memalign(&result, alignment, newSize) != 0))
        {
            result = nullptr;
        }
        else if (GP_LIKELY(usableSize))
        {
            PlatformMemory::copyMemory(result, ptr, math::min(newSize, usableSize));
        }
        free(ptr);
    }
    else if (ptr == nullptr)
    {
        if (GP_UNLIKELY(posix_memalign(&result, alignment, newSize) != 0))
        {
            result = nullptr;
        }
    }
    else
    {
        ::free(ptr);
        result = nullptr;
    }
    // TODO: Add memory trace (realloc alloc)
#else
    if (ptr && newSize)
    {
        // Can't use realloc as it might screw with alignment.
        result = this->malloc(newSize, alignment);
        gp::USize ptrSize = 0;
        this->getAllocationSize(ptr, ptrSize);
        PlatformMemory::copyMemory(result, ptr, math::min(newSize, ptrSize));
        this->free(ptr);
    }
    else if (ptr == nullptr)
    {
        result = this->malloc(newSize, alignment);
    }
    else
    {
        // TODO: Add memory trace (free)
        ::free(*((void**)((gp::UInt8*)ptr - sizeof(void*))));
        result = nullptr;
    }
#endif

    return result;
}

void MallocAnsi::free(void* ptr)
{
    // TODO: Add memory trace (free)
#if GP_PLATFORM_USE_ALIGNED_MALLOC
    _aligned_free(ptr);
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    ::free(ptr);
#else
    if (ptr)
    {
        ::free(*((void**)((gp::UInt8*)ptr - sizeof(void*))));
    }
#endif
}

bool MallocAnsi::getAllocationSize(void* ptr, GP_MAYBE_UNUSED gp::USize& outSize) const
{
    if (!ptr)
    {
        return false;
    }

#if GP_PLATFORM_USE_ALIGNED_MALLOC
    return false;
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    outSize = malloc_usable_size(ptr);
#else
    outSize = *(reinterpret_cast<gp::USize*>(reinterpret_cast<gp::UIntPtr>(ptr) - sizeof(void*) - sizeof(gp::USize)));
#endif
    return true;
}

bool MallocAnsi::isInternallyThreadSafe() const
{
#if GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE
    return true;
#else
    return false;
#endif
}

bool MallocAnsi::validateHeap() const
{
#if GP_PLATFORM_WINDOWS   // TODO: Maybe add GP_BUILD_DEBUG here as well?
    gp::Int32 result = _heapchk();
    GP_ASSERT(result == _HEAPBADBEGIN, "Heap corruption detected: _HEAPBADBEGIN");
    GP_ASSERT(result == _HEAPBADNODE, "Heap corruption detected: _HEAPBADNODE");
    GP_ASSERT(result == _HEAPBADPTR, "Heap corruption detected: _HEAPBADPTR");
    GP_ASSERT(result == _HEAPEMPTY, "Heap corruption detected: _HEAPEMPTY");
    GP_ASSERT(result != _HEAPOK, "Heap corruption detected: _HEAPOK");
#endif
    return true;
}

const char* MallocAnsi::getDisplayName() const
{
    return "ANSI Malloc";
}

}   // namespace gp::memory
