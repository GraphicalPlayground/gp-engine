// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/AllocatorTypes.hpp"
#include <gtest/gtest.h>

namespace gp::memory::tests
{

namespace
{

/// @brief Minimal constexpr mock markable arena used to verify `ScopedMemoryMark` in `constexpr` and runtime contexts.
class MockMarkableArena
{
public:
    gp::USize offset{ 0u };
    gp::UInt64 activeAllocations{ 0ull };
    gp::USize allocatedBytes{ 0u };
    gp::UInt32 rewindCount{ 0u };

    constexpr void bump(const gp::USize bytes) noexcept
    {
        offset += bytes;
        allocatedBytes += bytes;
        ++activeAllocations;
    }

    [[nodiscard]] constexpr MemoryMark mark() const noexcept
    {
        return MemoryMark{ .offset = offset, .activeAllocations = activeAllocations, .allocatedBytes = allocatedBytes };
    }

    constexpr void rewind(const MemoryMark& savedMark) noexcept
    {
        offset = savedMark.offset;
        activeAllocations = savedMark.activeAllocations;
        allocatedBytes = savedMark.allocatedBytes;
        ++rewindCount;
    }
};

constexpr bool testConstexprScopedMemoryMark() noexcept
{
    MockMarkableArena arena{};
    arena.bump(64u);

    {
        ScopedMemoryMark<MockMarkableArena> guard(arena);
        arena.bump(128u);
        if (arena.offset != 192u)
        {
            return false;
        }
    }

    return arena.offset == 64u && arena.activeAllocations == 1ull && arena.rewindCount == 1u;
}

static_assert(testConstexprScopedMemoryMark(), "Constexpr ScopedMemoryMark evaluation failed.");

}   // namespace

TEST(AllocatorTypesTest, MemoryMarkDefaultInitialization)
{
    const MemoryMark mark{};
    EXPECT_EQ(mark.offset, 0u);
    EXPECT_EQ(mark.activeAllocations, 0ull);
    EXPECT_EQ(mark.allocatedBytes, 0u);
}

TEST(AllocatorTypesTest, AllocatorStatsDefaultInitialization)
{
    const AllocatorStats stats{};
    EXPECT_EQ(stats.allocatedBytes, 0u);
    EXPECT_EQ(stats.peakAllocatedBytes, 0u);
    EXPECT_EQ(stats.capacityBytes, 0u);
    EXPECT_EQ(stats.activeAllocations, 0ull);
    EXPECT_EQ(stats.totalAllocations, 0ull);
    EXPECT_EQ(stats.totalDeallocations, 0ull);
}

TEST(AllocatorTypesTest, ScopedMemoryMarkRewindsOnScopeExitAndSupportsManualRewindNow)
{
    MockMarkableArena arena{};
    arena.bump(32u);

    {
        ScopedMemoryMark<MockMarkableArena> outerGuard(arena);
        EXPECT_EQ(outerGuard.getMark().offset, 32u);

        arena.bump(64u);
        EXPECT_EQ(arena.offset, 96u);
        EXPECT_EQ(arena.activeAllocations, 2ull);

        // Manual mid-scope rewind
        outerGuard.rewindNow();
        EXPECT_EQ(arena.offset, 32u);
        EXPECT_EQ(arena.activeAllocations, 1ull);
        EXPECT_EQ(arena.rewindCount, 1u);

        // Allocate again in the same scope; destructor must rewind a second time
        arena.bump(256u);
        EXPECT_EQ(arena.offset, 288u);
    }

    EXPECT_EQ(arena.offset, 32u);
    EXPECT_EQ(arena.activeAllocations, 1ull);
    EXPECT_EQ(arena.allocatedBytes, 32u);
    EXPECT_EQ(arena.rewindCount, 2u);
}

}   // namespace gp::memory::tests
