// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/backends/MallocAnsi.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/Memory.hpp"
#include "profiling/Profiler.hpp"
#if GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    #include <malloc.h>
#endif
#if GP_PLATFORM_WINDOWS
    #include <windows.h>   // TODO: Add a wrapper around windows.h and include that instead
#endif

namespace gp::memory
{

namespace ansi
{

[[nodiscard]] static void* allocate(USize size, UInt32 alignment)
{
#if GP_PLATFORM_USE_ALIGNED_MALLOC
    void* ptr = _aligned_malloc(size, alignment);
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) [[unlikely]]
    {
        ptr = nullptr;
    }
#elif GP_PLATFORM_USE_ANSI_MEMALIGN
    void* ptr = memalign(alignment, size);
#else
    // alignment - 1 is the maximum alignment waste (not alignment)
    void* temp = ::malloc(size + alignment - 1 + sizeof(void*) + sizeof(USize));
    void* ptr = nullptr;
    if (temp) [[likely]]
    {
        ptr = memory::align(static_cast<UInt8*>(temp) + sizeof(void*) + sizeof(USize), alignment);
        *reinterpret_cast<void**>(static_cast<UInt8*>(ptr) - sizeof(void*)) = temp;
        *reinterpret_cast<USize*>(static_cast<UInt8*>(ptr) - sizeof(void*) - sizeof(USize)) = size;
    }
#endif

    GP_MEM_ALLOC_N(ptr, size, "MallocAnsi");

    return ptr;
}

[[nodiscard]] [[maybe_unused]] static USize getAllocationSize(void* ptr)
{
#if GP_PLATFORM_USE_ALIGNED_MALLOC
    // TODO: We assume that the alignment is always 16, but this may not be the case.
    return _aligned_msize(ptr, 16, 0);
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC || GP_PLATFORM_USE_ANSI_MEMALIGN
    return malloc_usable_size(ptr);
#else
    return *reinterpret_cast<USize*>(static_cast<UInt8*>(ptr) - sizeof(void*) - sizeof(USize));
#endif
}

static void deallocate(void* ptr)
{
    GP_MEM_FREE_N(ptr, "MallocAnsi");

#if GP_PLATFORM_USE_ALIGNED_MALLOC
    _aligned_free(ptr);
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC || GP_PLATFORM_USE_ANSI_MEMALIGN
    ::free(ptr);
#else
    if (ptr)
    {
        ::free(*reinterpret_cast<void**>(static_cast<UInt8*>(ptr) - sizeof(void*)));
    }
#endif
}

[[nodiscard]] static void* reallocate(void* ptr, USize newSize, UInt32 alignment)
{
    void* newPtr = nullptr;

#if GP_PLATFORM_USE_ALIGNED_MALLOC
    GP_MEM_FREE_N(ptr, "MallocAnsi");
    if (ptr && newSize != 0)
    {
        newPtr = _aligned_realloc(ptr, newSize, alignment);
    }
    else if (ptr == nullptr)
    {
        newPtr = _aligned_malloc(newSize, alignment);
    }
    else /* newSize == 0 */
    {
        _aligned_free(ptr);
        newPtr = nullptr;
    }
    GP_MEM_ALLOC_N(newPtr, newSize, "MallocAnsi");
#elif GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    GP_MEM_FREE_N(ptr, "MallocAnsi");
    if (ptr && newSize != 0)
    {
        USize usableSize = malloc_usable_size(ptr);
        // Reuse the existing block when it already fits and satisfies the new alignment.
        // posix_memalign always produces aligned results so the runtime check is only
        // needed when the caller requests a stricter alignment than was originally used.
        if (newSize <= usableSize && (reinterpret_cast<uintptr_t>(ptr) % alignment == 0))
        {
            return ptr;
        }
        if (posix_memalign(&newPtr, alignment, newSize) != 0) [[unlikely]]
        {
            newPtr = nullptr;
        }
        else
        {
            memory::copyMemory(newPtr, ptr, math::min(newSize, usableSize));
            ::free(ptr);
        }
    }
    else if (ptr == nullptr)
    {
        if (posix_memalign(&newPtr, alignment, newSize) != 0) [[unlikely]]
        {
            newPtr = nullptr;
        }
    }
    else /* newSize == 0 */
    {
        ::free(ptr);
        newPtr = nullptr;
    }
    GP_MEM_ALLOC_N(newPtr, newSize, "MallocAnsi");
#elif GP_PLATFORM_USE_ANSI_MEMALIGN
    GP_MEM_FREE_N(ptr, "MallocAnsi");
    newPtr = reallocalign(ptr, newSize, alignment);
    GP_MEM_ALLOC_N(newPtr, newSize, "MallocAnsi");
#else
    if (ptr && newSize != 0)
    {
        USize oldSize = getAllocationSize(ptr);
        newPtr = ansi::allocate(newSize, alignment);
        if (newPtr) [[likely]]
        {
            memory::copyMemory(newPtr, ptr, math::min(newSize, oldSize));
            ansi::deallocate(ptr);
        }
    }
    else if (ptr == nullptr)
    {
        newPtr = ansi::allocate(newSize, alignment);
    }
    else /* newSize == 0 */
    {
        ansi::deallocate(ptr);
        newPtr = nullptr;
    }
#endif

    return newPtr;
}

}   // namespace ansi

void* MallocAnsi::allocate(USize size, UInt32 alignment)
{
    void* ptr = tryAllocate(size, alignment);
    if (ptr == nullptr && size != 0)
    {
        // TODO: Raise out of memory error
    }
    return ptr;
}

void* MallocAnsi::tryAllocate(USize size, UInt32 alignment) noexcept
{
    alignment = math::max<UInt32>(size >= 16 ? 16 : 8, alignment);
    [[assume(alignment >= 8 && (alignment & (alignment - 1)) == 0)]];
    void* ptr = ansi::allocate(size, alignment);
    return ptr;
}

void* MallocAnsi::reallocate(void* ptr, USize newSize, UInt32 alignment)
{
    void* newPtr = tryReallocate(ptr, newSize, alignment);
    if (newPtr == nullptr && newSize != 0)
    {
        // TODO: Raise out of memory error
    }
    return newPtr;
}

void* MallocAnsi::tryReallocate(void* ptr, USize newSize, UInt32 alignment) noexcept
{
    alignment = math::max<UInt32>(newSize >= 16 ? 16 : 8, alignment);
    [[assume(alignment >= 8 && (alignment & (alignment - 1)) == 0)]];
    void* newPtr = ansi::reallocate(ptr, newSize, alignment);
    return newPtr;
}

void MallocAnsi::deallocate(void* ptr)
{
    ansi::deallocate(ptr);
}

USize MallocAnsi::getAllocationSize(void* ptr)
{
    if (ptr == nullptr)
    {
        return 0;
    }

#if GP_PLATFORM_USE_ALIGNED_MALLOC
    return 0;
#else
    return ansi::getAllocationSize(ptr);
#endif
}

bool MallocAnsi::canGetAllocationSize()
{
#if GP_PLATFORM_USE_ALIGNED_MALLOC
    return false;
#else
    return true;
#endif
}

}   // namespace gp::memory
