// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanPipelineBarrier.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanQueue.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/VulkanFence.hpp"
#include "translation/VulkanResourceEnums.hpp"   // IWYU pragma: keep
#include "VulkanThirdParty.hpp"                  // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

VulkanBarrierResource makeColorTexture(const VkImage image, const bool supportsSampling = true)
{
    VulkanBarrierResource tex{};
    tex.type = VulkanBarrierResourceType::Texture;
    tex.image = image;
    tex.fullAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    tex.isDepthOrStencilAspect = false;
    tex.supportsSampling = supportsSampling;
    tex.numMips = 1u;
    tex.numArrayLayers = 1u;
    return tex;
}

VulkanBarrierResource makeDepthStencilTexture(const VkImage image)
{
    VulkanBarrierResource tex{};
    tex.type = VulkanBarrierResourceType::Texture;
    tex.image = image;
    tex.fullAspectMask = static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    tex.isDepthOrStencilAspect = true;
    tex.supportsSampling = false;
    tex.numMips = 1u;
    tex.numArrayLayers = 1u;
    return tex;
}

}   // namespace

// isEmpty tests

TEST(VulkanPipelineBarrierTest, EmptyBarrierBatch)
{
    const VulkanPipelineBarrier barrier{};

    EXPECT_TRUE(barrier.isEmpty());
    EXPECT_TRUE(barrier.memoryBarriers.isEmpty());
    EXPECT_TRUE(barrier.imageBarriers.isEmpty());
    EXPECT_TRUE(barrier.bufferBarriers.isEmpty());
}

TEST(VulkanPipelineBarrierTest, NotEmptyAfterMemoryBarrier)
{
    VulkanPipelineBarrier barrier{};
    barrier.addMemoryBarrier(0u, 0u, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    EXPECT_FALSE(barrier.isEmpty());
}

TEST(VulkanPipelineBarrierTest, NotEmptyAfterImageLayoutTransition)
{
    VulkanPipelineBarrier barrier{};
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x1000u));
    const VkImageSubresourceRange range = VulkanPipelineBarrier::makeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    barrier.addImageLayoutTransition(fakeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, range);

    EXPECT_FALSE(barrier.isEmpty());
}

// makeSubresourceRange tests

