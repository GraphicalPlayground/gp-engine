// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanShaderResourceView.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanLinkedView.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanViewableResource.hpp"
#include "RHIPixelFormat.hpp"
#include "views/ShaderResourceView.hpp"
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

static_assert(std::is_base_of_v<view::ShaderResourceView, VulkanShaderResourceView>, "SRV must be an RHI SRV.");
static_assert(std::is_base_of_v<VulkanView, VulkanShaderResourceView>, "SRV must own a native view.");
static_assert(std::is_base_of_v<VulkanLinkedView, VulkanShaderResourceView>, "SRV must follow resource renames.");
static_assert(std::is_final_v<VulkanShaderResourceView>, "SRV is a leaf type.");

using VulkanShaderResourceViewLiveTest = VulkanViewLiveTest;

TEST_F(VulkanShaderResourceViewLiveTest, TextureSRVBuildsImageViewAndLinks)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    ASSERT_TRUE(texture->isValid());
    const view::ViewDesc desc = view::ViewDesc::createTextureSRV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .asViewDesc();

    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), texture.get(), desc);
    ASSERT_NE(srv.get(), nullptr);
    VulkanShaderResourceView* const vulkanSRV = asVulkan(srv);

    EXPECT_TRUE(srv->isTexture());
    EXPECT_EQ(srv->getTexture(), texture.get());
    EXPECT_EQ(vulkanSRV->getBaseResource(), static_cast<VulkanViewableResource*>(texture.get()));
    EXPECT_TRUE(vulkanSRV->isLinked());
    EXPECT_EQ(vulkanSRV->getDescriptorType(), view::DescriptorType::TextureSRV);
    EXPECT_FALSE(srv->getBindlessHandle().isValid());

    ASSERT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::Texture);
    EXPECT_NE(vulkanSRV->getTextureView().view, static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(vulkanSRV->getTextureView().image, texture->getImage());
}

TEST_F(VulkanShaderResourceViewLiveTest, TextureSRVsOfOneTextureGetDistinctViews)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    const view::ViewDesc desc = view::ViewDesc::createTextureSRV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .asViewDesc();

    const view::ShaderResourceViewRef first = createVulkanShaderResourceView(cmdList(), texture.get(), desc);
    const view::ShaderResourceViewRef second = createVulkanShaderResourceView(cmdList(), texture.get(), desc);

    ASSERT_EQ(asVulkan(first)->getViewKind(), VulkanViewKind::Texture);
    ASSERT_EQ(asVulkan(second)->getViewKind(), VulkanViewKind::Texture);
    EXPECT_NE(asVulkan(first)->getTextureView().view, asVulkan(second)->getTextureView().view);
    EXPECT_NE(asVulkan(first)->getTextureView().viewId, asVulkan(second)->getTextureView().viewId);
}

TEST_F(VulkanShaderResourceViewLiveTest, CubeSRVUsesCubeViewType)
{
    const VulkanViewTestTextureRef texture = makeTextureCube(resource::TextureCreateFlags::ShaderResource);
    ASSERT_TRUE(texture->isValid());
    const view::ViewDesc desc = view::ViewDesc::createTextureSRV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .asViewDesc();

    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), texture.get(), desc);

    ASSERT_EQ(asVulkan(srv)->getViewKind(), VulkanViewKind::Texture);
    EXPECT_NE(asVulkan(srv)->getTextureView().view, static_cast<VkImageView>(VK_NULL_HANDLE));
}

TEST_F(VulkanShaderResourceViewLiveTest, TypedBufferSRVBuildsTexelBufferView)
{
    const VulkanViewTestBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::Static | resource::BufferUsageFlags::ShaderResource);
    const view::ViewDesc desc = view::ViewDesc::createBufferSRV()
                                    .setType(view::BufferViewType::Typed)
                                    .setFormat(PixelFormat::R32_UInt)
                                    .asViewDesc();

    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), buffer.get(), desc);
    VulkanShaderResourceView* const vulkanSRV = asVulkan(srv);

    EXPECT_TRUE(srv->isBuffer());
    EXPECT_TRUE(vulkanSRV->isLinked());
    EXPECT_EQ(vulkanSRV->getDescriptorType(), view::DescriptorType::TypedBufferSRV);
    ASSERT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::TypedBuffer);
    const VulkanTypedBufferView& state = vulkanSRV->getOrCreateTypedBufferView();
    EXPECT_NE(state.view, static_cast<VkBufferView>(VK_NULL_HANDLE));
    EXPECT_EQ(state.format, VK_FORMAT_R32_UINT);
    EXPECT_EQ(state.offset, buffer->getOffset());
    EXPECT_EQ(state.range, kBufferSize);
}

TEST_F(VulkanShaderResourceViewLiveTest, TypedBufferSRVHonorsOffsetAndElementCount)
{
    const gp::UInt32 alignment = static_cast<gp::UInt32>(m_device->getLimits().minTexelBufferOffsetAlignment);
    const gp::UInt32 viewOffset = (alignment > 4u) ? alignment : 4u;
    if ((viewOffset + 64u) > kBufferSize)
    {
        GTEST_SKIP() << "minTexelBufferOffsetAlignment too large for the test buffer.";
    }

    const VulkanViewTestBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::Static | resource::BufferUsageFlags::ShaderResource);
    const view::ViewDesc desc = view::ViewDesc::createBufferSRV()
                                    .setType(view::BufferViewType::Typed)
                                    .setFormat(PixelFormat::R32_UInt)
                                    .setOffsetInBytes(viewOffset)
                                    .setNumElements(16u)
                                    .asViewDesc();

    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), buffer.get(), desc);

    ASSERT_EQ(asVulkan(srv)->getViewKind(), VulkanViewKind::TypedBuffer);
    const VulkanTypedBufferView& state = asVulkan(srv)->getOrCreateTypedBufferView();
    EXPECT_EQ(state.offset, buffer->getOffset() + viewOffset);
    EXPECT_EQ(state.range, 64u);
}

