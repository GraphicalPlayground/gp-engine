// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/NullGPUFence.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::null::tests
{

namespace
{

TEST(NullGPUFenceTest, PollsSignaledAndWaitIsImmediateNoOp)
{
    sync::GPUFenceRef fence(new NullGPUFence(gp::Name("UnitNullFence")));

    ASSERT_TRUE(fence.isValid());
    EXPECT_EQ(fence->getType(), resource::ResourceType::GPUFence);
    EXPECT_EQ(fence->getName(), gp::Name("UnitNullFence"));
    EXPECT_TRUE(fence->poll());

    fence->clear();
    EXPECT_TRUE(fence->poll());

    fence->wait(command::getImmediateCommandList(), GPUMask::all());
    EXPECT_TRUE(fence->poll());
}

}   // namespace

}   // namespace gp::rhi::null::tests
