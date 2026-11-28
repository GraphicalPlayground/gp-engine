// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierSettings.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanBarrierSettingsTest, ComputeGraphicsPipelineStageBitsAllFourCombinations)
{
    const VkPipelineStageFlags kBase =
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    // Vertex + fragment only (no geometry, no mesh)
    EXPECT_EQ(computeVulkanGraphicsPipelineStageBits(false, false), kBase);

    // Vertex + fragment + geometry
    EXPECT_EQ(
        computeVulkanGraphicsPipelineStageBits(true, false),
        static_cast<VkPipelineStageFlags>(kBase | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT)
    );

    // Vertex + fragment + task + mesh
    EXPECT_EQ(
        computeVulkanGraphicsPipelineStageBits(false, true),
        static_cast<VkPipelineStageFlags>(
            kBase | VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT
        )
    );

    // All four stage groups
    EXPECT_EQ(
        computeVulkanGraphicsPipelineStageBits(true, true),
        static_cast<VkPipelineStageFlags>(
            kBase | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
            VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT
        )
    );
}

TEST(VulkanBarrierSettingsTest, GetVulkanBarrierSettingsMatchesConsoleVariableDefaults)
{
    const VulkanBarrierSettings settings = getVulkanBarrierSettings();

    // Console variable defaults: UseMemoryBarrierOpt=1, MaxBarriersPerBatch=-1,
    // AllowSplitBarriers=1, AllowConcurrentImage=0
    EXPECT_TRUE(settings.useMemoryBarrierOpt);
    EXPECT_EQ(settings.maxBarriersPerBatch, -1);
    EXPECT_TRUE(settings.allowSplitBarriers);
    EXPECT_FALSE(settings.allowConcurrentImage);
}

class VulkanBarrierSettingsLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader library not available on this host.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc instDesc{};
        instDesc.applicationName = "VulkanBarrierSettingsTest";
        instDesc.allowRetryWithoutLayers = true;
        instDesc.allowMissingSurfaceFunctions = true;
        instDesc.enableDebugCallbacks = false;

        if (!m_instance.initialize(instDesc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No Vulkan instance available on this host.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No compatible Vulkan physical device on this host.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "Failed to initialize live VulkanDevice.";
        }
    }

    virtual void TearDown() override
    {
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
};

TEST_F(VulkanBarrierSettingsLiveTest, TranslationAndDeviceDescMirrorDeviceCapabilities)
{
    const VulkanBarrierTranslationDesc transDesc = makeVulkanBarrierTranslationDesc(*m_device);

    // Vertex and fragment are always present regardless of optional extensions.
    const VkPipelineStageFlags kMinGraphicsBits =
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(transDesc.graphicsPipelineStageBits & kMinGraphicsBits, kMinGraphicsBits);

    const VulkanBarrierDeviceDesc devDesc = makeVulkanBarrierDeviceDesc(*m_device);

    const gp::Int32 graphicsFamily = m_device->getQueueFamilyIndices().graphicsFamily;
    ASSERT_GE(graphicsFamily, 0);
    EXPECT_EQ(devDesc.graphicsQueueFamilyIndex, static_cast<gp::UInt32>(graphicsFamily));

    // When no dedicated async compute queue, compute index falls back to graphics index.
    if (!m_device->hasAsyncComputeQueue())
    {
        EXPECT_EQ(devDesc.computeQueueFamilyIndex, devDesc.graphicsQueueFamilyIndex);
    }

    EXPECT_EQ(devDesc.supportsParallelRendering, m_device->supportsParallelRendering());
    EXPECT_EQ(devDesc.hasAsyncComputeQueue, m_device->hasAsyncComputeQueue());
}

}   // namespace gp::rhi::vulkan::tests
