// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/NullUniformBuffer.hpp"
#include "core/ResourceEnums.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::null::tests
{

namespace
{

TEST(NullUniformBufferTest, RetainsBoundLayoutAndSize)
{
    resource::UniformBufferLayoutDesc layoutDesc("UnitNullUB");
    layoutDesc.constantBufferSize = 64u;
    resource::UniformBufferLayoutRef layout(new resource::UniformBufferLayout(layoutDesc));
    resource::UniformBufferRef uniformBuffer(new NullUniformBuffer(layout.get()));

    ASSERT_TRUE(uniformBuffer.isValid());
    EXPECT_EQ(uniformBuffer->getType(), resource::ResourceType::UniformBuffer);
    EXPECT_EQ(uniformBuffer->getLayoutPtr(), layout.get());
    EXPECT_EQ(uniformBuffer->getSize(), 64u);
}

}   // namespace

}   // namespace gp::rhi::null::tests
