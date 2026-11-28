// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/UniformBufferEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::resource::tests
{

static_assert(isUniformBufferStaticSlotValid(0u), "Slot 0 must be valid.");
static_assert(
    isUniformBufferStaticSlotValid(static_cast<UniformBufferStaticSlot>(kMaxUniformBufferStaticSlots - 1u)),
    "Maximum valid static slot index must be accepted."
);
static_assert(
    !isUniformBufferStaticSlotValid(kMaxUniformBufferStaticSlots),
    "Sentinel kMaxUniformBufferStaticSlots must be rejected."
);

TEST(UniformBufferEnumsTest, StaticSlotBoundsValidation)
{
    EXPECT_TRUE(isUniformBufferStaticSlotValid(0u));
    EXPECT_TRUE(isUniformBufferStaticSlotValid(1u));
    EXPECT_TRUE(isUniformBufferStaticSlotValid(128u));
    EXPECT_TRUE(
        isUniformBufferStaticSlotValid(static_cast<UniformBufferStaticSlot>(kMaxUniformBufferStaticSlots - 1u))
    );
    EXPECT_FALSE(isUniformBufferStaticSlotValid(kMaxUniformBufferStaticSlots));
}

TEST(UniformBufferEnumsTest, UniformBufferFlagsBitwiseOperations)
{
    const UniformBufferFlags combined = UniformBufferFlags::NoEmulatedUniformBuffer |
                                        UniformBufferFlags::NeedsReflectedMembers |
                                        UniformBufferFlags::BindlessAccessible;

    EXPECT_NE(combined & UniformBufferFlags::NoEmulatedUniformBuffer, UniformBufferFlags::None);
    EXPECT_NE(combined & UniformBufferFlags::NeedsReflectedMembers, UniformBufferFlags::None);
    EXPECT_NE(combined & UniformBufferFlags::BindlessAccessible, UniformBufferFlags::None);
    EXPECT_EQ(combined & UniformBufferFlags::HasNonGraphOutputs, UniformBufferFlags::None);
}

}   // namespace gp::rhi::resource::tests
