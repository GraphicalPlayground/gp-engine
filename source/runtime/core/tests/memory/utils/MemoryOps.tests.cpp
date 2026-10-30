// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/utils/MemoryOps.hpp"
#include <gtest/gtest.h>

namespace gp::memory::tests
{

namespace
{

/// @brief Non-trivial, non-bitwise-copyable type that tracks all construction and destruction events.
struct LifecycleTracker
{
    static inline gp::Int32 s_activeInstances{ 0 };
    static inline gp::Int32 s_defaultConstructions{ 0 };
    static inline gp::Int32 s_valueConstructions{ 0 };
    static inline gp::Int32 s_copyConstructions{ 0 };
    static inline gp::Int32 s_moveConstructions{ 0 };
    static inline gp::Int32 s_destructions{ 0 };

    static void reset() noexcept
    {
        s_activeInstances = 0;
        s_defaultConstructions = 0;
        s_valueConstructions = 0;
        s_copyConstructions = 0;
        s_moveConstructions = 0;
        s_destructions = 0;
    }

    gp::Int32 id{ 0 };

    LifecycleTracker() noexcept
        : id(42)
    {
        ++s_activeInstances;
        ++s_defaultConstructions;
    }

    explicit LifecycleTracker(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_activeInstances;
        ++s_valueConstructions;
    }

    LifecycleTracker(const LifecycleTracker& other) noexcept
        : id(other.id)
    {
        ++s_activeInstances;
        ++s_copyConstructions;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : id(other.id)
    {
        other.id = -1;
        ++s_activeInstances;
        ++s_moveConstructions;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other) noexcept
    {
        id = other.id;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        id = other.id;
        other.id = -1;
        return *this;
    }

    ~LifecycleTracker() noexcept
    {
        --s_activeInstances;
        ++s_destructions;
    }
};

/// @brief Implicitly convertible source wrapper to test cross-type `constructItems<Dest, Src>`.
struct ConvertibleSource
{
    gp::Int32 payload{ 0 };

    explicit operator LifecycleTracker() const noexcept
    {
        return LifecycleTracker(payload * 10);
    }
};

}   // namespace

class MemoryOpsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LifecycleTracker::reset();
    }

    void TearDown() override
    {
        EXPECT_EQ(LifecycleTracker::s_activeInstances, 0) << "LifecycleTracker instance leak detected!";
    }
};

TEST_F(MemoryOpsTest, TrivialDefaultAndValueConstructItems)
{
    gp::UInt32 buffer[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };

    // defaultConstructItems on trivial types is a deliberate no-op (leaves existing bytes untouched)
    defaultConstructItems<gp::UInt32>(buffer, 8u);
    EXPECT_EQ(buffer[0], 1u);
    EXPECT_EQ(buffer[7], 8u);

    // valueConstructItems on trivial types bulk-zeros the buffer via zeroMemory
    valueConstructItems<gp::UInt32>(buffer, 8u);
    for (gp::USize i = 0u; i < 8u; ++i)
    {
        EXPECT_EQ(buffer[i], 0u);
    }
}

TEST_F(MemoryOpsTest, NonTrivialDefaultAndValueConstructAndDestructItems)
{
    constexpr gp::USize kCount = 6u;
    alignas(LifecycleTracker) gp::UInt8 rawStorage[sizeof(LifecycleTracker) * kCount];
    LifecycleTracker* items = reinterpret_cast<LifecycleTracker*>(rawStorage);

    defaultConstructItems<LifecycleTracker>(items, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, static_cast<gp::Int32>(kCount));
    EXPECT_EQ(LifecycleTracker::s_defaultConstructions, static_cast<gp::Int32>(kCount));
    for (gp::USize i = 0u; i < kCount; ++i)
    {
        EXPECT_EQ(items[i].id, 42);
    }

    // Destruct single item at index 0, then remaining items
    destructItem(&items[0]);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, static_cast<gp::Int32>(kCount - 1u));

    destructItems(items + 1u, kCount - 1u);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 0);
    EXPECT_EQ(LifecycleTracker::s_destructions, static_cast<gp::Int32>(kCount));

    // Now test valueConstructItems on non-trivial type
    LifecycleTracker::reset();
    valueConstructItems<LifecycleTracker>(items, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, static_cast<gp::Int32>(kCount));
    EXPECT_EQ(LifecycleTracker::s_defaultConstructions, static_cast<gp::Int32>(kCount));

    destructItems(items, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 0);
}

TEST_F(MemoryOpsTest, TrivialConstructMoveAndRelocateItems)
{
    constexpr gp::USize kCount = 5u;
    const gp::UInt64 source[kCount] = { 100ull, 200ull, 300ull, 400ull, 500ull };
    gp::UInt64 copied[kCount] = {};
    gp::UInt64 moved[kCount] = {};
    gp::UInt64 relocated[kCount] = {};

    constructItems<gp::UInt64>(copied, source, kCount);
    for (gp::USize i = 0u; i < kCount; ++i)
    {
        EXPECT_EQ(copied[i], source[i]);
    }

    moveConstructItems<gp::UInt64>(moved, copied, kCount);
    for (gp::USize i = 0u; i < kCount; ++i)
    {
        EXPECT_EQ(moved[i], source[i]);
    }

    relocateConstructItems<gp::UInt64>(relocated, moved, kCount);
    for (gp::USize i = 0u; i < kCount; ++i)
    {
        EXPECT_EQ(relocated[i], source[i]);
    }

    // Trivial destructors are no-ops
    destructItem(&relocated[0]);
    destructItems(relocated, kCount);
}

