// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanTransitionData.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/Transition.hpp"
#include "sync/TransitionDesc.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

VulkanDevice* g_transitionDevice{ nullptr };
VulkanBarrierSettings g_transitionSettings{};

void testOnCreateVulkanTransition(
    sync::Transition* const transition, const sync::TransitionCreateInfo& createInfo
) noexcept
{
    createVulkanTransition(*g_transitionDevice, transition, createInfo, g_transitionSettings);
}

void testOnReleaseVulkanTransition(sync::Transition* const transition) noexcept
{
    releaseVulkanTransition(transition);
}

}   // namespace

// getVulkanTransitionPrivateDataSize matches sizeof(VulkanTransitionData).
TEST(VulkanTransitionDataTest, PrivateDataSizeMatchesSizeof)
{
    EXPECT_EQ(getVulkanTransitionPrivateDataSize(), sizeof(VulkanTransitionData));
}

// getVulkanTransitionPrivateDataAlign matches alignof(VulkanTransitionData).
TEST(VulkanTransitionDataTest, PrivateDataAlignMatchesAlignof)
{
    EXPECT_EQ(getVulkanTransitionPrivateDataAlign(), alignof(VulkanTransitionData));
}

// Same pipelines -> never needs a semaphore regardless of flags or ownership transfers.
TEST(VulkanTransitionDataTest, NeedsCrossQueueSemaphore_SamePipelinesAlwaysFalse)
{
    EXPECT_FALSE(needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::None, false
    ));
    EXPECT_FALSE(needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::None, true
    ));
    EXPECT_FALSE(needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::NoFence, true
    ));
}

// Different pipelines, no ownership transfers, no NoFence -> needs semaphore.
TEST(VulkanTransitionDataTest, NeedsCrossQueueSemaphore_DifferentPipelinesDefaultTrue)
{
    EXPECT_TRUE(needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics, sync::PipelineStage::AsyncCompute, sync::TransitionCreateFlags::None, false
    ));
}

// Different pipelines, no ownership transfers, NoFence set -> no semaphore.
TEST(VulkanTransitionDataTest, NeedsCrossQueueSemaphore_NoFenceWithoutOwnershipTransfers)
{
    EXPECT_FALSE(needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics, sync::PipelineStage::AsyncCompute, sync::TransitionCreateFlags::NoFence, false
    ));
}

// Different pipelines, ownership transfers required, NoFence set -> semaphore still needed.
TEST(VulkanTransitionDataTest, NeedsCrossQueueSemaphore_OwnershipTransfersOverrideNoFence)
{
    EXPECT_TRUE(needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics, sync::PipelineStage::AsyncCompute, sync::TransitionCreateFlags::NoFence, true
    ));
}

// Different pipelines, ownership transfers, no NoFence -> semaphore needed.
TEST(VulkanTransitionDataTest, NeedsCrossQueueSemaphore_OwnershipTransfersNoFlags)
{
    EXPECT_TRUE(needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics, sync::PipelineStage::AsyncCompute, sync::TransitionCreateFlags::None, true
    ));
}

// canUseVulkanSplitBarrier: allowSplitBarriers=false disqualifies.
TEST(VulkanTransitionDataTest, CanUseSplitBarrier_AllowSplitBarriersFalse)
{
    VulkanBarrierSettings settings{};
    settings.allowSplitBarriers = false;
    settings.maxBarriersPerBatch = -1;

    EXPECT_FALSE(canUseVulkanSplitBarrier(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::None, settings, true
    ));
}

// canUseVulkanSplitBarrier: maxBarriersPerBatch > 0 disqualifies.
TEST(VulkanTransitionDataTest, CanUseSplitBarrier_MaxBarriersPerBatchPositiveDisqualifies)
{
    VulkanBarrierSettings settings{};
    settings.allowSplitBarriers = true;
    settings.maxBarriersPerBatch = 1;

    EXPECT_FALSE(canUseVulkanSplitBarrier(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::None, settings, true
    ));
}

// canUseVulkanSplitBarrier: supportsParallelRendering=false disqualifies.
TEST(VulkanTransitionDataTest, CanUseSplitBarrier_NoParallelRenderingDisqualifies)
{
    VulkanBarrierSettings settings{};
    settings.allowSplitBarriers = true;
    settings.maxBarriersPerBatch = -1;

    EXPECT_FALSE(canUseVulkanSplitBarrier(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::None, settings, false
    ));
}

