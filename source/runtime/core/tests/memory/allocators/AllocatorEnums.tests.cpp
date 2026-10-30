// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/AllocatorEnums.hpp"
#include <gtest/gtest.h>

namespace gp::memory::tests
{

constexpr bool testConstexprAllocatorFlagsBitwiseOps() noexcept
{
    const AllocatorFlags combined = AllocatorFlags::ZeroMemory | AllocatorFlags::TrackTelemetry;
    if (!gp::enums::hasAllFlags(combined, AllocatorFlags::ZeroMemory))
    {
        return false;
    }
    if (!gp::enums::hasAllFlags(combined, AllocatorFlags::TrackTelemetry))
    {
        return false;
    }
    if (gp::enums::hasAnyFlags(combined & AllocatorFlags::ThreadSafe))
    {
        return false;
    }

    return true;
}

static_assert(testConstexprAllocatorFlagsBitwiseOps(), "Constexpr AllocatorFlags bitwise operations failed.");
static_assert(gp::concepts::IsBitwiseEnum<AllocatorFlags>, "AllocatorFlags must satisfy IsBitwiseEnum.");

TEST(AllocatorEnumsTest, AllocatorFlagsBitmaskValuesAndOperations)
{
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorFlags::None), 0x00u);
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorFlags::ZeroMemory), 0x01u);
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorFlags::ThreadSafe), 0x02u);
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorFlags::AllowFallback), 0x04u);
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorFlags::TrackTelemetry), 0x08u);

    AllocatorFlags flags = AllocatorFlags::None;
    EXPECT_FALSE(gp::enums::hasAnyFlags(flags));

    flags |= AllocatorFlags::ZeroMemory;
    flags |= AllocatorFlags::AllowFallback;
    EXPECT_TRUE(gp::enums::hasAnyFlags(flags));
    EXPECT_TRUE(gp::enums::hasAllFlags(flags, AllocatorFlags::ZeroMemory));
    EXPECT_TRUE(gp::enums::hasAllFlags(flags, AllocatorFlags::AllowFallback));
    EXPECT_TRUE(gp::enums::hasNoFlags(flags, AllocatorFlags::ThreadSafe));

    flags = gp::enums::clearFlags(flags, AllocatorFlags::ZeroMemory);
    EXPECT_FALSE(gp::enums::hasAllFlags(flags, AllocatorFlags::ZeroMemory));
    EXPECT_TRUE(gp::enums::hasAllFlags(flags, AllocatorFlags::AllowFallback));
}

TEST(AllocatorEnumsTest, AllocatorGrowthPolicyValues)
{
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorGrowthPolicy::Geometric), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorGrowthPolicy::Exact), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(AllocatorGrowthPolicy::Fixed), 2u);
}

}   // namespace gp::memory::tests