TEST_F(VulkanShaderResourceViewLiveTest, StructuredBufferSRVIsAStorageBufferRange)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::Static | resource::BufferUsageFlags::StructuredBuffer |
            resource::BufferUsageFlags::ShaderResource,
        kBufferSize,
        16u
    );
    const view::ViewDesc desc = view::ViewDesc::createBufferSRV().setTypeFromBuffer(buffer.get()).asViewDesc();

    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), buffer.get(), desc);
    VulkanShaderResourceView* const vulkanSRV = asVulkan(srv);

    EXPECT_EQ(vulkanSRV->getDescriptorType(), view::DescriptorType::BufferUAV);
    ASSERT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::StructuredBuffer);
    const VulkanStructuredBufferView& state = vulkanSRV->getStructuredBufferView();
    EXPECT_EQ(state.buffer, buffer->getHandle());
    EXPECT_EQ(state.offset, buffer->getOffset());
    EXPECT_EQ(state.size, kBufferSize);
}

TEST_F(VulkanShaderResourceViewLiveTest, RawBufferSRVIsAStorageBufferRange)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::Static | resource::BufferUsageFlags::ByteAddressBuffer |
        resource::BufferUsageFlags::ShaderResource
    );
    const view::ViewDesc desc = view::ViewDesc::createBufferSRV().setType(view::BufferViewType::Raw).asViewDesc();

    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), buffer.get(), desc);

    EXPECT_EQ(asVulkan(srv)->getDescriptorType(), view::DescriptorType::BufferUAV);
    ASSERT_EQ(asVulkan(srv)->getViewKind(), VulkanViewKind::StructuredBuffer);
    EXPECT_EQ(asVulkan(srv)->getStructuredBufferView().size, kBufferSize);
}

TEST_F(VulkanShaderResourceViewLiveTest, NullBufferSRVHoldsNoNativeView)
{
    const VulkanViewTestBufferRef buffer =
        VulkanViewTestBufferRef(new VulkanBuffer(m_services, resource::BufferCreateDesc::createNull("NullBuffer")));
    const view::ViewDesc desc = view::ViewDesc::createBufferSRV()
                                    .setType(view::BufferViewType::Typed)
                                    .setFormat(PixelFormat::R32_UInt)
                                    .asViewDesc();

    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), buffer.get(), desc);

    EXPECT_TRUE(asVulkan(srv)->isLinked());
    EXPECT_EQ(asVulkan(srv)->getViewKind(), VulkanViewKind::Null);
    EXPECT_FALSE(asVulkan(srv)->isInitialized());
}

TEST_F(VulkanShaderResourceViewLiveTest, UpdateViewRebuildsAfterInvalidate)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    const view::ViewDesc desc = view::ViewDesc::createTextureSRV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .asViewDesc();
    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), texture.get(), desc);
    VulkanShaderResourceView* const vulkanSRV = asVulkan(srv);
    ASSERT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::Texture);
    const gp::UInt32 firstId = vulkanSRV->getTextureView().viewId;

    vulkanSRV->invalidate();
    EXPECT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::Null);
    EXPECT_TRUE(vulkanSRV->isInitialized());

    vulkanSRV->updateView(allocator::ContextArray{});
    ASSERT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::Texture);
    EXPECT_NE(vulkanSRV->getTextureView().viewId, firstId);
}

TEST_F(VulkanShaderResourceViewLiveTest, BufferRenameRefreshesTheStorageRange)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::Dynamic | resource::BufferUsageFlags::StructuredBuffer |
            resource::BufferUsageFlags::ShaderResource,
        kBufferSize,
        16u
    );
    const view::ViewDesc desc = view::ViewDesc::createBufferSRV().setTypeFromBuffer(buffer.get()).asViewDesc();
    const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), buffer.get(), desc);
    VulkanShaderResourceView* const vulkanSRV = asVulkan(srv);
    ASSERT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::StructuredBuffer);

    // The first write lock maps in place; the second one renames to a new allocation.
    static_cast<void>(buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, kBufferSize, 0u));
    buffer->unlock(cmdList());
    const gp::UInt32 previousOffset = vulkanSRV->getStructuredBufferView().offset;
    const gp::UInt32 previousHandleId = vulkanSRV->getStructuredBufferView().handleId;
    static_cast<void>(buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, kBufferSize, 0u));
    buffer->unlock(cmdList());

    ASSERT_EQ(vulkanSRV->getViewKind(), VulkanViewKind::StructuredBuffer);
    const VulkanStructuredBufferView& state = vulkanSRV->getStructuredBufferView();
    EXPECT_EQ(state.buffer, buffer->getHandle());
    EXPECT_EQ(state.offset, buffer->getOffset());
    EXPECT_EQ(state.handleId, buffer->getCurrentBufferAlloc().alloc.handleId);
    EXPECT_TRUE((state.offset != previousOffset) || (state.handleId != previousHandleId));
}

TEST_F(VulkanShaderResourceViewLiveTest, ReleasingTheViewUnlinksIt)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    const view::ViewDesc desc = view::ViewDesc::createTextureSRV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .asViewDesc();
    {
        const view::ShaderResourceViewRef srv = createVulkanShaderResourceView(cmdList(), texture.get(), desc);
        ASSERT_TRUE(asVulkan(srv)->isLinked());
    }

    // An unlinked list means a later rename has nothing to refresh and touches no freed view.
    texture->updateLinkedViews(allocator::ContextArray{});
    SUCCEED();
}

}   // namespace gp::rhi::vulkan::tests
