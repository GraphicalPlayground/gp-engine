// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanView.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "CoreMinimal.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"
#include "views/VulkanImageViewCreateInfo.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "views/VulkanViewEnums.hpp"
#include "views/VulkanViewTranslation.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include "VulkanViewTestFixture.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::vulkan::tests
{

static_assert(!std::is_copy_constructible_v<VulkanView>, "VulkanView owns a native object and must not be copied.");
static_assert(!std::is_move_constructible_v<VulkanView>, "Linked views are addressed by pointer and must not move.");
static_assert(!std::is_copy_constructible_v<VulkanImageViewCreateInfo>, "The pNext chain points into the object.");
static_assert(std::is_trivially_copyable_v<VulkanTypedBufferView>, "View arms must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<VulkanTextureView>, "View arms must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<VulkanStructuredBufferView>, "View arms must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<VulkanAccelerationStructureView>, "View arms must be trivially copyable.");

/// @brief Builds views directly, without the RHI view wrappers.
class VulkanViewLiveTestDirect : public VulkanViewLiveTest
{
protected:
    /// @brief Returns the SRV description of every mip and slice of `texture`.
    [[nodiscard]] static VulkanTextureViewDesc makeFullSRVDesc(VulkanTexture& texture) noexcept
    {
        const view::ViewDesc desc = view::ViewDesc::createTextureSRV()
                                        .setDimensionFromTexture(&texture)
                                        .setFormat(texture.getDesc().format)
                                        .asViewDesc();
        return translateVulkanTextureViewDesc(texture, desc.texture.srv.getViewInfo(&texture));
    }
};

TEST_F(VulkanViewLiveTestDirect, NewViewIsNullAndUninitialized)
{
    const VulkanView view(m_services, view::DescriptorType::Invalid);

    EXPECT_EQ(view.getViewKind(), VulkanViewKind::Null);
    EXPECT_FALSE(view.isInitialized());
    EXPECT_FALSE(view.getBindlessHandle().isValid());
    EXPECT_EQ(view.getDescriptorType(), view::DescriptorType::Invalid);
}

TEST_F(VulkanViewLiveTestDirect, DescriptorTypeIsKeptWithoutBindlessSlot)
{
    const VulkanView view(m_services, view::DescriptorType::TextureSRV);

    EXPECT_EQ(view.getDescriptorType(), view::DescriptorType::TextureSRV);
    EXPECT_FALSE(view.getBindlessHandle().isValid());
}

TEST_F(VulkanViewLiveTestDirect, CreateTextureViewBuildsImageView)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    ASSERT_TRUE(texture->isValid());

    VulkanView view(m_services, view::DescriptorType::Invalid);
    view.createTextureView(texture->getImage(), makeFullSRVDesc(*texture));

    ASSERT_EQ(view.getViewKind(), VulkanViewKind::Texture);
    EXPECT_TRUE(view.isInitialized());
    EXPECT_NE(view.getTextureView().view, static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(view.getTextureView().image, texture->getImage());
    EXPECT_NE(view.getTextureView().viewId, 0u);
}

TEST_F(VulkanViewLiveTestDirect, InvalidateReturnsToNullButStaysInitialized)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    VulkanView view(m_services, view::DescriptorType::Invalid);
    view.createTextureView(texture->getImage(), makeFullSRVDesc(*texture));
    ASSERT_EQ(view.getViewKind(), VulkanViewKind::Texture);

    view.invalidate();
    EXPECT_EQ(view.getViewKind(), VulkanViewKind::Null);
    EXPECT_TRUE(view.isInitialized());

    // A second invalidate has nothing to release and keeps the flag.
    view.invalidate();
    EXPECT_EQ(view.getViewKind(), VulkanViewKind::Null);
    EXPECT_TRUE(view.isInitialized());
}

TEST_F(VulkanViewLiveTestDirect, RebuildAssignsANewViewId)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    const VulkanTextureViewDesc desc = makeFullSRVDesc(*texture);
    VulkanView view(m_services, view::DescriptorType::Invalid);

    view.createTextureView(texture->getImage(), desc);
    const gp::UInt32 firstId = view.getTextureView().viewId;
    view.invalidate();
    view.updateTextureView(allocator::ContextArray{}, texture->getImage(), desc);

    ASSERT_EQ(view.getViewKind(), VulkanViewKind::Texture);
    EXPECT_NE(view.getTextureView().viewId, firstId);
}

