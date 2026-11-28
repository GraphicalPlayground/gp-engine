// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/NullTexture.hpp"
#include "core/ResourceEnums.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/TextureCreateDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::null::tests
{

namespace
{

TEST(NullTextureTest, RetainsCreateDescriptorAndTextureProperties)
{
    const resource::TextureCreateDesc desc = resource::TextureCreateDesc::create2D("UnitNullTexture")
                                                 .setExtent(64, 32)
                                                 .setNumMips(4u)
                                                 .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                                 .addFlags(resource::TextureCreateFlags::ShaderResource);
    resource::TextureRef texture(new NullTexture(desc));

    ASSERT_TRUE(texture.isValid());
    EXPECT_EQ(texture->getType(), resource::ResourceType::Texture);
    EXPECT_EQ(texture->getDesc().extent.x, 64);
    EXPECT_EQ(texture->getDesc().extent.y, 32);
    EXPECT_EQ(texture->getNumMips(), 4u);
    EXPECT_EQ(texture->getFormat(), PixelFormat::R8G8B8A8_UNorm);
}

}   // namespace

}   // namespace gp::rhi::null::tests
