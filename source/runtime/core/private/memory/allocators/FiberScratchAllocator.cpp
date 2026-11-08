// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::memory
{

namespace detail
{

static constexpr gp::UInt32 kPoolStateUninitialized = 0u;
static constexpr gp::UInt32 kPoolStateInitializing = 1u;
static constexpr gp::UInt32 kPoolStateInitialized = 2u;

static gp::thread::Atomic<gp::UInt32> s_sharedPoolState{ kPoolStateUninitialized };

[[nodiscard]] static PagePool& getSharedPagePoolInstance() noexcept
{
    static PagePool s_sharedPagePool{};
    return s_sharedPagePool;
}

[[nodiscard]] static PagePool* resolveSharedPagePool() noexcept
{
    PagePool& pool = getSharedPagePoolInstance();
    gp::UInt32 state = s_sharedPoolState.load(MemoryOrder::Acquire);
    if (state == kPoolStateInitialized && pool.isValid()) [[likely]]
    {
        return &pool;
    }

    gp::UInt32 expected = kPoolStateUninitialized;
    if (s_sharedPoolState.compareExchangeStrong(
            expected, kPoolStateInitializing, MemoryOrder::Acquire, MemoryOrder::Relaxed
        ))
    {
        const bool initialized = pool.initialize(
            FiberScratchArena::kDefaultPageSizeBytes,
            FiberScratchArena::kDefaultPoolPageCount,
            nullptr,
            "FiberScratchArena.PagePool"
        );
        s_sharedPoolState.store(initialized ? kPoolStateInitialized : kPoolStateUninitialized, MemoryOrder::Release);
        return initialized ? &pool : nullptr;
    }

    while (s_sharedPoolState.load(MemoryOrder::Acquire) == kPoolStateInitializing)
    {
        gp::platform::Thread::spinWait();
    }

    return pool.isValid() ? &pool : nullptr;
}

[[nodiscard]] static PagedLinearAllocator*& getThreadLocalBoundFiberArena() noexcept
{
    thread_local PagedLinearAllocator* s_boundFiberArena{ nullptr };
    return s_boundFiberArena;
}

[[nodiscard]] static PagedLinearAllocator& getThreadLocalFallbackFiberArena() noexcept
{
    thread_local PagedLinearAllocator s_fallbackFiberArena{
        &resolveSharedPagePool, nullptr, true, "FiberScratchArena.Fallback"
    };
    if (!s_fallbackFiberArena.isValid()) [[unlikely]]
    {
        (void)s_fallbackFiberArena.initialize(&resolveSharedPagePool, nullptr, true, "FiberScratchArena.Fallback");
    }

    return s_fallbackFiberArena;
}

}   // namespace detail

bool FiberScratchArena::initializePool(
    const gp::USize pageSizeInBytes, const gp::UInt32 pageCount, Malloc* backingMalloc
) noexcept
{
    shutdownThreadFallbackScratch();

    PagePool& pool = detail::getSharedPagePoolInstance();
    detail::s_sharedPoolState.store(detail::kPoolStateInitializing, MemoryOrder::Release);

    const bool initialized = pool.initialize(pageSizeInBytes, pageCount, backingMalloc, "FiberScratchArena.PagePool");
    detail::s_sharedPoolState.store(
        initialized ? detail::kPoolStateInitialized : detail::kPoolStateUninitialized, MemoryOrder::Release
    );

    return initialized;
}

void FiberScratchArena::shutdownPool() noexcept
{
    shutdownThreadFallbackScratch();

    PagePool& pool = detail::getSharedPagePoolInstance();
    pool.shutdown();
    detail::s_sharedPoolState.store(detail::kPoolStateUninitialized, MemoryOrder::Release);
}

void FiberScratchArena::shutdownThreadFallbackScratch() noexcept
{
    PagedLinearAllocator& fallbackArena = detail::getThreadLocalFallbackFiberArena();
    fallbackArena.reset();
}

PagePool& FiberScratchArena::getSharedPagePool() noexcept
{
    (void)detail::resolveSharedPagePool();
    return detail::getSharedPagePoolInstance();
}

PagePool* FiberScratchArena::tryGetSharedPagePool() noexcept
{
    if (detail::s_sharedPoolState.load(MemoryOrder::Acquire) != detail::kPoolStateInitialized)
    {
        return nullptr;
    }

    PagePool& pool = detail::getSharedPagePoolInstance();
    return pool.isValid() ? &pool : nullptr;
}

PagedLinearAllocator* FiberScratchArena::bindFiberArena(PagedLinearAllocator* fiberArena) noexcept
{
    if (fiberArena != nullptr && !fiberArena->isValid())
    {
        (void)fiberArena->initialize(&detail::resolveSharedPagePool, nullptr, true, "FiberScratchArena.Fiber");
    }

    PagedLinearAllocator*& boundSlot = detail::getThreadLocalBoundFiberArena();
    PagedLinearAllocator* const previousArena = boundSlot;
    boundSlot = fiberArena;

    (void)ThreadScratchArena::advanceCurrentThreadEpoch();

    return previousArena;
}

PagedLinearAllocator* FiberScratchArena::getBoundFiberArena() noexcept
{
    return detail::getThreadLocalBoundFiberArena();
}

gp::UInt64 FiberScratchArena::notifyFiberYield() noexcept
{
    return ThreadScratchArena::advanceCurrentThreadEpoch();
}

gp::UInt64 FiberScratchArena::getThreadSwitchEpoch() noexcept
{
    return ThreadScratchArena::getCurrentThreadEpoch();
}

PagedLinearAllocator& FiberScratchArena::get() noexcept
{
    if (PagedLinearAllocator* const boundArena = detail::getThreadLocalBoundFiberArena(); boundArena != nullptr)
        [[likely]]
    {
        return *boundArena;
    }

    return detail::getThreadLocalFallbackFiberArena();
}

PagedLinearAllocator* FiberScratchArena::tryGet() noexcept
{
    if (PagedLinearAllocator* const boundArena = detail::getThreadLocalBoundFiberArena(); boundArena != nullptr)
    {
        return boundArena;
    }

    if (tryGetSharedPagePool() == nullptr)
    {
        return nullptr;
    }

    PagedLinearAllocator& fallbackArena = detail::getThreadLocalFallbackFiberArena();
    return (fallbackArena.getTotalPageCount() > 0u) ? &fallbackArena : nullptr;
}

void FiberScratchArena::resetCurrentScratch() noexcept
{
    if (PagedLinearAllocator* const arena = tryGet(); arena != nullptr)
    {
        arena->reset();
    }
}

}   // namespace gp::memory