// canUseVulkanSplitBarrier: different pipelines disqualifies.
TEST(VulkanTransitionDataTest, CanUseSplitBarrier_DifferentPipelinesDisqualifies)
{
    VulkanBarrierSettings settings{};
    settings.allowSplitBarriers = true;
    settings.maxBarriersPerBatch = -1;

    EXPECT_FALSE(canUseVulkanSplitBarrier(
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute,
        sync::TransitionCreateFlags::None,
        settings,
        true
    ));
}

// canUseVulkanSplitBarrier: NoSplit flag disqualifies.
TEST(VulkanTransitionDataTest, CanUseSplitBarrier_NoSplitFlagDisqualifies)
{
    VulkanBarrierSettings settings{};
    settings.allowSplitBarriers = true;
    settings.maxBarriersPerBatch = -1;

    EXPECT_FALSE(canUseVulkanSplitBarrier(
        sync::PipelineStage::Graphics,
        sync::PipelineStage::Graphics,
        sync::TransitionCreateFlags::NoSplit,
        settings,
        true
    ));
}

// canUseVulkanSplitBarrier: all conditions met -> true.
TEST(VulkanTransitionDataTest, CanUseSplitBarrier_AllConditionsMetReturnsTrue)
{
    VulkanBarrierSettings settings{};
    settings.allowSplitBarriers = true;
    settings.maxBarriersPerBatch = -1;

    EXPECT_TRUE(canUseVulkanSplitBarrier(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::None, settings, true
    ));
}

// vulkanTransitionIncludesHostSync: empty view returns false.
TEST(VulkanTransitionDataTest, IncludesHostSync_EmptyViewReturnsFalse)
{
    EXPECT_FALSE(vulkanTransitionIncludesHostSync(gp::ConstVectorView<sync::TransitionInfo>{}));
}

// vulkanTransitionIncludesHostSync: CPURead in accessBefore returns true.
TEST(VulkanTransitionDataTest, IncludesHostSync_CPUReadInAccessBefore)
{
    const sync::TransitionInfo info(
        static_cast<resource::Buffer*>(nullptr), sync::ResourceAccess::CPURead, sync::ResourceAccess::SRVCompute
    );
    const gp::ConstVectorView<sync::TransitionInfo> view(&info, 1u);

    EXPECT_TRUE(vulkanTransitionIncludesHostSync(view));
}

// vulkanTransitionIncludesHostSync: CPURead in accessAfter returns true.
TEST(VulkanTransitionDataTest, IncludesHostSync_CPUReadInAccessAfter)
{
    const sync::TransitionInfo info(
        static_cast<resource::Buffer*>(nullptr), sync::ResourceAccess::CopyDest, sync::ResourceAccess::CPURead
    );
    const gp::ConstVectorView<sync::TransitionInfo> view(&info, 1u);

    EXPECT_TRUE(vulkanTransitionIncludesHostSync(view));
}

// vulkanTransitionIncludesHostSync: CPURead combined with other bits in accessBefore returns true.
TEST(VulkanTransitionDataTest, IncludesHostSync_CPUReadCombinedWithOtherBits)
{
    const sync::TransitionInfo info(
        static_cast<resource::Buffer*>(nullptr),
        sync::ResourceAccess::CPURead | sync::ResourceAccess::SRVCompute,
        sync::ResourceAccess::CopyDest
    );
    const gp::ConstVectorView<sync::TransitionInfo> view(&info, 1u);

    EXPECT_TRUE(vulkanTransitionIncludesHostSync(view));
}

// vulkanTransitionIncludesHostSync: no CPURead anywhere returns false.
TEST(VulkanTransitionDataTest, IncludesHostSync_NoCPUReadReturnsFalse)
{
    const sync::TransitionInfo info(
        static_cast<resource::Buffer*>(nullptr), sync::ResourceAccess::CopyDest, sync::ResourceAccess::SRVCompute
    );
    const gp::ConstVectorView<sync::TransitionInfo> view(&info, 1u);

    EXPECT_FALSE(vulkanTransitionIncludesHostSync(view));
}

// Live device fixture for createVulkanTransition / releaseVulkanTransition tests.
class VulkanTransitionDataLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader unavailable; skipping live transition data tests.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-transition-data-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping live transition data tests.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No physical device found; skipping live transition data tests.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping live transition data tests.";
        }

        g_transitionDevice = m_device;
        g_transitionSettings = VulkanBarrierSettings{};

        sync::TransitionBackendHooks hooks{};
        hooks.privateDataSizeInBytes = getVulkanTransitionPrivateDataSize();
        hooks.privateDataAlignInBytes = getVulkanTransitionPrivateDataAlign();
        hooks.onCreate = &testOnCreateVulkanTransition;
        hooks.onRelease = &testOnReleaseVulkanTransition;
        sync::TransitionController::configureBackend(hooks);
    }

    virtual void TearDown() override
    {
        sync::TransitionController::resetBackend();
        g_transitionDevice = nullptr;

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

    VulkanInstance m_instance{};
    VulkanDevice* m_device{ nullptr };
    bool m_loaderLoaded{ false };
};