TEST_F(MemoryOpsTest, NonTrivialCopyMoveAndRelocateItems)
{
    constexpr gp::USize kCount = 4u;
    alignas(LifecycleTracker) gp::UInt8 rawSrc[sizeof(LifecycleTracker) * kCount];
    alignas(LifecycleTracker) gp::UInt8 rawCopy[sizeof(LifecycleTracker) * kCount];
    alignas(LifecycleTracker) gp::UInt8 rawMoved[sizeof(LifecycleTracker) * kCount];
    alignas(LifecycleTracker) gp::UInt8 rawRelocated[sizeof(LifecycleTracker) * kCount];

    LifecycleTracker* src = reinterpret_cast<LifecycleTracker*>(rawSrc);
    LifecycleTracker* copyDest = reinterpret_cast<LifecycleTracker*>(rawCopy);
    LifecycleTracker* moveDest = reinterpret_cast<LifecycleTracker*>(rawMoved);
    LifecycleTracker* relocateDest = reinterpret_cast<LifecycleTracker*>(rawRelocated);

    for (gp::USize i = 0u; i < kCount; ++i)
    {
        ::new (static_cast<void*>(src + i)) LifecycleTracker(static_cast<gp::Int32>(i + 1u));
    }
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 4);

    // 1. Copy-construct from src into copyDest
    constructItems<LifecycleTracker>(copyDest, src, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 8);
    EXPECT_EQ(LifecycleTracker::s_copyConstructions, 4);

    // 2. Move-construct from copyDest into moveDest (copyDest remains alive in moved-from state id == -1)
    moveConstructItems<LifecycleTracker>(moveDest, copyDest, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 12);
    EXPECT_EQ(LifecycleTracker::s_moveConstructions, 4);
    for (gp::USize i = 0u; i < kCount; ++i)
    {
        EXPECT_EQ(copyDest[i].id, -1);
        EXPECT_EQ(moveDest[i].id, static_cast<gp::Int32>(i + 1u));
    }

    destructItems(src, kCount);
    destructItems(copyDest, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 4);

    // 3. Relocate from moveDest into relocateDest (move-constructs into relocateDest AND destroys moveDest)
    relocateConstructItems<LifecycleTracker>(relocateDest, moveDest, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 4);
    for (gp::USize i = 0u; i < kCount; ++i)
    {
        EXPECT_EQ(relocateDest[i].id, static_cast<gp::Int32>(i + 1u));
    }

    destructItems(relocateDest, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 0);
}

TEST_F(MemoryOpsTest, HeterogeneousConstructItemsFromConvertibleSource)
{
    constexpr gp::USize kCount = 3u;
    const ConvertibleSource sources[kCount] = { { 1 }, { 2 }, { 3 } };
    alignas(LifecycleTracker) gp::UInt8 rawDest[sizeof(LifecycleTracker) * kCount];
    LifecycleTracker* dest = reinterpret_cast<LifecycleTracker*>(rawDest);

    constructItems<LifecycleTracker>(dest, sources, kCount);
    EXPECT_EQ(LifecycleTracker::s_activeInstances, 3);
    EXPECT_EQ(dest[0].id, 10);
    EXPECT_EQ(dest[1].id, 20);
    EXPECT_EQ(dest[2].id, 30);

    destructItems(dest, kCount);
}

TEST_F(MemoryOpsTest, NullPointerAndZeroCountEdgeCasesAreSafeNoOps)
{
    LifecycleTracker* nullTracker = nullptr;
    valueConstructItems<LifecycleTracker>(nullptr, 5u);
    destructItem<LifecycleTracker>(nullTracker);
    destructItems<LifecycleTracker>(nullTracker, 5u);
    constructItems<LifecycleTracker>(nullptr, nullTracker, 5u);
    moveConstructItems<LifecycleTracker>(nullptr, nullTracker, 5u);
    relocateConstructItems<LifecycleTracker>(nullptr, nullTracker, 5u);

    alignas(LifecycleTracker) gp::UInt8 dummy[sizeof(LifecycleTracker)];
    LifecycleTracker* validPtr = reinterpret_cast<LifecycleTracker*>(dummy);
    defaultConstructItems<LifecycleTracker>(validPtr, 0);
    valueConstructItems<LifecycleTracker>(validPtr, 0);
    destructItems<LifecycleTracker>(validPtr, 0);
    constructItems<LifecycleTracker>(validPtr, validPtr, 0);
    moveConstructItems<LifecycleTracker>(validPtr, validPtr, 0);
    relocateConstructItems<LifecycleTracker>(validPtr, validPtr, 0);

    EXPECT_EQ(LifecycleTracker::s_activeInstances, 0);
}

}   // namespace gp::memory::tests
