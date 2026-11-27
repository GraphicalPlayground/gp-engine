// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanPackedUniformBufferState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/Memory.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanPackedUniformBufferStateTest, DefaultStateHasNoData)
{
    const VulkanPackedUniformBufferState state{};

    EXPECT_FALSE(state.hasData());
    EXPECT_FALSE(state.isDirty());
    EXPECT_EQ(state.getData(), nullptr);
    EXPECT_EQ(state.getSize(), 0u);
}

TEST(VulkanPackedUniformBufferStateTest, InitializeZeroesAndMarksForUpload)
{
    VulkanPackedUniformBufferState state{};

    state.initialize(16u);

    ASSERT_TRUE(state.hasData());
    EXPECT_TRUE(state.isDirty());
    EXPECT_EQ(state.getSize(), 16u);
    for (gp::UInt32 index = 0u; index < 16u; ++index)
    {
        EXPECT_EQ(state.getData()[index], 0u);
    }
}

TEST(VulkanPackedUniformBufferStateTest, ZeroSizeDisablesTheState)
{
    VulkanPackedUniformBufferState state{};
    state.initialize(16u);

    state.initialize(0u);

    EXPECT_FALSE(state.hasData());
    EXPECT_FALSE(state.isDirty());
    state.reset();
    EXPECT_FALSE(state.isDirty());
}

TEST(VulkanPackedUniformBufferStateTest, ChangedBytesAreCopiedAndMarkDirty)
{
    VulkanPackedUniformBufferState state{};
    state.initialize(16u);
    state.clearDirty();
    const gp::Float32 value = 2.5f;

    state.setParameter(4u, sizeof(value), &value);

    EXPECT_TRUE(state.isDirty());
    gp::Float32 stored = 0.0f;
    gp::memory::copyMemory(&stored, state.getData() + 4u, sizeof(stored));
    EXPECT_FLOAT_EQ(stored, value);
    EXPECT_EQ(state.getData()[0], 0u);
}

TEST(VulkanPackedUniformBufferStateTest, IdenticalBytesKeepTheStateClean)
{
    VulkanPackedUniformBufferState state{};
    state.initialize(8u);
    const gp::UInt32 value = 0x01020304u;
    state.setParameter(0u, sizeof(value), &value);
    state.clearDirty();

    state.setParameter(0u, sizeof(value), &value);

    EXPECT_FALSE(state.isDirty());
}

TEST(VulkanPackedUniformBufferStateTest, EmptyWritesAreIgnored)
{
    VulkanPackedUniformBufferState state{};
    state.initialize(8u);
    state.clearDirty();
    const gp::UInt32 value = 7u;

    state.setParameter(0u, 0u, &value);
    state.setParameter(0u, sizeof(value), nullptr);

    EXPECT_FALSE(state.isDirty());
}

TEST(VulkanPackedUniformBufferStateTest, ResetKeepsContentsButRequestsAnUpload)
{
    VulkanPackedUniformBufferState state{};
    state.initialize(4u);
    const gp::UInt32 value = 9u;
    state.setParameter(0u, sizeof(value), &value);
    state.clearDirty();

    state.reset();

    EXPECT_TRUE(state.isDirty());
    gp::UInt32 stored = 0u;
    gp::memory::copyMemory(&stored, state.getData(), sizeof(stored));
    EXPECT_EQ(stored, value);
}

}   // namespace gp::rhi::vulkan::tests