TEST_F(VulkanViewLiveTestDirect, StructuredBufferViewCoversTheAllocation)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::Static | resource::BufferUsageFlags::StructuredBuffer |
            resource::BufferUsageFlags::ShaderResource,
        kBufferSize,
        16u
    );
    VulkanView view(m_services, view::DescriptorType::BufferUAV);
    view.initAsStructuredBufferView(allocator::ContextArray{}, *buffer, 32u, 256u);

    ASSERT_EQ(view.getViewKind(), VulkanViewKind::StructuredBuffer);
    const VulkanStructuredBufferView& state = view.getStructuredBufferView();
    EXPECT_EQ(state.buffer, buffer->getHandle());
    EXPECT_EQ(state.handleId, buffer->getCurrentBufferAlloc().alloc.handleId);
    EXPECT_EQ(state.offset, buffer->getOffset() + 32u);
    EXPECT_EQ(state.size, 256u);
}

TEST_F(VulkanViewLiveTestDirect, TypedBufferViewIsCreatedEagerlyWithoutBindless)
{
    const VulkanViewTestBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::Static | resource::BufferUsageFlags::ShaderResource);
    VulkanView view(m_services, view::DescriptorType::TypedBufferSRV);
    view.initAsTypedBufferView(allocator::ContextArray{}, *buffer, PixelFormat::R32_UInt, 0u, kBufferSize);

    ASSERT_EQ(view.getViewKind(), VulkanViewKind::TypedBuffer);
    const VulkanTypedBufferView& state = view.getOrCreateTypedBufferView();
    EXPECT_NE(state.view, static_cast<VkBufferView>(VK_NULL_HANDLE));
    EXPECT_EQ(state.buffer, buffer->getHandle());
    EXPECT_EQ(state.format, VK_FORMAT_R32_UINT);
    EXPECT_EQ(state.offset, buffer->getOffset());
    EXPECT_EQ(state.range, kBufferSize);
    EXPECT_NE(state.viewId, 0u);
}

TEST_F(VulkanViewLiveTestDirect, TypedBufferViewIdsAreUnique)
{
    const VulkanViewTestBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::Static | resource::BufferUsageFlags::ShaderResource);
    VulkanView first(m_services, view::DescriptorType::TypedBufferSRV);
    VulkanView second(m_services, view::DescriptorType::TypedBufferSRV);
    first.initAsTypedBufferView(allocator::ContextArray{}, *buffer, PixelFormat::R32_UInt, 0u, kBufferSize);
    second.initAsTypedBufferView(allocator::ContextArray{}, *buffer, PixelFormat::R32_UInt, 0u, kBufferSize);

    EXPECT_NE(first.getOrCreateTypedBufferView().viewId, second.getOrCreateTypedBufferView().viewId);
}