TEST(VulkanPipelineBarrierTest, MakeSubresourceRangeDefaults)
{
    const VkImageSubresourceRange range = VulkanPipelineBarrier::makeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    EXPECT_EQ(range.aspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    EXPECT_EQ(range.baseMipLevel, 0u);
    EXPECT_EQ(range.levelCount, VK_REMAINING_MIP_LEVELS);
    EXPECT_EQ(range.baseArrayLayer, 0u);
    EXPECT_EQ(range.layerCount, VK_REMAINING_ARRAY_LAYERS);
}

TEST(VulkanPipelineBarrierTest, MakeSubresourceRangeExplicit)
{
    const VkImageAspectFlags depthStencilAspect =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    const VkImageSubresourceRange range =
        VulkanPipelineBarrier::makeSubresourceRange(depthStencilAspect, 1u, 3u, 2u, 4u);

    EXPECT_EQ(range.aspectMask, depthStencilAspect);
    EXPECT_EQ(range.baseMipLevel, 1u);
    EXPECT_EQ(range.levelCount, 3u);
    EXPECT_EQ(range.baseArrayLayer, 2u);
    EXPECT_EQ(range.layerCount, 4u);
}

// addMemoryBarrier tests

TEST(VulkanPipelineBarrierTest, AddMemoryBarrierFirstCallCreatesSingleBarrierWithCorrectSType)
{
    VulkanPipelineBarrier barrier{};
    barrier.addMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    EXPECT_EQ(barrier.memoryBarriers.size(), 1u);
    EXPECT_EQ(barrier.memoryBarriers[0].sType, VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
}

TEST(VulkanPipelineBarrierTest, AddMemoryBarrierSecondCallMergesIntoSingleBarrier)
{
    VulkanPipelineBarrier barrier{};
    barrier.addMemoryBarrier(0u, 0u, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    barrier.addMemoryBarrier(0u, 0u, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Always at most one memory barrier; both calls fold into it.
    EXPECT_EQ(barrier.memoryBarriers.size(), 1u);
    EXPECT_NE(
        barrier.memoryBarriers[0].srcStageMask &
            static_cast<VkPipelineStageFlags2KHR>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT),
        static_cast<VkPipelineStageFlags2KHR>(0u)
    );
    EXPECT_NE(
        barrier.memoryBarriers[0].srcStageMask &
            static_cast<VkPipelineStageFlags2KHR>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),
        static_cast<VkPipelineStageFlags2KHR>(0u)
    );
}

TEST(VulkanPipelineBarrierTest, AddMemoryBarrierReadOnlySourceKeepsAccessMasksZero)
{
    VulkanPipelineBarrier barrier{};
    // VK_ACCESS_SHADER_READ_BIT is a read-only access; source has no writes to make visible.
    barrier.addMemoryBarrier(
        VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    ASSERT_EQ(barrier.memoryBarriers.size(), 1u);
    // Access masks should stay zero; only stage masks are set.
    EXPECT_EQ(barrier.memoryBarriers[0].srcAccessMask, static_cast<VkAccessFlags2KHR>(0u));
    EXPECT_EQ(barrier.memoryBarriers[0].dstAccessMask, static_cast<VkAccessFlags2KHR>(0u));
    // Stage masks still accumulate.
    EXPECT_NE(
        barrier.memoryBarriers[0].srcStageMask &
            static_cast<VkPipelineStageFlags2KHR>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT),
        static_cast<VkPipelineStageFlags2KHR>(0u)
    );
}

TEST(VulkanPipelineBarrierTest, AddMemoryBarrierWriteSourceAddsAccessMasks)
{
    VulkanPipelineBarrier barrier{};
    // VK_ACCESS_SHADER_WRITE_BIT is a write access; source must make memory visible.
    barrier.addMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    ASSERT_EQ(barrier.memoryBarriers.size(), 1u);
    EXPECT_NE(barrier.memoryBarriers[0].srcAccessMask, static_cast<VkAccessFlags2KHR>(0u));
    EXPECT_NE(barrier.memoryBarriers[0].dstAccessMask, static_cast<VkAccessFlags2KHR>(0u));
}

TEST(VulkanPipelineBarrierTest, AddMemoryBarrierStageMasksAccumulateAcrossMultipleCalls)
{
    VulkanPipelineBarrier barrier{};

    // First call with read-only source: stage masks set, access masks stay zero.
    barrier.addMemoryBarrier(
        VK_ACCESS_SHADER_READ_BIT, 0u, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );
    const VkPipelineStageFlags2KHR srcAfterFirst = barrier.memoryBarriers[0].srcStageMask;

    // Second call with write source: access masks are added, stage masks accumulate.
    barrier.addMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );

    EXPECT_EQ(barrier.memoryBarriers.size(), 1u);
    EXPECT_NE(barrier.memoryBarriers[0].srcStageMask, srcAfterFirst);
    EXPECT_NE(
        barrier.memoryBarriers[0].srcStageMask &
            static_cast<VkPipelineStageFlags2KHR>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),
        static_cast<VkPipelineStageFlags2KHR>(0u)
    );
}

// addFullImageLayoutTransition tests

TEST(VulkanPipelineBarrierTest, AddFullImageLayoutTransitionColorTextureStoresLayouts)
{
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x2000u));
    VulkanPipelineBarrier barrier{};
    const VulkanBarrierResource tex = makeColorTexture(fakeImage);

    barrier.addFullImageLayoutTransition(tex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ASSERT_EQ(barrier.imageBarriers.size(), 1u);
    EXPECT_EQ(barrier.imageBarriers[0].image, fakeImage);
    EXPECT_EQ(barrier.imageBarriers[0].oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(barrier.imageBarriers[0].newLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(
        barrier.imageBarriers[0].subresourceRange.aspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT)
    );
    EXPECT_EQ(barrier.imageBarriers[0].subresourceRange.levelCount, VK_REMAINING_MIP_LEVELS);
    EXPECT_EQ(barrier.imageBarriers[0].subresourceRange.layerCount, VK_REMAINING_ARRAY_LAYERS);
}

TEST(VulkanPipelineBarrierTest, AddFullImageLayoutTransitionDepthStencilMergesAttachmentOptimal)
{
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x3000u));
    VulkanPipelineBarrier barrier{};
    const VulkanBarrierResource tex = makeDepthStencilTexture(fakeImage);

    // Per-plane ATTACHMENT_OPTIMAL should merge to DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
    barrier.addFullImageLayoutTransition(tex, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);

    ASSERT_EQ(barrier.imageBarriers.size(), 1u);
    EXPECT_EQ(barrier.imageBarriers[0].oldLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barrier.imageBarriers[0].newLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

TEST(VulkanPipelineBarrierTest, AddFullImageLayoutTransitionDepthStencilMergesReadOnlyOptimal)
{
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x4000u));
    VulkanPipelineBarrier barrier{};
    const VulkanBarrierResource tex = makeDepthStencilTexture(fakeImage);

    // Per-plane READ_ONLY_OPTIMAL merges to DEPTH_STENCIL_READ_ONLY_OPTIMAL.
    barrier.addFullImageLayoutTransition(tex, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    ASSERT_EQ(barrier.imageBarriers.size(), 1u);
    EXPECT_EQ(barrier.imageBarriers[0].oldLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barrier.imageBarriers[0].newLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

// addImageLayoutTransition tests

TEST(VulkanPipelineBarrierTest, AddImageLayoutTransitionStoresAllFields)
{
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x5000u));
    VulkanPipelineBarrier barrier{};

    const VkImageSubresourceRange range =
        VulkanPipelineBarrier::makeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 1u, 2u, 0u, 3u);
    barrier.addImageLayoutTransition(
        fakeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range
    );

    ASSERT_EQ(barrier.imageBarriers.size(), 1u);
    EXPECT_EQ(barrier.imageBarriers[0].image, fakeImage);
    EXPECT_EQ(barrier.imageBarriers[0].oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(barrier.imageBarriers[0].newLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(
        barrier.imageBarriers[0].subresourceRange.aspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT)
    );
    EXPECT_EQ(barrier.imageBarriers[0].subresourceRange.baseMipLevel, 1u);
    EXPECT_EQ(barrier.imageBarriers[0].subresourceRange.levelCount, 2u);
    EXPECT_EQ(barrier.imageBarriers[0].subresourceRange.layerCount, 3u);
}

