// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/ThreadScratchAllocator.hpp"

namespace gp::memory
{

namespace detail
{

static thread_local gp::UInt64 tl_threadFiberSwitchEpoch{ 0ull };

static GP_FORCEINLINE void compilerMemoryBarrier() noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#else
    _ReadWriteBarrier();
#endif
}

[[nodiscard]] static GP_FORCENOINLINE LinearAllocator& getThreadLocalScratchInstance() noexcept
{
    compilerMemoryBarrier();
    thread_local LinearAllocator s_threadScratchArena{};
    return s_threadScratchArena;
}

[[nodiscard]] static GP_FORCENOINLINE gp::UInt64 getThreadLocalEpochValue() noexcept
{
    compilerMemoryBarrier();
    return tl_threadFiberSwitchEpoch;
}

[[nodiscard]] static GP_FORCENOINLINE gp::UInt64 incrementThreadLocalEpochValue() noexcept
{
    compilerMemoryBarrier();
    const gp::UInt64 nextEpoch = ++tl_threadFiberSwitchEpoch;
    compilerMemoryBarrier();
    return nextEpoch;
}

}   // namespace detail

bool ThreadScratchArena::initializeThreadScratch(const gp::USize capacityInBytes, Malloc* backingMalloc) noexcept
{
    LinearAllocator& arena = detail::getThreadLocalScratchInstance();
    return arena.initialize(capacityInBytes, backingMalloc, "ThreadScratchArena");
}

void ThreadScratchArena::shutdownThreadScratch() noexcept
{
    LinearAllocator& arena = detail::getThreadLocalScratchInstance();
    arena.shutdown();
}

void ThreadScratchArena::resetThreadScratch() noexcept
{
    LinearAllocator& arena = detail::getThreadLocalScratchInstance();
    if (arena.isValid())
    {
        arena.reset();
    }
}

LinearAllocator& ThreadScratchArena::get() noexcept
{
    LinearAllocator& arena = detail::getThreadLocalScratchInstance();
    if (!arena.isValid())
    {
        arena.initialize(kDefaultScratchCapacityBytes, nullptr, "ThreadScratchArena");
    }

    return arena;
}

LinearAllocator* ThreadScratchArena::tryGet() noexcept
{
    LinearAllocator& arena = detail::getThreadLocalScratchInstance();
    return arena.isValid() ? &arena : nullptr;
}

LinearAllocator& ThreadScratchArena::getUnchecked() noexcept
{
    return detail::getThreadLocalScratchInstance();
}

gp::UInt64 ThreadScratchArena::getCurrentThreadEpoch() noexcept
{
    return detail::getThreadLocalEpochValue();
}

gp::UInt64 ThreadScratchArena::advanceCurrentThreadEpoch() noexcept
{
    return detail::incrementThreadLocalEpochValue();
}

}   // namespace gp::memory
