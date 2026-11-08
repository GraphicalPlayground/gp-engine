// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "memory/utils/MemoryOps.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <new>   // IWYU pragma: keep
#include <thread>
#include <vector>

namespace gp::memory::tests
{

namespace
{

struct MockContainerWithFiberScratch
{
    void* data{ nullptr };
    gp::USize size{ 0u };
    gp::USize capacity{ 0u };
    GP_NO_UNIQUE_ADDRESS FiberScratchAllocator allocator{};
};

static_assert(
    sizeof(MockContainerWithFiberScratch) == (sizeof(void*) + (2u * sizeof(gp::USize))),
    "Stateless FiberScratchAllocator must occupy 0 bytes when embedded via GP_NO_UNIQUE_ADDRESS."
);

struct FiberLifecycleItem
{
    static inline gp::thread::Atomic<gp::Int32> s_constructs{ 0 };
    static inline gp::thread::Atomic<gp::Int32> s_destructs{ 0 };

    static void reset() noexcept
    {
        s_constructs.store(0, gp::MemoryOrder::Release);
        s_destructs.store(0, gp::MemoryOrder::Release);
    }

    gp::UInt32 fiberId{ 0u };
    gp::UInt32 sequence{ 0u };

    FiberLifecycleItem(const gp::UInt32 inFiberId, const gp::UInt32 inSequence) noexcept
        : fiberId(inFiberId)
        , sequence(inSequence)
    {
        s_constructs.fetchAdd(1, gp::MemoryOrder::Relaxed);
    }

