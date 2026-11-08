// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/ThreadScratchAllocator.hpp"

namespace gp::memory
{

namespace detail
{

[[nodiscard]] static LinearAllocator& getThreadLocalScratchInstance() noexcept
{
    thread_local LinearAllocator s_threadScratchArena{};
    return s_threadScratchArena;
}

[[nodiscard]] static gp::UInt64& getThreadLocalEpochInstance() noexcept
{
    thread_local gp::UInt64 s_threadFiberSwitchEpoch{ 0ull };
    return s_threadFiberSwitchEpoch;
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
    return detail::getThreadLocalEpochInstance();
}

gp::UInt64 ThreadScratchArena::advanceCurrentThreadEpoch() noexcept
{
    gp::UInt64& epoch = detail::getThreadLocalEpochInstance();
    ++epoch;
    return epoch;
}

}   // namespace gp::memory
