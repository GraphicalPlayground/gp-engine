// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/NullBuffer.hpp"
#include "core/ResourceEnums.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/BufferCreateDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::null::tests
{

namespace
{

TEST(NullBufferTest, RetainsCreateDescriptorAndResourceMetadata)
{
    const resource::BufferCreateDesc desc = resource::BufferCreateDesc::create(
        "UnitNullBuffer",
        256u,
        16u,
        resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::ShaderResource
    );
    resource::BufferRef buffer(new NullBuffer(desc));

    ASSERT_TRUE(buffer.isValid());
    EXPECT_EQ(buffer->getType(), resource::ResourceType::Buffer);
    EXPECT_EQ(buffer->getSize(), 256u);
    EXPECT_EQ(buffer->getStride(), 16u);
    EXPECT_EQ(
        buffer->getUsage(), resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::ShaderResource
    );
}

}   // namespace

}   // namespace gp::rhi::null::tests
