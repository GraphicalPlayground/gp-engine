// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanUnorderedAccessView.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "CoreMinimal.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanLinkedView.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanViewableResource.hpp"
#include "RHIPixelFormat.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"
#include "views/ViewForward.hpp"
#include "views/VulkanView.hpp"
#include "views/VulkanViewEnums.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include "VulkanViewTestFixture.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::vulkan::tests
{

static_assert(std::is_base_of_v<view::UnorderedAccessView, VulkanUnorderedAccessView>, "UAV must be an RHI UAV.");
static_assert(std::is_base_of_v<VulkanView, VulkanUnorderedAccessView>, "UAV must own a native view.");
static_assert(std::is_base_of_v<VulkanLinkedView, VulkanUnorderedAccessView>, "UAV must follow resource renames.");
static_assert(std::is_final_v<VulkanUnorderedAccessView>, "UAV is a leaf type.");

using VulkanUnorderedAccessViewLiveTest = VulkanViewLiveTest;

TEST_F(VulkanUnorderedAccessViewLiveTest, TextureUAVBuildsStorageImageView)
{
    const VulkanViewTestTextureRef texture =
        makeTexture2D(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::UAV);
    ASSERT_TRUE(texture->isValid());
    const view::ViewDesc desc = view::ViewDesc::createTextureUAV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .setMipLevel(0u)
                                    .asViewDesc();

    const view::UnorderedAccessViewRef uav = createVulkanUnorderedAccessView(cmdList(), texture.get(), desc);
    ASSERT_NE(uav.get(), nullptr);
    VulkanUnorderedAccessView* const vulkanUAV = asVulkan(uav);

    EXPECT_TRUE(uav->isTexture());
    EXPECT_EQ(vulkanUAV->getBaseResource(), static_cast<VulkanViewableResource*>(texture.get()));
    EXPECT_TRUE(vulkanUAV->isLinked());
    EXPECT_EQ(vulkanUAV->getDescriptorType(), view::DescriptorType::TextureUAV);
    EXPECT_FALSE(uav->getBindlessHandle().isValid());

    ASSERT_EQ(vulkanUAV->getViewKind(), VulkanViewKind::Texture);
    EXPECT_NE(vulkanUAV->getTextureView().view, static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(vulkanUAV->getTextureView().image, texture->getImage());
}

TEST_F(VulkanUnorderedAccessViewLiveTest, CubeUAVIsBuilt)
{
    const VulkanViewTestTextureRef texture =
        makeTextureCube(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::UAV);
    ASSERT_TRUE(texture->isValid());
    const view::ViewDesc desc = view::ViewDesc::createTextureUAV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .asViewDesc();

    const view::UnorderedAccessViewRef uav = createVulkanUnorderedAccessView(cmdList(), texture.get(), desc);

    ASSERT_EQ(asVulkan(uav)->getViewKind(), VulkanViewKind::Texture);
    EXPECT_NE(asVulkan(uav)->getTextureView().view, static_cast<VkImageView>(VK_NULL_HANDLE));
}

TEST_F(VulkanUnorderedAccessViewLiveTest, TypedBufferUAVBuildsStorageTexelView)
{
    const VulkanViewTestBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::Static | resource::BufferUsageFlags::UnorderedAccess);
    const view::ViewDesc desc = view::ViewDesc::createBufferUAV()
                                    .setType(view::BufferViewType::Typed)
                                    .setFormat(PixelFormat::R32_UInt)
                                    .asViewDesc();

    const view::UnorderedAccessViewRef uav = createVulkanUnorderedAccessView(cmdList(), buffer.get(), desc);
    VulkanUnorderedAccessView* const vulkanUAV = asVulkan(uav);

    EXPECT_TRUE(uav->isBuffer());
    EXPECT_TRUE(vulkanUAV->isLinked());
    EXPECT_EQ(vulkanUAV->getDescriptorType(), view::DescriptorType::TypedBufferUAV);
    ASSERT_EQ(vulkanUAV->getViewKind(), VulkanViewKind::TypedBuffer);
    const VulkanTypedBufferView& state = vulkanUAV->getOrCreateTypedBufferView();
    EXPECT_NE(state.view, static_cast<VkBufferView>(VK_NULL_HANDLE));
    EXPECT_EQ(state.format, VK_FORMAT_R32_UINT);
    EXPECT_EQ(state.range, kBufferSize);
}

TEST_F(VulkanUnorderedAccessViewLiveTest, StructuredBufferUAVIsAStorageBufferRange)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::Static | resource::BufferUsageFlags::StructuredBuffer |
            resource::BufferUsageFlags::UnorderedAccess,
        kBufferSize,
        16u
    );
    const view::ViewDesc desc = view::ViewDesc::createBufferUAV().setTypeFromBuffer(buffer.get()).asViewDesc();

    const view::UnorderedAccessViewRef uav = createVulkanUnorderedAccessView(cmdList(), buffer.get(), desc);

    EXPECT_EQ(asVulkan(uav)->getDescriptorType(), view::DescriptorType::BufferUAV);
    ASSERT_EQ(asVulkan(uav)->getViewKind(), VulkanViewKind::StructuredBuffer);
    const VulkanStructuredBufferView& state = asVulkan(uav)->getStructuredBufferView();
    EXPECT_EQ(state.buffer, buffer->getHandle());
    EXPECT_EQ(state.offset, buffer->getOffset());
    EXPECT_EQ(state.size, kBufferSize);
}