TEST(VulkanPipelineBarrierTest, AddImageLayoutTransitionDepthAspectDeterminesDepthPath)
{
    const VkImage fakeImageColor = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x6000u));
    const VkImage fakeImageDepth = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x7000u));
    VulkanPipelineBarrier colorBarrier{};
    VulkanPipelineBarrier depthBarrier{};

    const VkImageSubresourceRange colorRange = VulkanPipelineBarrier::makeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    const VkImageSubresourceRange depthRange = VulkanPipelineBarrier::makeSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT);

    colorBarrier.addImageLayoutTransition(
        fakeImageColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, colorRange
    );
    depthBarrier.addImageLayoutTransition(
        fakeImageDepth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, depthRange
    );

    ASSERT_EQ(colorBarrier.imageBarriers.size(), 1u);
    ASSERT_EQ(depthBarrier.imageBarriers.size(), 1u);

    // addImageLayoutTransition stores the layouts as passed, without merging them.
    EXPECT_EQ(colorBarrier.imageBarriers[0].newLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(depthBarrier.imageBarriers[0].newLayout, VK_IMAGE_LAYOUT_GENERAL);

    // Aspect mask is preserved verbatim on both paths.
    EXPECT_EQ(
        colorBarrier.imageBarriers[0].subresourceRange.aspectMask,
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT)
    );
    EXPECT_EQ(
        depthBarrier.imageBarriers[0].subresourceRange.aspectMask,
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT)
    );

    // Both image handles are captured correctly.
    EXPECT_EQ(colorBarrier.imageBarriers[0].image, fakeImageColor);
    EXPECT_EQ(depthBarrier.imageBarriers[0].image, fakeImageDepth);
}

// getDefaultLayout tests

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutSRVColorSampling)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE, true);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::SRVGraphicsPixel, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutSRVColorNonSampling)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE, false);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::SRVCompute, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_GENERAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutSRVDepthParallelRendering)
{
    const VulkanBarrierResource depthTex = makeDepthStencilTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            depthTex, sync::ResourceAccess::SRVGraphicsNonPixel, true, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutSRVDepthNoParallelRendering)
{
    const VulkanBarrierResource depthTex = makeDepthStencilTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            depthTex, sync::ResourceAccess::SRVMask, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutUAVVariantsReturnGeneral)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::UAVCompute, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_GENERAL
    );
    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::UAVGraphics, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_GENERAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutCopySrc)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::CopySrc, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutCopyDest)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::CopyDest, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutDSVRead)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::DSVRead, true, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutDSVWrite)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::DSVWrite, false, VulkanShadingRateImageType::NotSupported
        ),
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutShadingRatePalette)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::ShadingRateSource, false, VulkanShadingRateImageType::Palette
        ),
        VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR
    );
}

TEST(VulkanPipelineBarrierTest, GetDefaultLayoutShadingRateFractional)
{
    const VulkanBarrierResource colorTex = makeColorTexture(VK_NULL_HANDLE);

    EXPECT_EQ(
        VulkanPipelineBarrier::getDefaultLayout(
            colorTex, sync::ResourceAccess::ShadingRateSource, false, VulkanShadingRateImageType::Fractional
        ),
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
    );
}

// getDepthOrStencilLayout tests