// createVulkanTransition copies transitionInfos, sets srcPipelines and dstPipelines.
// releaseVulkanTransition destructs the payload cleanly.
TEST_F(VulkanTransitionDataLiveTest, CreateSetsFieldsAndReleaseCleansUp)
{
    const sync::TransitionInfo info(
        static_cast<resource::Buffer*>(nullptr), sync::ResourceAccess::CopyDest, sync::ResourceAccess::SRVCompute
    );

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::None;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&info, 1u);

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    EXPECT_EQ(data->srcPipelines, sync::PipelineStage::Graphics);
    EXPECT_EQ(data->dstPipelines, sync::PipelineStage::Graphics);
    ASSERT_EQ(data->transitionInfos.size(), 1u);
    EXPECT_EQ(data->transitionInfos[0u], info);

    // No cross-queue semaphore for same-queue transition.
    EXPECT_EQ(data->semaphore, nullptr);

    // Retrieve the event before retiring so we can return it to the pool.
    const VkEvent eventHandle = data->eventHandle;
    if (eventHandle != VK_NULL_HANDLE)
    {
        m_device->releaseBarrierEvent(eventHandle);
    }

    // Retire the transition.
    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// Cross-queue transition creates a semaphore when needsVulkanCrossQueueSemaphore is true.
TEST_F(VulkanTransitionDataLiveTest, CrossQueueTransitionCreatesSemaphore)
{
    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::AsyncCompute;
    createInfo.flags = sync::TransitionCreateFlags::None;

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    // Cross-queue with ownership transfers or without NoFence needs a semaphore.
    const bool expectSemaphore = needsVulkanCrossQueueSemaphore(
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute,
        sync::TransitionCreateFlags::None,
        m_device->needsQueueFamilyOwnershipTransfers()
    );

    if (expectSemaphore)
    {
        EXPECT_NE(data->semaphore, nullptr);
    }
    else
    {
        EXPECT_EQ(data->semaphore, nullptr);
    }

    // No split barrier for cross-queue.
    EXPECT_EQ(data->eventHandle, VK_NULL_HANDLE);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
}

// Same-queue split-eligible transition: eventHandle non-null when device supports parallel rendering.
TEST_F(VulkanTransitionDataLiveTest, SameQueueSplitEligibleSetsEventHandle)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping split-barrier event test.";
    }

    g_transitionSettings.allowSplitBarriers = true;
    g_transitionSettings.maxBarriersPerBatch = -1;

    const sync::TransitionInfo info(
        static_cast<resource::Buffer*>(nullptr), sync::ResourceAccess::CopyDest, sync::ResourceAccess::SRVCompute
    );

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::None;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&info, 1u);

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    EXPECT_NE(data->eventHandle, VK_NULL_HANDLE);

    // Return the event to the pool before retiring.
    if (data->eventHandle != VK_NULL_HANDLE)
    {
        m_device->releaseBarrierEvent(data->eventHandle);
    }

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// CPURead in transition infos prevents a split-barrier event from being taken.
TEST_F(VulkanTransitionDataLiveTest, CPUReadTransitionHasNoSplitBarrierEvent)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping CPURead no-split test.";
    }

    g_transitionSettings.allowSplitBarriers = true;
    g_transitionSettings.maxBarriersPerBatch = -1;

    // CPURead in accessBefore prevents an event from being taken.
    const sync::TransitionInfo info(
        static_cast<resource::Buffer*>(nullptr), sync::ResourceAccess::CPURead, sync::ResourceAccess::SRVCompute
    );

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::None;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&info, 1u);

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    EXPECT_EQ(data->eventHandle, VK_NULL_HANDLE);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// NoSplit flag prevents a split-barrier event from being taken.
TEST_F(VulkanTransitionDataLiveTest, NoSplitFlagPreventsEventHandle)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping NoSplit flag test.";
    }

    g_transitionSettings.allowSplitBarriers = true;
    g_transitionSettings.maxBarriersPerBatch = -1;

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::NoSplit;

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    EXPECT_EQ(data->eventHandle, VK_NULL_HANDLE);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

}   // namespace gp::rhi::vulkan::tests