TEST_F(VulkanUnorderedAccessViewLiveTest, RawBufferUAVIsAStorageBufferRange)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::Static | resource::BufferUsageFlags::ByteAddressBuffer |
        resource::BufferUsageFlags::UnorderedAccess
    );
    const view::ViewDesc desc = view::ViewDesc::createBufferUAV().setType(view::BufferViewType::Raw).asViewDesc();

    const view::UnorderedAccessViewRef uav = createVulkanUnorderedAccessView(cmdList(), buffer.get(), desc);

    ASSERT_EQ(asVulkan(uav)->getViewKind(), VulkanViewKind::StructuredBuffer);
    EXPECT_EQ(asVulkan(uav)->getStructuredBufferView().size, kBufferSize);
}

TEST_F(VulkanUnorderedAccessViewLiveTest, UpdateViewRebuildsAfterInvalidate)
{
    const VulkanViewTestBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::Static | resource::BufferUsageFlags::UnorderedAccess);
    const view::ViewDesc desc = view::ViewDesc::createBufferUAV()
                                    .setType(view::BufferViewType::Typed)
                                    .setFormat(PixelFormat::R32_UInt)
                                    .asViewDesc();
    const view::UnorderedAccessViewRef uav = createVulkanUnorderedAccessView(cmdList(), buffer.get(), desc);
    VulkanUnorderedAccessView* const vulkanUAV = asVulkan(uav);
    ASSERT_EQ(vulkanUAV->getViewKind(), VulkanViewKind::TypedBuffer);
    const gp::UInt32 firstId = vulkanUAV->getOrCreateTypedBufferView().viewId;

    vulkanUAV->invalidate();
    EXPECT_EQ(vulkanUAV->getViewKind(), VulkanViewKind::Null);
    EXPECT_TRUE(vulkanUAV->isInitialized());

    vulkanUAV->updateView(allocator::ContextArray{});
    ASSERT_EQ(vulkanUAV->getViewKind(), VulkanViewKind::TypedBuffer);
    EXPECT_NE(vulkanUAV->getOrCreateTypedBufferView().viewId, firstId);
    EXPECT_NE(vulkanUAV->getOrCreateTypedBufferView().view, static_cast<VkBufferView>(VK_NULL_HANDLE));
}

}   // namespace gp::rhi::vulkan::tests