TEST(VulkanPipelineBarrierTest, GetDepthOrStencilLayoutVariants)
{
    EXPECT_EQ(
        VulkanPipelineBarrier::getDepthOrStencilLayout(sync::ResourceAccess::DSVWrite),
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        VulkanPipelineBarrier::getDepthOrStencilLayout(sync::ResourceAccess::DSVRead), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        VulkanPipelineBarrier::getDepthOrStencilLayout(sync::ResourceAccess::CopySrc),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );
    EXPECT_EQ(
        VulkanPipelineBarrier::getDepthOrStencilLayout(sync::ResourceAccess::CopyDest),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    EXPECT_EQ(VulkanPipelineBarrier::getDepthOrStencilLayout(sync::ResourceAccess::Unknown), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(VulkanPipelineBarrier::getDepthOrStencilLayout(sync::ResourceAccess::Discard), VK_IMAGE_LAYOUT_UNDEFINED);
    // SRVGraphics falls into the else branch -> READ_ONLY_OPTIMAL.
    EXPECT_EQ(
        VulkanPipelineBarrier::getDepthOrStencilLayout(sync::ResourceAccess::SRVGraphicsPixel),
        VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );
}

// execute with empty batch tests (no live device required)

TEST(VulkanPipelineBarrierTest, ExecuteEmptyBatchOnNullVkCommandBufferIsNoOp)
{
    VulkanPipelineBarrier barrier{};
    EXPECT_TRUE(barrier.isEmpty());

    // The function returns early when empty; passing VK_NULL_HANDLE is safe.
    barrier.execute(VK_NULL_HANDLE);
}

// Live tests

class VulkanBarrierLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader library unavailable; skipping live barrier test.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-barriers-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping live barrier test.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No Vulkan physical device found; skipping live barrier test.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping live barrier test.";
        }

        m_fenceManager = new VulkanFenceManager(*m_device);
        const gp::Int32 graphicsFamily = m_device->getQueueFamilyIndices().graphicsFamily;
        ASSERT_GE(graphicsFamily, 0);
        m_queue = new VulkanQueue(
            *m_device, *m_fenceManager, static_cast<gp::UInt32>(graphicsFamily), VulkanQueueType::Graphics, 0u
        );
        ASSERT_TRUE(m_queue->isValid());
    }

    virtual void TearDown() override
    {
        delete m_queue;
        m_queue = nullptr;

        if (m_fenceManager != nullptr)
        {
            m_fenceManager->deinit();
            delete m_fenceManager;
            m_fenceManager = nullptr;
        }

        if (m_device != nullptr)
        {
            m_device->destroy();
            delete m_device;
            m_device = nullptr;
        }

        m_instance.shutdown();
        if (m_loaderLoaded)
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
        }
    }

    bool m_loaderLoaded{ false };
    VulkanInstance m_instance{};
    VulkanDevice* m_device{ nullptr };
    VulkanFenceManager* m_fenceManager{ nullptr };
    VulkanQueue* m_queue{ nullptr };
};

TEST_F(VulkanBarrierLiveTest, ExecuteEmptyBatchOnCommandBufferReturnsEarly)
{
    VulkanCommandBufferPool* const pool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    ASSERT_NE(pool, nullptr);

    VulkanCommandBuffer* const cmd = pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanPipelineBarrier barrier{};
    EXPECT_TRUE(barrier.isEmpty());

    // Both execute overloads must return without issuing any Vulkan command.
    barrier.execute(cmd->getHandle());
    barrier.execute(*cmd);

    ASSERT_TRUE(cmd->end());
    EXPECT_TRUE(cmd->hasEnded());

    cmd->reset();
    m_queue->releaseCommandBufferPool(pool);
}

TEST_F(VulkanBarrierLiveTest, ExecuteMemoryBarrierOnRawCommandBuffer)
{
    VulkanCommandBufferPool* const pool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    ASSERT_NE(pool, nullptr);

    VulkanCommandBuffer* const cmd = pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanPipelineBarrier barrier{};
    barrier.addMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
    );
    EXPECT_FALSE(barrier.isEmpty());

    barrier.execute(cmd->getHandle());

    ASSERT_TRUE(cmd->end());
    EXPECT_TRUE(cmd->hasEnded());

    cmd->reset();
    m_queue->releaseCommandBufferPool(pool);
}

TEST_F(VulkanBarrierLiveTest, ExecuteMemoryBarrierOnVulkanCommandBuffer)
{
    VulkanCommandBufferPool* const pool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    ASSERT_NE(pool, nullptr);

    VulkanCommandBuffer* const cmd = pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanPipelineBarrier barrier{};
    barrier.addMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    if (!m_device->supportsParallelRendering() || dynamic_api::vkCmdPipelineBarrier2KHR == nullptr)
    {
        GTEST_SKIP() << "Device does not support synchronization2; skipping sync2 execute path.";
    }

    barrier.execute(*cmd);

    ASSERT_TRUE(cmd->end());
    EXPECT_TRUE(cmd->hasEnded());

    cmd->reset();
    m_queue->releaseCommandBufferPool(pool);
}

}   // namespace gp::rhi::vulkan::tests
