// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTexture.hpp"
#include "containers/arrays/Vector.hpp"
#include "core/ClearValueBinding.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/Texture.hpp"   // IWYU pragma: keep
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/TextureReference.hpp"
#include "resources/VulkanTextureEnums.hpp"
#include "resources/VulkanTextureInitializer.hpp"
#include "resources/VulkanTextureLocking.hpp"
#include "resources/VulkanTextureReadback.hpp"
#include "resources/VulkanTextureReference.hpp"
#include "resources/VulkanTextureSize.hpp"
#include "resources/VulkanTextureStats.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"     // IWYU pragma: keep
#include "VulkanResourceTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

constexpr gp::Int32 kTextureSize = 4;
constexpr gp::UInt32 kTexelBytes = 4u;
constexpr gp::UInt32 kMip0Bytes = static_cast<gp::UInt32>(kTextureSize * kTextureSize) * kTexelBytes;

/// @brief Bulk data source that records whether the backend released it.
class TrackingBulkData final : public resource::IResourceBulkData
{
public:
    TrackingBulkData() noexcept
    {
        for (gp::UInt32 index = 0u; index < kMip0Bytes; ++index)
        {
            bytes[index] = static_cast<gp::UInt8>(index);
        }
    }

    [[nodiscard]] virtual const void* getResourceBulkData() const noexcept override
    {
        return bytes;
    }

    [[nodiscard]] virtual gp::UInt32 getResourceBulkDataSize() const noexcept override
    {
        return kMip0Bytes;
    }

    virtual void discard() noexcept override
    {
        isDiscarded = true;
    }

    gp::UInt8 bytes[kMip0Bytes]{};
    bool isDiscarded{ false };
};

using VulkanTextureRef = gp::RefCountedPtr<VulkanTexture>;

[[nodiscard]] VulkanTexture* asVulkan(const resource::TextureRef& texture) noexcept
{
    return static_cast<VulkanTexture*>(texture.get());
}

}   // namespace

class VulkanTextureLiveTest : public VulkanResourceLiveTest
{
protected:
    /// @brief Builds a sampled 4x4 RGBA8 2D descriptor.
    [[nodiscard]] static resource::TextureCreateDesc makeSampled2D() noexcept
    {
        resource::TextureCreateDesc desc =
            resource::TextureCreateDesc::create2D("Sampled2D", kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm);
        desc.setFlags(resource::TextureCreateFlags::ShaderResource);
        return desc;
    }

    /// @brief Builds a 4x4 RGBA8 CPU readback descriptor.
    [[nodiscard]] static resource::TextureCreateDesc makeReadback2D() noexcept
    {
        resource::TextureCreateDesc desc = resource::TextureCreateDesc::create2D(
            "Readback2D", kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm
        );
        desc.setFlags(resource::TextureCreateFlags::CPUReadback);
        return desc;
    }

    /// @brief Creates a texture straight from the constructor.
    [[nodiscard]] VulkanTextureRef createTexture(const resource::TextureCreateDesc& desc) noexcept
    {
        return VulkanTextureRef(new VulkanTexture(m_services, cmdList(), desc));
    }

    /// @brief Forgets every command recorded so far.
    void resetRecorders() noexcept
    {
        m_provider.uploadRecorder = FakeTransferRecorder{};
        m_provider.commandListRecorder = FakeTransferRecorder{};
        m_provider.submitCount = 0u;
    }
};