TEST_F(VulkanViewLiveTestDirect, ImageViewCreateInfoChainsTheUsageRestriction)
{
    const VulkanViewTestTextureRef texture =
        makeTexture2D(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::UAV);
    const VulkanTextureViewDesc desc = makeFullSRVDesc(*texture);
    ASSERT_NE(desc.imageUsageFlags, 0u);

    const VulkanImageViewCreateInfo info(*m_device, texture->getImage(), desc);

    EXPECT_EQ(info.createInfo.sType, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    EXPECT_EQ(info.createInfo.image, texture->getImage());
    EXPECT_EQ(info.createInfo.viewType, VK_IMAGE_VIEW_TYPE_2D);
    EXPECT_EQ(info.createInfo.format, desc.format);
    EXPECT_EQ(info.createInfo.subresourceRange.levelCount, desc.numMips);
    EXPECT_EQ(info.createInfo.subresourceRange.layerCount, desc.numArraySlices);

    ASSERT_NE(info.createInfo.pNext, nullptr);
    const VkImageViewUsageCreateInfo* const usage =
        static_cast<const VkImageViewUsageCreateInfo*>(info.createInfo.pNext);
    EXPECT_EQ(usage->sType, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
    EXPECT_EQ(usage->usage, desc.imageUsageFlags);
}

TEST_F(VulkanViewLiveTestDirect, ImageViewCreateInfoHonorsIdentitySwizzleAndStencilPlane)
{
    const VulkanViewTestTextureRef texture = makeTexture2D(resource::TextureCreateFlags::ShaderResource);
    VulkanTextureViewDesc desc = makeFullSRVDesc(*texture);
    desc.useIdentitySwizzle = true;
    desc.imageUsageFlags = 0u;

    const VulkanImageViewCreateInfo identity(*m_device, texture->getImage(), desc);
    EXPECT_EQ(identity.createInfo.pNext, nullptr);
    EXPECT_EQ(identity.createInfo.components.r, VK_COMPONENT_SWIZZLE_IDENTITY);
    EXPECT_EQ(identity.createInfo.components.a, VK_COMPONENT_SWIZZLE_IDENTITY);

    // Only the create info is checked: the stencil plane request rewrites the aspect whatever the image format.
    desc.pixelFormat = PixelFormat::X24_G8_UInt;
    const VulkanImageViewCreateInfo stencil(*m_device, texture->getImage(), desc);
    EXPECT_EQ(
        stencil.createInfo.subresourceRange.aspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_STENCIL_BIT)
    );
}

TEST_F(VulkanViewLiveTestDirect, SRVTranslationStripsStorageUsageAndHonorsSRGB)
{
    const VulkanViewTestTextureRef texture =
        makeTexture2D(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::SRGB);
    const view::ViewDesc srgbDesc = view::ViewDesc::createTextureSRV()
                                        .setDimensionFromTexture(texture.get())
                                        .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                        .asViewDesc();
    const view::ViewDesc linearDesc = view::ViewDesc::createTextureSRV()
                                          .setDimensionFromTexture(texture.get())
                                          .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                          .setDisableSRGB(true)
                                          .asViewDesc();

    const VulkanTextureViewDesc srgb =
        translateVulkanTextureViewDesc(*texture, srgbDesc.texture.srv.getViewInfo(texture.get()));
    const VulkanTextureViewDesc linear =
        translateVulkanTextureViewDesc(*texture, linearDesc.texture.srv.getViewInfo(texture.get()));

    EXPECT_EQ(srgb.format, VK_FORMAT_R8G8B8A8_SRGB);
    EXPECT_EQ(linear.format, VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(srgb.viewType, VK_IMAGE_VIEW_TYPE_2D);
    EXPECT_EQ(srgb.aspectFlags, texture->getPartialAspectMask());
    EXPECT_EQ(srgb.numMips, static_cast<gp::UInt32>(texture->getDesc().numMips));
    EXPECT_EQ(srgb.numArraySlices, 1u);
    EXPECT_FALSE(srgb.useIdentitySwizzle);
    EXPECT_EQ(srgb.imageUsageFlags & static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT), 0u);
}

TEST_F(VulkanViewLiveTestDirect, UAVTranslationTargetsOneLinearMipWithIdentitySwizzle)
{
    const VulkanViewTestTextureRef texture =
        makeTexture2D(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::UAV);
    const view::ViewDesc desc = view::ViewDesc::createTextureUAV()
                                    .setDimensionFromTexture(texture.get())
                                    .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                    .setMipLevel(0u)
                                    .asViewDesc();

    const VulkanTextureViewDesc uav =
        translateVulkanTextureViewDesc(*texture, desc.texture.uav.getViewInfo(texture.get()));

    EXPECT_EQ(uav.format, VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(uav.firstMip, 0u);
    EXPECT_EQ(uav.numMips, 1u);
    EXPECT_TRUE(uav.useIdentitySwizzle);
    EXPECT_EQ(uav.imageUsageFlags, 0u);
}

TEST_F(VulkanViewLiveTestDirect, CubeTranslationCountsFacesAsLayers)
{
    const VulkanViewTestTextureRef texture =
        makeTextureCube(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::UAV);
    ASSERT_TRUE(texture->isValid());

    const view::ViewDesc srvDesc = view::ViewDesc::createTextureSRV()
                                       .setDimensionFromTexture(texture.get())
                                       .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                       .asViewDesc();
    const view::ViewDesc uavDesc = view::ViewDesc::createTextureUAV()
                                       .setDimensionFromTexture(texture.get())
                                       .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                       .asViewDesc();

    const VulkanTextureViewDesc srv =
        translateVulkanTextureViewDesc(*texture, srvDesc.texture.srv.getViewInfo(texture.get()));
    const VulkanTextureViewDesc uav =
        translateVulkanTextureViewDesc(*texture, uavDesc.texture.uav.getViewInfo(texture.get()));

    EXPECT_EQ(srv.viewType, VK_IMAGE_VIEW_TYPE_CUBE);
    EXPECT_EQ(srv.arraySliceIndex, 0u);
    EXPECT_EQ(srv.numArraySlices, resource::kCubeFaceCount);
    EXPECT_EQ(uav.viewType, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    EXPECT_EQ(uav.numArraySlices, resource::kCubeFaceCount);
}

}   // namespace gp::rhi::vulkan::tests
