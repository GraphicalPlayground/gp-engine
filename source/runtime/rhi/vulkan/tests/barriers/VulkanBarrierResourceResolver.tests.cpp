// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierResourceResolver.hpp"
#include "../views/VulkanViewTestFixture.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanBarrierResourceResolverTest, TransitionWithoutResourceResolvesToNone)
{
    const VulkanBarrierResourceResolver resolver{};
    const sync::TransitionInfo info{};
    EXPECT_EQ(resolver.resolve(info).type, VulkanBarrierResourceType::None);
}

class VulkanBarrierResourceResolverLiveTest : public VulkanViewLiveTest
{};

TEST_F(VulkanBarrierResourceResolverLiveTest, BufferResolvesToItsHandle)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(resource::BufferUsageFlags::Static);
    const VulkanBarrierResource resolved = VulkanBarrierResourceResolver::resolveBuffer(*buffer);
    EXPECT_EQ(resolved.type, VulkanBarrierResourceType::Buffer);
    EXPECT_EQ(resolved.buffer, buffer->getHandle());
    EXPECT_EQ(resolved.image, VK_NULL_HANDLE);
}

TEST_F(VulkanBarrierResourceResolverLiveTest, TextureResolvesToItsImageAndShape)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    const VulkanBarrierResource resolved = VulkanBarrierResourceResolver::resolveTexture(*texture);
    EXPECT_EQ(resolved.type, VulkanBarrierResourceType::Texture);
    EXPECT_EQ(resolved.image, texture->getImage());
    EXPECT_EQ(resolved.fullAspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    EXPECT_FALSE(resolved.isDepthOrStencilAspect);
    EXPECT_FALSE(resolved.is3DTexture);
    EXPECT_FALSE(resolved.isReservedResource);
    EXPECT_EQ(resolved.numMips, 1u);
    EXPECT_EQ(resolved.numArrayLayers, 1u);
}

TEST_F(VulkanBarrierResourceResolverLiveTest, ResolveDispatchesOnTheTransitionType)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(resource::BufferUsageFlags::Static);
    sync::TransitionInfo info{};
    info.buffer = buffer.get();
    info.type = sync::TransitionResourceType::Buffer;

    const VulkanBarrierResourceResolver resolver{};
    const VulkanBarrierResource resolved = resolver.resolve(info);
    EXPECT_EQ(resolved.type, VulkanBarrierResourceType::Buffer);
    EXPECT_EQ(resolved.buffer, buffer->getHandle());

    info.type = sync::TransitionResourceType::Unknown;
    EXPECT_EQ(resolver.resolve(info).type, VulkanBarrierResourceType::None);
}

}   // namespace gp::rhi::vulkan::tests
