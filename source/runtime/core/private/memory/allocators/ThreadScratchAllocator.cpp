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

}   // namespace gp::memory