TEST_F(VulkanTextureLiveTest, Sampled2DOwnsImageAndDefaultView)
{
    const VulkanTextureRef texture = createTexture(makeSampled2D());

    ASSERT_TRUE(texture->isValid());
    EXPECT_NE(texture->getImage(), VK_NULL_HANDLE);
    EXPECT_NE(texture->getDefaultView(), VK_NULL_HANDLE);
    EXPECT_EQ(texture->getViewFormat(), VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(texture->getFullAspectMask(), static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    EXPECT_TRUE(texture->supportsSampling());
    EXPECT_FALSE(texture->isDepthOrStencilAspect());
    EXPECT_EQ(texture->getOwnerType(), VulkanImageOwnerType::LocalOwner);
    EXPECT_EQ(texture->getNativeResource(), static_cast<void*>(texture->getImage()));
    EXPECT_EQ(texture->getNativeShaderResourceView(), static_cast<void*>(texture->getDefaultView()));
    EXPECT_GT(texture->getMemoryRequirements().size, 0u);
}

TEST_F(VulkanTextureLiveTest, DefaultActionLeavesUndefinedWithoutClear)
{
    resetRecorders();
    const VulkanTextureRef texture = createTexture(makeSampled2D());
    ASSERT_TRUE(texture->isValid());

    const FakeTransferRecorder& upload = m_provider.uploadRecorder;
    EXPECT_EQ(upload.clearCount, 0u);
    ASSERT_EQ(upload.transitionCount, 1u);
    EXPECT_EQ(upload.transitions[0].image, texture->getImage());
    EXPECT_EQ(upload.transitions[0].oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(upload.transitions[0].newLayout, texture->getDefaultLayout());
    EXPECT_NE(texture->getDefaultLayout(), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_F(VulkanTextureLiveTest, RenderTargetWithClearValueIsClearedOnCreation)
{
    resource::TextureCreateDesc desc =
        resource::TextureCreateDesc::create2D("ClearedRT", kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm);
    desc.setFlags(resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource);
    desc.setClearValue(resource::ClearValueBinding(gp::math::LinearColor(0.25f, 0.5f, 0.75f, 1.0f)));

    resetRecorders();
    const VulkanTextureRef texture = createTexture(desc);
    ASSERT_TRUE(texture->isValid());

    const FakeTransferRecorder& upload = m_provider.uploadRecorder;
    ASSERT_EQ(upload.clearCount, 1u);
    EXPECT_FALSE(upload.clears[0].isDepthStencil);
    EXPECT_EQ(upload.clears[0].layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_FLOAT_EQ(upload.clears[0].color.float32[0], 0.25f);
    EXPECT_FLOAT_EQ(upload.clears[0].color.float32[1], 0.5f);
    EXPECT_FLOAT_EQ(upload.clears[0].color.float32[2], 0.75f);
    EXPECT_FLOAT_EQ(upload.clears[0].color.float32[3], 1.0f);

    ASSERT_EQ(upload.transitionCount, 2u);
    EXPECT_EQ(upload.transitions[0].oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(upload.transitions[0].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(upload.transitions[1].oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(upload.transitions[1].newLayout, texture->getDefaultLayout());
}

TEST_F(VulkanTextureLiveTest, DepthStencilTargetUsesDepthAspectsAndClearsDepth)
{
    resource::TextureCreateDesc desc =
        resource::TextureCreateDesc::create2D("Depth", kTextureSize, kTextureSize, PixelFormat::D32_Float);
    desc.setFlags(resource::TextureCreateFlags::DepthStencilTargetable);
    desc.setClearValue(resource::ClearValueBinding(1.0f, 0u));

    resetRecorders();
    const VulkanTextureRef texture = createTexture(desc);
    ASSERT_TRUE(texture->isValid());

    EXPECT_TRUE(texture->isDepthOrStencilAspect());
    EXPECT_NE(texture->getFullAspectMask() & VK_IMAGE_ASPECT_DEPTH_BIT, 0u);
    ASSERT_EQ(m_provider.uploadRecorder.clearCount, 1u);
    EXPECT_TRUE(m_provider.uploadRecorder.clears[0].isDepthStencil);
    EXPECT_FLOAT_EQ(m_provider.uploadRecorder.clears[0].depthStencil.depth, 1.0f);
}

TEST_F(VulkanTextureLiveTest, VolumeCubeArrayAndMultisampledTexturesAreCreated)
{
    resource::TextureCreateDesc volumeDesc = resource::TextureCreateDesc::create3D(
        "Volume", kTextureSize, kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm
    );
    volumeDesc.setFlags(resource::TextureCreateFlags::ShaderResource);
    const VulkanTextureRef volume = createTexture(volumeDesc);
    ASSERT_TRUE(volume->isValid());
    EXPECT_NE(volume->getDefaultView(), VK_NULL_HANDLE);

    resource::TextureCreateDesc cubeDesc = resource::TextureCreateDesc::createCube(
        "Cube", static_cast<gp::UInt32>(kTextureSize), PixelFormat::R8G8B8A8_UNorm
    );
    cubeDesc.setFlags(resource::TextureCreateFlags::ShaderResource);
    const VulkanTextureRef cube = createTexture(cubeDesc);
    ASSERT_TRUE(cube->isValid());
    EXPECT_NE(cube->getDefaultView(), VK_NULL_HANDLE);

    resource::TextureCreateDesc arrayDesc = resource::TextureCreateDesc::create2DArray(
        "Array", kTextureSize, kTextureSize, static_cast<gp::UInt16>(3u), PixelFormat::R8G8B8A8_UNorm
    );
    arrayDesc.setFlags(resource::TextureCreateFlags::ShaderResource);
    arrayDesc.setNumMips(static_cast<gp::UInt8>(3u));
    const VulkanTextureRef array = createTexture(arrayDesc);
    ASSERT_TRUE(array->isValid());
    EXPECT_NE(array->getDefaultView(), VK_NULL_HANDLE);

    resource::TextureCreateDesc msaaDesc =
        resource::TextureCreateDesc::create2D("Msaa", kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm);
    msaaDesc.setFlags(resource::TextureCreateFlags::RenderTargetable);
    msaaDesc.setNumSamples(static_cast<gp::UInt8>(4u));
    const VulkanTextureRef msaa = createTexture(msaaDesc);
    ASSERT_TRUE(msaa->isValid());
    EXPECT_NE(msaa->getDefaultView(), VK_NULL_HANDLE);
    EXPECT_NE(msaa->getImageUsageFlags() & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0u);
}

TEST_F(VulkanTextureLiveTest, CpuReadbackTextureIsBufferBacked)
{
    resetRecorders();
    const VulkanTextureRef texture = createTexture(makeReadback2D());

    ASSERT_TRUE(texture->isValid());
    EXPECT_EQ(texture->getImage(), VK_NULL_HANDLE);
    const VulkanCpuReadbackBuffer* const readback = texture->getCpuReadbackBuffer();
    ASSERT_NE(readback, nullptr);
    EXPECT_NE(readback->buffer, VK_NULL_HANDLE);
    EXPECT_EQ(readback->mipOffsets[0], 0u);
    EXPECT_EQ(readback->mipSizes[0], kMip0Bytes);
    EXPECT_EQ(texture->getNativeResource(), static_cast<void*>(readback->buffer));
    EXPECT_EQ(m_provider.uploadRecorder.totalCommandCount(), 0u);
}

TEST_F(VulkanTextureLiveTest, AliasSharesTheSourceImage)
{
    const VulkanTextureRef source = createTexture(makeSampled2D());
    const VulkanTextureRef dest = createTexture(makeSampled2D());
    ASSERT_TRUE(source->isValid());
    ASSERT_TRUE(dest->isValid());

    resource::TextureRef destRef(dest.get());
    resource::TextureRef sourceRef(source.get());
    aliasVulkanTextureResources(destRef, sourceRef);

    EXPECT_EQ(dest->getImage(), source->getImage());
    EXPECT_EQ(dest->getOwnerType(), VulkanImageOwnerType::Aliased);
    EXPECT_NE(dest->getDefaultView(), VK_NULL_HANDLE);
    EXPECT_NE(dest->getDefaultView(), source->getDefaultView());
}

TEST_F(VulkanTextureLiveTest, DefaultInitializerReturnsTextureWithoutWritableData)
{
    resource::TextureInitializer initializer = createVulkanTextureInitializer(m_services, cmdList(), makeSampled2D());
    ASSERT_TRUE(initializer.isValid());
    EXPECT_FALSE(initializer.getTexture2DSubresource(0u).isValid());

    const resource::TextureRef texture = initializer.finalize();
    ASSERT_NE(texture.get(), nullptr);
    EXPECT_NE(asVulkan(texture)->getImage(), VK_NULL_HANDLE);
    EXPECT_EQ(m_provider.uploadRecorder.bufferToImageCount, 0u);
}

TEST_F(VulkanTextureLiveTest, BulkDataIsUploadedAndDiscarded)
{
    TrackingBulkData bulkData{};
    resource::TextureCreateDesc desc = makeSampled2D();
    desc.setInitActionBulkData(&bulkData);

    resetRecorders();
    resource::TextureInitializer initializer = createVulkanTextureInitializer(m_services, cmdList(), desc);
    const resource::TextureRef texture = initializer.finalize();
    ASSERT_NE(texture.get(), nullptr);
    EXPECT_TRUE(bulkData.isDiscarded);

    const FakeTransferRecorder& upload = m_provider.uploadRecorder;
    ASSERT_EQ(upload.bufferToImageCount, 1u);
    EXPECT_EQ(upload.bufferToImageCopies[0].image, asVulkan(texture)->getImage());
    EXPECT_EQ(upload.bufferToImageCopies[0].layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(upload.bufferToImageCopies[0].region.imageExtent.width, static_cast<gp::UInt32>(kTextureSize));
    EXPECT_EQ(upload.bufferToImageCopies[0].region.imageExtent.height, static_cast<gp::UInt32>(kTextureSize));

    ASSERT_GE(upload.transitionCount, 2u);
    EXPECT_EQ(upload.transitions[0].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(upload.transitions[upload.transitionCount - 1u].newLayout, asVulkan(texture)->getDefaultLayout());
}

TEST_F(VulkanTextureLiveTest, InitializerExposesEveryMipAndUploadsOnFinalize)
{
    resource::TextureCreateDesc desc = makeSampled2D();
    desc.setNumMips(static_cast<gp::UInt8>(3u));
    desc.setInitActionInitializer();

    resetRecorders();
    resource::TextureInitializer initializer = createVulkanTextureInitializer(m_services, cmdList(), desc);
    ASSERT_TRUE(initializer.isValid());

    const resource::TextureSubresourceInitializer mip0 = initializer.getTexture2DSubresource(0u);
    const resource::TextureSubresourceInitializer mip1 = initializer.getTexture2DSubresource(1u);
    const resource::TextureSubresourceInitializer mip2 = initializer.getTexture2DSubresource(2u);
    ASSERT_TRUE(mip0.isValid());
    ASSERT_TRUE(mip1.isValid());
    ASSERT_TRUE(mip2.isValid());
    EXPECT_EQ(mip0.size, static_cast<gp::UInt64>(kMip0Bytes));
    EXPECT_EQ(mip0.stride, static_cast<gp::UInt64>(kTextureSize) * kTexelBytes);
    EXPECT_EQ(mip1.size, 16u);
    EXPECT_EQ(mip2.size, 4u);
    EXPECT_EQ(m_provider.uploadRecorder.bufferToImageCount, 0u);

    const resource::TextureRef texture = initializer.finalize();
    ASSERT_NE(texture.get(), nullptr);
    EXPECT_EQ(m_provider.uploadRecorder.bufferToImageCount, 1u);
}

TEST_F(VulkanTextureLiveTest, WriteLockRecordsCopyOnUnlock)
{
    const VulkanTextureRef texture = createTexture(makeSampled2D());
    ASSERT_TRUE(texture->isValid());
    resetRecorders();

    const resource::TextureLockArgs args =
        resource::TextureLockArgs::lock2D(texture.get(), 0u, resource::ResourceLockMode::WriteOnly);
    const resource::TextureLockResult result = lockVulkanTexture(cmdList(), args);
    ASSERT_TRUE(result.isValid());
    EXPECT_EQ(result.byteCount, static_cast<gp::UInt64>(kMip0Bytes));
    EXPECT_EQ(result.stride, static_cast<gp::UInt32>(kTextureSize) * kTexelBytes);
    EXPECT_EQ(m_provider.commandListRecorder.bufferToImageCount, 0u);

    unlockVulkanTexture(cmdList(), args);

    const FakeTransferRecorder& recorder = m_provider.commandListRecorder;
    ASSERT_EQ(recorder.bufferToImageCount, 1u);
    EXPECT_EQ(recorder.bufferToImageCopies[0].image, texture->getImage());
    ASSERT_EQ(recorder.transitionCount, 2u);
    EXPECT_EQ(recorder.transitions[0].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(recorder.transitions[1].oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(recorder.transitions[1].newLayout, recorder.transitions[0].oldLayout);
    EXPECT_EQ(m_provider.submitCount, 0u);
}

TEST_F(VulkanTextureLiveTest, ReadLockCopiesBackAndFlushes)
{
    const VulkanTextureRef texture = createTexture(makeSampled2D());
    ASSERT_TRUE(texture->isValid());
    resetRecorders();

    const resource::TextureLockArgs args =
        resource::TextureLockArgs::lock2D(texture.get(), 0u, resource::ResourceLockMode::ReadOnly);
    const resource::TextureLockResult result = lockVulkanTexture(cmdList(), args);
    ASSERT_TRUE(result.isValid());

    const FakeTransferRecorder& recorder = m_provider.commandListRecorder;
    EXPECT_EQ(recorder.imageToBufferCount, 1u);
    EXPECT_EQ(recorder.barrierCount, 1u);
    EXPECT_EQ(m_provider.submitCount, 1u);

    unlockVulkanTexture(cmdList(), args);
    EXPECT_EQ(recorder.bufferToImageCount, 0u);
}

TEST_F(VulkanTextureLiveTest, UpdateTexture2DRecordsRegionCopy)
{
    const VulkanTextureRef texture = createTexture(makeSampled2D());
    ASSERT_TRUE(texture->isValid());
    resetRecorders();

    gp::UInt8 source[kMip0Bytes]{};
    const resource::UpdateTextureRegion2D region(1u, 2u, 0, 0, 2u, 2u);
    updateVulkanTexture2D(
        cmdList(), texture.get(), 0u, region, static_cast<gp::UInt32>(kTextureSize) * kTexelBytes, source
    );

    const FakeTransferRecorder& recorder = m_provider.commandListRecorder;
    ASSERT_EQ(recorder.bufferToImageCount, 1u);
    const VkBufferImageCopy& copy = recorder.bufferToImageCopies[0].region;
    EXPECT_EQ(copy.imageOffset.x, 1);
    EXPECT_EQ(copy.imageOffset.y, 2);
    EXPECT_EQ(copy.imageExtent.width, 2u);
    EXPECT_EQ(copy.imageExtent.height, 2u);
    EXPECT_EQ(copy.imageExtent.depth, 1u);
}

TEST_F(VulkanTextureLiveTest, UpdateTexture3DRecordsBoxCopy)
{
    resource::TextureCreateDesc desc = resource::TextureCreateDesc::create3D(
        "Volume", kTextureSize, kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm
    );
    desc.setFlags(resource::TextureCreateFlags::ShaderResource);
    const VulkanTextureRef texture = createTexture(desc);
    ASSERT_TRUE(texture->isValid());
    resetRecorders();

    gp::UInt8 source[kMip0Bytes * static_cast<gp::UInt32>(kTextureSize)]{};
    const resource::UpdateTextureRegion3D region(0u, 0u, 1u, 0, 0, 0, 4u, 4u, 2u);
    updateVulkanTexture3D(
        cmdList(), texture.get(), 0u, region, static_cast<gp::UInt32>(kTextureSize) * kTexelBytes, kMip0Bytes, source
    );

    const FakeTransferRecorder& recorder = m_provider.commandListRecorder;
    ASSERT_EQ(recorder.bufferToImageCount, 1u);
    const VkBufferImageCopy& copy = recorder.bufferToImageCopies[0].region;
    EXPECT_EQ(copy.imageOffset.z, 1);
    EXPECT_EQ(copy.imageExtent.depth, 2u);
}

TEST_F(VulkanTextureLiveTest, GpuReadbackRecordsCopyAndFlushes)
{
    const VulkanTextureRef texture = createTexture(makeSampled2D());
    ASSERT_TRUE(texture->isValid());
    resetRecorders();

    gp::Vector<gp::math::Color> pixels;
    const resource::IntRect rect(resource::IntPoint(1, 1), resource::IntPoint(3, 4));
    ASSERT_TRUE(readVulkanSurfaceData(cmdList(), texture.get(), rect, pixels, resource::ReadSurfaceDataFlags{}));
    EXPECT_EQ(pixels.size(), 6u);

    const FakeTransferRecorder& recorder = m_provider.commandListRecorder;
    ASSERT_EQ(recorder.imageToBufferCount, 1u);
    EXPECT_EQ(recorder.imageToBufferCopies[0].image, texture->getImage());
    EXPECT_EQ(recorder.imageToBufferCopies[0].layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(recorder.imageToBufferCopies[0].region.imageOffset.x, 1);
    EXPECT_EQ(recorder.imageToBufferCopies[0].region.imageOffset.y, 1);
    EXPECT_EQ(recorder.imageToBufferCopies[0].region.imageExtent.width, 2u);
    EXPECT_EQ(recorder.imageToBufferCopies[0].region.imageExtent.height, 3u);
    ASSERT_EQ(recorder.barrierCount, 1u);
    EXPECT_EQ(recorder.barriers[0].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_HOST_BIT));
    EXPECT_EQ(m_provider.submitCount, 1u);
}

TEST_F(VulkanTextureLiveTest, ReadbackRejectsEmptyRectsAndMultisampledTextures)
{
    const VulkanTextureRef texture = createTexture(makeSampled2D());
    ASSERT_TRUE(texture->isValid());

    gp::Vector<gp::math::Color> pixels;
    const resource::IntRect emptyRect(resource::IntPoint(2, 2), resource::IntPoint(2, 3));
    EXPECT_FALSE(readVulkanSurfaceData(cmdList(), texture.get(), emptyRect, pixels, resource::ReadSurfaceDataFlags{}));
    EXPECT_EQ(pixels.size(), 0u);

    resource::TextureCreateDesc msaaDesc =
        resource::TextureCreateDesc::create2D("Msaa", kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm);
    msaaDesc.setFlags(resource::TextureCreateFlags::RenderTargetable);
    msaaDesc.setNumSamples(static_cast<gp::UInt8>(4u));
    const VulkanTextureRef msaa = createTexture(msaaDesc);
    ASSERT_TRUE(msaa->isValid());

    const resource::IntRect fullRect(resource::IntPoint(0, 0), resource::IntPoint(kTextureSize, kTextureSize));
    EXPECT_FALSE(readVulkanSurfaceData(cmdList(), msaa.get(), fullRect, pixels, resource::ReadSurfaceDataFlags{}));
}

TEST_F(VulkanTextureLiveTest, CpuReadbackTextureReturnsWrittenTexels)
{
    const VulkanTextureRef texture = createTexture(makeReadback2D());
    ASSERT_TRUE(texture->isValid());

    void* mapped = nullptr;
    gp::Int32 width = 0;
    gp::Int32 height = 0;
    mapVulkanStagingSurface(texture.get(), mapped, width, height);
    ASSERT_NE(mapped, nullptr);
    EXPECT_EQ(width, kTextureSize);
    EXPECT_EQ(height, kTextureSize);

    gp::UInt8* const bytes = static_cast<gp::UInt8*>(mapped);
    for (gp::UInt32 index = 0u; index < kMip0Bytes; ++index)
    {
        bytes[index] = static_cast<gp::UInt8>(index * 3u);
    }
    unmapVulkanStagingSurface(texture.get());

    resetRecorders();
    gp::Vector<gp::math::Color> pixels;
    const resource::IntRect rect(resource::IntPoint(1, 2), resource::IntPoint(3, 3));
    ASSERT_TRUE(readVulkanSurfaceData(cmdList(), texture.get(), rect, pixels, resource::ReadSurfaceDataFlags{}));
    ASSERT_EQ(pixels.size(), 2u);
    EXPECT_EQ(m_provider.commandListRecorder.totalCommandCount(), 0u);

    // Texel (1, 2) starts at byte (2 * 4 + 1) * 4 = 36.
    EXPECT_EQ(pixels[0].r, static_cast<gp::UInt8>(36u * 3u));
    EXPECT_EQ(pixels[0].g, static_cast<gp::UInt8>(37u * 3u));
    EXPECT_EQ(pixels[0].b, static_cast<gp::UInt8>(38u * 3u));
    EXPECT_EQ(pixels[0].a, static_cast<gp::UInt8>(39u * 3u));
    EXPECT_EQ(pixels[1].r, static_cast<gp::UInt8>(40u * 3u));

    gp::Vector<gp::math::LinearColor> linearPixels;
    ASSERT_TRUE(readVulkanSurfaceData(cmdList(), texture.get(), rect, linearPixels, resource::ReadSurfaceDataFlags{}));
    ASSERT_EQ(linearPixels.size(), 2u);
    EXPECT_NEAR(linearPixels[0].r, static_cast<gp::Float32>(36u * 3u) / 255.0f, 1e-5f);
}

TEST_F(VulkanTextureLiveTest, FloatReadbackRequiresAFloatFormat)
{
    resource::TextureCreateDesc desc =
        resource::TextureCreateDesc::create2D("Half", kTextureSize, kTextureSize, PixelFormat::R16G16B16A16_Float);
    desc.setFlags(resource::TextureCreateFlags::ShaderResource);
    const VulkanTextureRef halfTexture = createTexture(desc);
    ASSERT_TRUE(halfTexture->isValid());

    gp::Vector<resource::Float16Color> pixels;
    const resource::IntRect rect(resource::IntPoint(0, 0), resource::IntPoint(2, 2));
    EXPECT_TRUE(readVulkanSurfaceFloatData(cmdList(), halfTexture.get(), rect, pixels, resource::CubeFace::PosX, 0, 0));
    EXPECT_EQ(pixels.size(), 4u);

    const VulkanTextureRef byteTexture = createTexture(makeSampled2D());
    ASSERT_TRUE(byteTexture->isValid());
    EXPECT_FALSE(
        readVulkanSurfaceFloatData(cmdList(), byteTexture.get(), rect, pixels, resource::CubeFace::PosX, 0, 0)
    );
    EXPECT_EQ(pixels.size(), 0u);
}

TEST_F(VulkanTextureLiveTest, VolumeFloatReadbackCopiesTheSliceRange)
{
    resource::TextureCreateDesc desc = resource::TextureCreateDesc::create3D(
        "HalfVolume", kTextureSize, kTextureSize, kTextureSize, PixelFormat::R16G16B16A16_Float
    );
    desc.setFlags(resource::TextureCreateFlags::ShaderResource);
    const VulkanTextureRef texture = createTexture(desc);
    ASSERT_TRUE(texture->isValid());
    resetRecorders();

    gp::Vector<resource::Float16Color> pixels;
    const resource::IntRect rect(resource::IntPoint(0, 0), resource::IntPoint(2, 2));
    ASSERT_TRUE(read3DVulkanSurfaceFloatData(cmdList(), texture.get(), rect, resource::IntPoint(1, 3), pixels));
    EXPECT_EQ(pixels.size(), 8u);

    const FakeTransferRecorder& recorder = m_provider.commandListRecorder;
    ASSERT_EQ(recorder.imageToBufferCount, 1u);
    EXPECT_EQ(recorder.imageToBufferCopies[0].region.imageOffset.z, 1);
    EXPECT_EQ(recorder.imageToBufferCopies[0].region.imageExtent.depth, 2u);

    const VulkanTextureRef flat = createTexture(makeSampled2D());
    EXPECT_FALSE(read3DVulkanSurfaceFloatData(cmdList(), flat.get(), rect, resource::IntPoint(0, 1), pixels));
}

TEST_F(VulkanTextureLiveTest, MapStagingSurfaceRejectsImageTextures)
{
    const VulkanTextureRef texture = createTexture(makeSampled2D());
    ASSERT_TRUE(texture->isValid());

    void* mapped = reinterpret_cast<void*>(static_cast<gp::UIntPtr>(1u));
    gp::Int32 width = -1;
    gp::Int32 height = -1;
    mapVulkanStagingSurface(texture.get(), mapped, width, height);
    EXPECT_EQ(mapped, nullptr);
}

TEST_F(VulkanTextureLiveTest, TextureReferenceFollowsUpdates)
{
    const VulkanTextureRef first = createTexture(makeSampled2D());
    const VulkanTextureRef second = createTexture(makeSampled2D());

    const resource::TextureReferenceRef reference = createVulkanTextureReference(cmdList(), first.get());
    ASSERT_NE(reference.get(), nullptr);
    EXPECT_EQ(reference->getReferencedTexture(), first.get());

    updateVulkanTextureReference(cmdList(), reference.get(), second.get());
    EXPECT_EQ(reference->getReferencedTexture(), second.get());

    // A null reference is ignored.
    updateVulkanTextureReference(cmdList(), nullptr, first.get());
    EXPECT_EQ(reference->getReferencedTexture(), second.get());
}

TEST_F(VulkanTextureLiveTest, PlatformSizeMatchesTheCreatedImage)
{
    const resource::TextureCreateDesc desc = makeSampled2D();
    const resource::CalcTextureSizeResult result = calcVulkanTexturePlatformSize(m_services, desc, 0u);
    EXPECT_GE(result.size, static_cast<gp::UInt64>(kMip0Bytes));
    EXPECT_GT(result.align, 0u);

    const resource::CalcTextureSizeResult cached = calcVulkanTexturePlatformSize(m_services, desc, 0u);
    EXPECT_EQ(cached, result);

    const VulkanTextureRef texture = createTexture(desc);
    ASSERT_TRUE(texture->isValid());
    EXPECT_EQ(computeVulkanTextureMemorySize(texture.get()), result.size);
    EXPECT_EQ(computeVulkanTextureMemorySize(nullptr), 0u);
}

TEST_F(VulkanTextureLiveTest, PlatformSizeRejectsInvalidDescriptors)
{
    resource::TextureCreateDesc desc = makeSampled2D();
    const resource::CalcTextureSizeResult outOfRange = calcVulkanTexturePlatformSize(m_services, desc, 1u);
    EXPECT_EQ(outOfRange.size, 0u);

    desc.setExtent(0, 4);
    const resource::CalcTextureSizeResult empty = calcVulkanTexturePlatformSize(m_services, desc, 0u);
    EXPECT_EQ(empty.size, 0u);
}

TEST_F(VulkanTextureLiveTest, PlatformSizeShrinksWithFirstMip)
{
    resource::TextureCreateDesc desc =
        resource::TextureCreateDesc::create2D("Mipped", 256, 256, PixelFormat::R8G8B8A8_UNorm);
    desc.setFlags(resource::TextureCreateFlags::ShaderResource);
    desc.setNumMips(static_cast<gp::UInt8>(9u));

    const resource::CalcTextureSizeResult full = calcVulkanTexturePlatformSize(m_services, desc, 0u);
    const resource::CalcTextureSizeResult streamed = calcVulkanTexturePlatformSize(m_services, desc, 2u);
    ASSERT_GT(full.size, 0u);
    ASSERT_GT(streamed.size, 0u);
    EXPECT_LT(streamed.size, full.size);
}

TEST_F(VulkanTextureLiveTest, MemoryStatsReportDeviceMemory)
{
    resource::TextureMemoryStats stats{};
    fillVulkanTextureMemoryStats(m_services, stats);

    EXPECT_GT(stats.dedicatedVideoMemory, 0);
    EXPECT_EQ(stats.totalGraphicsMemory, stats.dedicatedVideoMemory);
    EXPECT_EQ(stats.sharedSystemMemory, -1);
    EXPECT_GE(stats.nonStreamingMemorySize, 0);
    if (isUnifiedMemory())
    {
        EXPECT_EQ(stats.dedicatedSystemMemory, 0);
    }

    const gp::Int64 bytesBefore = getVulkanTextureMemoryStats().totalTextureBytes;
    {
        const VulkanTextureRef texture = createTexture(makeSampled2D());
        ASSERT_TRUE(texture->isValid());
        fillVulkanTextureMemoryStats(m_services, stats);
        EXPECT_GT(stats.nonStreamingMemorySize, bytesBefore);
    }
}

TEST(VulkanTextureSizeTest, InvalidServicesGiveZeroes)
{
    const VulkanResourceServices services{};
    const resource::CalcTextureSizeResult result = calcVulkanTexturePlatformSize(
        services, resource::TextureCreateDesc::create2D("NoDevice", 4, 4, PixelFormat::R8G8B8A8_UNorm), 0u
    );
    EXPECT_EQ(result.size, 0u);
    EXPECT_EQ(result.align, 0u);

    resource::TextureMemoryStats stats{};
    stats.dedicatedVideoMemory = 42;
    fillVulkanTextureMemoryStats(services, stats);
    EXPECT_EQ(stats.dedicatedVideoMemory, 0);
}

}   // namespace gp::rhi::vulkan::tests