    ~FiberLifecycleItem() noexcept
    {
        s_destructs.fetchAdd(1, gp::MemoryOrder::Relaxed);
    }
};

}   // namespace

TEST(FiberScratchAllocatorTest, ColdQueriesAndEmptyScopedFiberScratchMarkDoNotTriggerLazyPoolAllocation)
{
    FiberScratchArena::shutdownPool();
    EXPECT_EQ(FiberScratchArena::tryGetSharedPagePool(), nullptr);
    EXPECT_EQ(FiberScratchArena::tryGet(), nullptr);

    FiberScratchAllocator scratch{};

    // Cold queries and empty scopes must never allocate the 8 MiB shared PagePool slab
    EXPECT_FALSE(scratch.owns(reinterpret_cast<const void*>(0x1000)));
    scratch.deallocate(nullptr, 0u);
    scratch.deallocate(reinterpret_cast<void*>(0x1000), 64u);
    EXPECT_EQ(scratch.getActualAllocationSize(128u, 16u), 128u);
    scratch.reset();
    {
        ScopedFiberScratchMark coldScope{};
        EXPECT_TRUE(coldScope.isArenaBoundToCurrentContext());
        EXPECT_EQ(coldScope.getMark().pageCookie, nullptr);
    }
    EXPECT_EQ(FiberScratchArena::tryGetSharedPagePool(), nullptr);

    // First non-zero allocation lazily initializes the shared PagePool and leases 1 page
    {
        ScopedFiberScratchMark activeScope{};
        void* const block = scratch.allocate(256u, 32u);
        ASSERT_NE(block, nullptr);
        EXPECT_TRUE(gp::memory::isAligned(block, 32u));
        EXPECT_TRUE(scratch.owns(block));

        ASSERT_NE(FiberScratchArena::tryGetSharedPagePool(), nullptr);
        EXPECT_EQ(FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 1u);
        EXPECT_EQ(FiberScratchArena::get().getUsedBytes(), 256u);
    }

    // Scope exit returns the leased page back to the shared PagePool (0 leased pages remaining)
    ASSERT_NE(FiberScratchArena::tryGetSharedPagePool(), nullptr);
    EXPECT_EQ(FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);
    EXPECT_EQ(FiberScratchArena::get().getLeasedPageCount(), 0u);

    FiberScratchArena::shutdownPool();
    EXPECT_EQ(FiberScratchArena::tryGetSharedPagePool(), nullptr);
}

TEST(FiberScratchAllocatorTest, SameThreadCooperativeFiberYieldOutOfOrderUnwindPreservesSiblingFiberMemory)
{
    ASSERT_TRUE(FiberScratchArena::initializePool(4096u, 16u));

    PagedLinearAllocator fiber1Arena{};
    PagedLinearAllocator fiber2Arena{};
    FiberScratchAllocator scratch{};

    // 1. Fiber 1 begins executing on Worker Thread 0
    (void)FiberScratchArena::bindFiberArena(&fiber1Arena);
    EXPECT_EQ(FiberScratchArena::getBoundFiberArena(), &fiber1Arena);

    ScopedFiberScratchMark fiber1Scope{};
    gp::UInt8* const fiber1Data = static_cast<gp::UInt8*>(scratch.allocate(1024u, 32u));
    ASSERT_NE(fiber1Data, nullptr);
    for (gp::USize i = 0u; i < 1024u; ++i)
    {
        fiber1Data[i] = 0xA1u;
    }

    // 2. Fiber 1 yields at `waitForCounter()`; Worker Thread 0 switches to Fiber 2
    (void)FiberScratchArena::bindFiberArena(&fiber2Arena);
    EXPECT_EQ(FiberScratchArena::getBoundFiberArena(), &fiber2Arena);
    EXPECT_FALSE(fiber1Scope.isArenaBoundToCurrentContext());

    MemoryMark fiber2Mark = scratch.mark();
    gp::UInt8* const fiber2Data = static_cast<gp::UInt8*>(scratch.allocate(1024u, 32u));
    ASSERT_NE(fiber2Data, nullptr);
    for (gp::USize i = 0u; i < 1024u; ++i)
    {
        fiber2Data[i] = 0xB2u;
    }

    // Both fibers hold independent leased pages from the shared PagePool
    EXPECT_EQ(FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 2u);

    // 3. Fiber 2 yields; Worker Thread 0 switches back to Fiber 1 first (out-of-order completion!)
    (void)FiberScratchArena::bindFiberArena(&fiber1Arena);
    EXPECT_TRUE(fiber1Scope.isArenaBoundToCurrentContext());

    // Fiber 1 grows its allocation in-place and rewinds its scope BEFORE Fiber 2 finishes
    gp::UInt8* const fiber1Grown = static_cast<gp::UInt8*>(scratch.reallocate(fiber1Data, 1024u, 2048u, 32u));
    EXPECT_EQ(fiber1Grown, fiber1Data);
    for (gp::USize i = 0u; i < 1024u; ++i)
    {
        EXPECT_EQ(fiber1Grown[i], 0xA1u);
    }

    fiber1Scope.rewindNow();
    EXPECT_EQ(fiber1Arena.getLeasedPageCount(), 0u);
    EXPECT_EQ(FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 1u);

    // 4. Worker Thread 0 switches back to Fiber 2; Fiber 2's allocation must be 100% intact and uncorrupted!
    (void)FiberScratchArena::bindFiberArena(&fiber2Arena);
    EXPECT_TRUE(scratch.owns(fiber2Data));
    for (gp::USize i = 0u; i < 1024u; ++i)
    {
        EXPECT_EQ(fiber2Data[i], 0xB2u);
    }

    scratch.rewind(fiber2Mark);
    EXPECT_EQ(fiber2Arena.getLeasedPageCount(), 0u);
    EXPECT_EQ(FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);

    (void)FiberScratchArena::bindFiberArena(nullptr);
    FiberScratchArena::shutdownPool();
}

TEST(FiberScratchAllocatorTest, CrossThreadWorkStealingFiberMigrationStressTest)
{
    constexpr gp::UInt32 kWorkerThreadCount = 8u;
    constexpr gp::UInt32 kFiberCount = 32u;
    constexpr gp::USize kPageSize = 4096u;
    constexpr gp::UInt32 kPoolPages = 128u;
    constexpr gp::USize kItemsPerFiber = 32u;

    FiberLifecycleItem::reset();
    ASSERT_TRUE(FiberScratchArena::initializePool(kPageSize, kPoolPages));

    struct SimulatedFiberContext
    {
        PagedLinearAllocator scratchArena{};
        MemoryMark scopeMark{};
        gp::UInt8* rawPayloadPage1{ nullptr };
        gp::UInt8* rawPayloadPage2{ nullptr };
        FiberLifecycleItem* items{ nullptr };
        gp::UInt32 initialWorkerId{ 0xFFFFFFFFu };
        gp::UInt32 resumingWorkerId{ 0xFFFFFFFFu };
    };

    SimulatedFiberContext fibers[kFiberCount]{};

    // Phase 1: Worker Thread `t` starts executing a subset of fibers, allocates multi-page scratch memory + items,
    //          and yields the fiber without unwinding its scratch scope.
    {
        std::vector<std::thread> phase1Workers;
        phase1Workers.reserve(kWorkerThreadCount);

        for (gp::UInt32 workerId = 0u; workerId < kWorkerThreadCount; ++workerId)
        {
            phase1Workers.emplace_back([workerId, &fibers]()
            {
                FiberScratchAllocator scratch{};
                for (gp::UInt32 fiberId = workerId; fiberId < kFiberCount; fiberId += kWorkerThreadCount)
                {
                    SimulatedFiberContext& fiber = fibers[fiberId];
                    ScopedFiberArenaBind bindGuard(fiber.scratchArena);

                    fiber.initialWorkerId = workerId;
                    fiber.scopeMark = scratch.mark();

                    // Allocate two 2500-byte payloads (forces 2 chained 4 KiB pages from PagePool)
                    fiber.rawPayloadPage1 = static_cast<gp::UInt8*>(scratch.allocate(2500u, 64u));
                    ASSERT_NE(fiber.rawPayloadPage1, nullptr);
                    EXPECT_TRUE(gp::memory::isAligned(fiber.rawPayloadPage1, 64u));

                    fiber.rawPayloadPage2 = static_cast<gp::UInt8*>(scratch.allocate(2500u, 64u));
                    ASSERT_NE(fiber.rawPayloadPage2, nullptr);
                    EXPECT_TRUE(gp::memory::isAligned(fiber.rawPayloadPage2, 64u));
                    EXPECT_EQ(fiber.scratchArena.getLeasedPageCount(), 2u);

                    const gp::UInt8 pattern = static_cast<gp::UInt8>((fiberId * 19u) + 7u);
                    for (gp::USize b = 0u; b < 2500u; ++b)
                    {
                        fiber.rawPayloadPage1[b] = pattern;
                        fiber.rawPayloadPage2[b] = static_cast<gp::UInt8>(pattern ^ 0x5Au);
                    }

                    // Allocate non-trivial lifecycle items on the same fiber scratch arena
                    fiber.items = static_cast<FiberLifecycleItem*>(
                        scratch.allocate(sizeof(FiberLifecycleItem) * kItemsPerFiber, alignof(FiberLifecycleItem))
                    );
                    ASSERT_NE(fiber.items, nullptr);
                    for (gp::UInt32 i = 0u; i < static_cast<gp::UInt32>(kItemsPerFiber); ++i)
                    {
                        ::new (static_cast<void*>(&fiber.items[i])) FiberLifecycleItem(fiberId, i);
                    }
                }
            });
        }

        for (std::thread& worker: phase1Workers)
        {
            worker.join();
        }
    }

    // All 32 suspended fibers currently hold 2 leased pages each across the shared PagePool
    EXPECT_GE(FiberScratchArena::getSharedPagePool().getLeasedPageCount(), kFiberCount * 2u);
    EXPECT_EQ(
        FiberLifecycleItem::s_constructs.load(gp::MemoryOrder::Acquire),
        static_cast<gp::Int32>(kFiberCount * kItemsPerFiber)
    );

    // Phase 2: Work-Stealing Migration! Every suspended fiber is stolen and resumed by a DIFFERENT OS Worker Thread
    //          (`(workerId + 3) % kWorkerThreadCount`), which verifies its memory, grows an allocation, destroys its
    //          items, and rewinds the fiber's `MemoryMark` on the new CPU thread!
    {
        std::vector<std::thread> phase2Workers;
        phase2Workers.reserve(kWorkerThreadCount);

        for (gp::UInt32 workerId = 0u; workerId < kWorkerThreadCount; ++workerId)
        {
            phase2Workers.emplace_back([workerId, &fibers]()
            {
                FiberScratchAllocator scratch{};
                const gp::UInt32 stolenGroup = (workerId + 3u) % kWorkerThreadCount;

                for (gp::UInt32 fiberId = stolenGroup; fiberId < kFiberCount; fiberId += kWorkerThreadCount)
                {
                    SimulatedFiberContext& fiber = fibers[fiberId];
                    ScopedFiberArenaBind bindGuard(fiber.scratchArena);

                    fiber.resumingWorkerId = workerId;
                    EXPECT_NE(fiber.resumingWorkerId, fiber.initialWorkerId);

                    // Verify ownership and payload integrity on the new OS thread
                    EXPECT_TRUE(scratch.owns(fiber.rawPayloadPage1));
                    EXPECT_TRUE(scratch.owns(fiber.rawPayloadPage2));
                    EXPECT_TRUE(scratch.owns(fiber.items));

                    const gp::UInt8 expectedPattern = static_cast<gp::UInt8>((fiberId * 19u) + 7u);
                    for (gp::USize b = 0u; b < 2500u; ++b)
                    {
                        EXPECT_EQ(fiber.rawPayloadPage1[b], expectedPattern);
                        EXPECT_EQ(fiber.rawPayloadPage2[b], static_cast<gp::UInt8>(expectedPattern ^ 0x5Au));
                    }

                    for (gp::UInt32 i = 0u; i < static_cast<gp::UInt32>(kItemsPerFiber); ++i)
                    {
                        EXPECT_EQ(fiber.items[i].fiberId, fiberId);
                        EXPECT_EQ(fiber.items[i].sequence, i);
                    }

                    // Allocate additional scratch space after migration
                    void* const postMigrationScratch = scratch.allocate(1024u, 32u);
                    ASSERT_NE(postMigrationScratch, nullptr);
                    EXPECT_TRUE(scratch.owns(postMigrationScratch));

                    gp::memory::destructItems(fiber.items, kItemsPerFiber);

                    // Unwind the fiber's scratch arena on the migrated worker thread
                    scratch.rewind(fiber.scopeMark);
                    EXPECT_EQ(fiber.scratchArena.getLeasedPageCount(), 0u);
                }
            });
        }

        for (std::thread& worker: phase2Workers)
        {
            worker.join();
        }
    }

    EXPECT_EQ(
        FiberLifecycleItem::s_destructs.load(gp::MemoryOrder::Acquire),
        FiberLifecycleItem::s_constructs.load(gp::MemoryOrder::Acquire)
    );
    EXPECT_EQ(FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);

    FiberScratchArena::shutdownPool();
}

TEST(FiberScratchAllocatorTest, ThreadScratchEpochCounterAdvancesOnFiberBindAndYield)
{
    const gp::UInt64 initialEpoch = FiberScratchArena::getThreadSwitchEpoch();
    EXPECT_EQ(initialEpoch, ThreadScratchArena::getCurrentThreadEpoch());

    {
        ScopedScratchMark leafThreadScope{};
        EXPECT_TRUE(leafThreadScope.isContextValid());
    }

    PagedLinearAllocator fiberArena{};
    {
        ScopedFiberArenaBind bind(fiberArena);
        EXPECT_EQ(FiberScratchArena::getThreadSwitchEpoch(), initialEpoch + 1ull);

        (void)FiberScratchArena::notifyFiberYield();
        EXPECT_EQ(FiberScratchArena::getThreadSwitchEpoch(), initialEpoch + 2ull);
    }

    // Restoring the previous binding on `~ScopedFiberArenaBind` also increments the epoch
    EXPECT_EQ(FiberScratchArena::getThreadSwitchEpoch(), initialEpoch + 3ull);
}

}   // namespace gp::memory::tests
