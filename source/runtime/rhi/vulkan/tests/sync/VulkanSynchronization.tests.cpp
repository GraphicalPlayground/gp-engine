// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/Name.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "sync/GPUFence.hpp"                   // IWYU pragma: keep
#include "sync/SyncForward.hpp"
#include "sync/VulkanFence.hpp"
#include "sync/VulkanGPUFence.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>
#include <thread>

namespace gp::rhi::vulkan::tests
{

namespace
{

gp::Int32 g_syncPointPumpCallCount{ 0 };

void testSyncPointPumpCallback(const VulkanSyncPoint& syncPoint) noexcept
{
    ++g_syncPointPumpCallCount;
    const_cast<VulkanSyncPoint&>(syncPoint).signal();
}

}   // namespace

TEST(VulkanSyncEnumsTest, SemaphoreFlagBitwiseOperations)
{
    VulkanSemaphoreFlags flags = VulkanSemaphoreFlags::None;
    EXPECT_FALSE(hasAnySemaphoreFlags(flags, VulkanSemaphoreFlags::Timeline));
    EXPECT_FALSE(hasAnySemaphoreFlags(flags, VulkanSemaphoreFlags::ExternallyOwned));

    flags |= VulkanSemaphoreFlags::ExternallyOwned;
    flags = flags | VulkanSemaphoreFlags::ExternallySignaled;
    EXPECT_TRUE(hasAnySemaphoreFlags(flags, VulkanSemaphoreFlags::ExternallyOwned));
    EXPECT_TRUE(hasAnySemaphoreFlags(flags, VulkanSemaphoreFlags::ExternallySignaled));
    EXPECT_FALSE(hasAnySemaphoreFlags(flags, VulkanSemaphoreFlags::ExternallyWaited));
    EXPECT_FALSE(hasAnySemaphoreFlags(flags, VulkanSemaphoreFlags::Timeline));
    EXPECT_EQ(flags & VulkanSemaphoreFlags::ExternallyOwned, VulkanSemaphoreFlags::ExternallyOwned);
}

TEST(VulkanSyncPointTest, CreateSignalPollAndWaitPumpCallback)
{
    const VulkanSyncPointRef defaultNamed = VulkanSyncPoint::create(VulkanSyncPointType::Context);
    ASSERT_NE(defaultNamed, nullptr);
    EXPECT_EQ(defaultNamed->getType(), VulkanSyncPointType::Context);
    EXPECT_STREQ(defaultNamed->getDebugName(), "VulkanSyncPoint");
    EXPECT_FALSE(defaultNamed->isComplete());

    const VulkanSyncPointRef sp = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "TestSyncPoint");
    ASSERT_NE(sp, nullptr);
    EXPECT_EQ(sp->getType(), VulkanSyncPointType::GPUAndCPU);
    EXPECT_STREQ(sp->getDebugName(), "TestSyncPoint");
    EXPECT_FALSE(sp->isComplete());

    // Waiting invokes the installed pump callback when unsignaled.
    g_syncPointPumpCallCount = 0;
    VulkanSyncPoint::setWaitCallback(&testSyncPointPumpCallback);
    EXPECT_EQ(VulkanSyncPoint::getWaitCallback(), &testSyncPointPumpCallback);

    sp->wait();
    EXPECT_EQ(g_syncPointPumpCallCount, 1);
    EXPECT_TRUE(sp->isComplete());

    // Calling wait() on an already-complete sync point returns immediately without invoking the pump.
    sp->wait();
    EXPECT_EQ(g_syncPointPumpCallCount, 1);

    VulkanSyncPoint::setWaitCallback(nullptr);
    EXPECT_EQ(VulkanSyncPoint::getWaitCallback(), nullptr);

    // Direct signal() is idempotent and unblocks cross-thread wait() without a pump callback.
    EXPECT_FALSE(defaultNamed->getCpuEvent().isSignaled());
    const VulkanSyncPoint& constRef = *defaultNamed;
    EXPECT_FALSE(constRef.getCpuEvent().isSignaled());

    std::thread signaler([defaultNamed]() noexcept
    {
        defaultNamed->signal();
        defaultNamed->signal();   // Idempotent second signal.
    });
    defaultNamed->wait();
    signaler.join();
    EXPECT_TRUE(defaultNamed->isComplete());
}

TEST(VulkanSynchronizationOfflineTest, NullDeviceGuardsForFenceSemaphoreAndGPUFence)
{
    VulkanDevice nullDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false);

    // FenceManager safely handles invalid device and null fence pointers.
    {
        VulkanFenceManager fenceMgr(nullDevice);
        EXPECT_EQ(fenceMgr.getFreeFenceCount(), 0u);
        EXPECT_EQ(fenceMgr.getUsedFenceCount(), 0u);
        EXPECT_EQ(fenceMgr.allocateFence(false), nullptr);
        EXPECT_FALSE(fenceMgr.isFenceSignaled(nullptr));
        EXPECT_FALSE(fenceMgr.waitForFence(nullptr, 0u));
        EXPECT_FALSE(fenceMgr.waitForAnyFence({}, 0u));

        VulkanFence* nullFence = nullptr;
        fenceMgr.resetFence(nullptr);
        fenceMgr.releaseFence(nullFence);
        fenceMgr.waitAndReleaseFence(nullFence, 0u);
        fenceMgr.deinit();
    }

    // Semaphore safely handles invalid device and external handle wrapping.
    {
        const VulkanSemaphoreRef invalidSem = VulkanSemaphore::create(nullDevice, VulkanSemaphoreFlags::None);
        EXPECT_FALSE(invalidSem->isValid());
        EXPECT_FALSE(invalidSem->isTimeline());
        gp::UInt64 value = 123u;
        EXPECT_FALSE(invalidSem->getTimelineSemaphoreValue(value));
        EXPECT_EQ(value, 123u);
        EXPECT_FALSE(invalidSem->waitForTimelineSemaphoreValue(1u, 0u));
        EXPECT_FALSE(invalidSem->signalTimelineSemaphoreValue(1u));

        const VkSemaphore fakeExternal = reinterpret_cast<VkSemaphore>(static_cast<gp::UIntPtr>(0xCAFEu));
        const VulkanSemaphoreRef extSem = VulkanSemaphore::createExternal(
            nullDevice,
            fakeExternal,
            VulkanSemaphoreFlags::ExternallyOwned | VulkanSemaphoreFlags::ExternallySignaled |
                VulkanSemaphoreFlags::ExternallyWaited
        );
        EXPECT_TRUE(extSem->isValid());
        EXPECT_EQ(extSem->getHandle(), fakeExternal);
        EXPECT_TRUE(extSem->isExternallyOwned());
        EXPECT_TRUE(extSem->isExternallySignaled());
        EXPECT_TRUE(extSem->isExternallyWaited());
        EXPECT_FALSE(extSem->isImmediateDeletion());
    }

    // GPUFence polls and waits through VulkanSyncPoint cleanly even offline.
    {
        alignas(64) gp::UInt8 dummyCmdListStorage[64]{};
        gp::rhi::command::CommandListImmediate& dummyCmdList =
            *reinterpret_cast<gp::rhi::command::CommandListImmediate*>(dummyCmdListStorage);

        VulkanGPUFence* const rawFence = new VulkanGPUFence(nullDevice, gp::Name("OfflineGPUFence"), true);
        const gp::rhi::sync::GPUFenceRef fenceRef(rawFence);

        EXPECT_EQ(rawFence->getEvent(), VK_NULL_HANDLE);
        EXPECT_FALSE(rawFence->poll());
        EXPECT_FALSE(rawFence->poll(gp::rhi::GPUMask::gpu0()));

        // Waiting with no sync points attached safely no-ops.
        rawFence->wait(dummyCmdList, gp::rhi::GPUMask::gpu0());

        const VulkanSyncPointRef submitted = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "Submitted");
        const VulkanSyncPointRef completed = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "GPUFenceTest");
        rawFence->setSyncPoints(submitted, completed);
        EXPECT_EQ(rawFence->getSubmittedSyncPoint(), submitted);
        EXPECT_FALSE(rawFence->poll());

        g_syncPointPumpCallCount = 0;
        VulkanSyncPoint::setWaitCallback(&testSyncPointPumpCallback);
        rawFence->wait(dummyCmdList, gp::rhi::GPUMask::gpu0());
        EXPECT_EQ(g_syncPointPumpCallCount, 1);
        EXPECT_TRUE(rawFence->poll());
        EXPECT_TRUE(rawFence->poll(gp::rhi::GPUMask::gpu0()));

        // Already-polled fence skips wait pump callback.
        rawFence->wait(dummyCmdList, gp::rhi::GPUMask::gpu0());
        EXPECT_EQ(g_syncPointPumpCallCount, 1);
        VulkanSyncPoint::setWaitCallback(nullptr);

        // clear() resets both completed and submitted sync points even without a VkEvent.
        rawFence->clear();
        EXPECT_FALSE(rawFence->poll());
        EXPECT_EQ(rawFence->getCompletedSyncPoint(), nullptr);
        EXPECT_EQ(rawFence->getSubmittedSyncPoint(), nullptr);

        // Offline useTimelineSemaphores = false safely degrades to VK_NULL_HANDLE.
        VulkanGPUFence* const rawEventFence = new VulkanGPUFence(nullDevice, gp::Name("OfflineEventFence"), false);
        const gp::rhi::sync::GPUFenceRef eventFenceRef(rawEventFence);
        EXPECT_EQ(rawEventFence->getEvent(), VK_NULL_HANDLE);
    }

    gp::rhi::resource::ResourceDeletionQueue::flushPendingDeletes();
}

TEST(VulkanSynchronizationLiveTest, FenceManagerSemaphoreAndGPUFenceOnLiveDevice)
{
    if (!VulkanLoader::loadLibrary())
    {
        GTEST_SKIP() << "Vulkan loader library not available on this host.";
    }

    VulkanInstance instance;
    VulkanInstanceCreateDesc instDesc{};
    instDesc.applicationName = "VulkanSyncUnitTest";
    instDesc.allowRetryWithoutLayers = true;
    instDesc.allowMissingSurfaceFunctions = true;
    instDesc.enableDebugCallbacks = false;

    if (!instance.initialize(instDesc))
    {
        VulkanLoader::freeLibrary();
        GTEST_SKIP() << "No Vulkan ICD available on this host.";
    }

    VulkanPhysicalDeviceInfo gpuInfo{};
    const VkPhysicalDevice physicalDevice =
        selectPhysicalDevice(instance.getHandle(), instance.getApiVersion(), gpuInfo);
    if (physicalDevice == VK_NULL_HANDLE)
    {
        instance.shutdown();
        VulkanLoader::freeLibrary();
        GTEST_SKIP() << "No compatible Vulkan physical device available on this host.";
    }

    VulkanDevice device(instance.getHandle(), physicalDevice, instance.getApiVersion(), false);
    if (!device.initialize(false))
    {
        instance.shutdown();
        VulkanLoader::freeLibrary();
        GTEST_SKIP() << "Failed to initialize live VulkanDevice.";
    }

    // 1. VulkanFenceManager: allocate, poll, wait, reset, recycle, waitAndRelease.
    {
        VulkanFenceManager fenceMgr(device);

        VulkanFence* unsignaled = fenceMgr.allocateFence(false);
        ASSERT_NE(unsignaled, nullptr);
        EXPECT_NE(unsignaled->getHandle(), VK_NULL_HANDLE);
        EXPECT_EQ(&unsignaled->getOwner(), &fenceMgr);
        EXPECT_FALSE(fenceMgr.isFenceSignaled(unsignaled));
        EXPECT_FALSE(fenceMgr.waitForFence(unsignaled, 0u));
        EXPECT_EQ(fenceMgr.getUsedFenceCount(), 1u);

        VulkanFence* signaled = fenceMgr.allocateFence(true);
        ASSERT_NE(signaled, nullptr);
        EXPECT_TRUE(fenceMgr.isFenceSignaled(signaled));
        EXPECT_TRUE(fenceMgr.waitForFence(signaled, 1000000u));

        VulkanFence* both[] = { unsignaled, signaled };
        EXPECT_TRUE(fenceMgr.waitForAnyFence(gp::ConstVectorView<VulkanFence*>(both, 2u), 0u));

        VulkanFence* onlyUnsignaled[] = { unsignaled };
        EXPECT_FALSE(fenceMgr.waitForAnyFence(gp::ConstVectorView<VulkanFence*>(onlyUnsignaled, 1u), 0u));

        VulkanFence* allNulls[] = { nullptr, nullptr };
        EXPECT_FALSE(fenceMgr.waitForAnyFence(gp::ConstVectorView<VulkanFence*>(allNulls, 2u), 0u));

        fenceMgr.resetFence(signaled);
        EXPECT_FALSE(signaled->isSignaled());
        EXPECT_FALSE(fenceMgr.isFenceSignaled(signaled));

        fenceMgr.releaseFence(unsignaled);
        EXPECT_EQ(unsignaled, nullptr);
        EXPECT_EQ(fenceMgr.getUsedFenceCount(), 1u);
        EXPECT_EQ(fenceMgr.getFreeFenceCount(), 1u);

        // Requesting a signaled fence while free pool has an unsignaled fence allocates a genuinely signaled VkFence.
        VulkanFence* freshSignaled = fenceMgr.allocateFence(true);
        ASSERT_NE(freshSignaled, nullptr);
        EXPECT_TRUE(fenceMgr.isFenceSignaled(freshSignaled));
        EXPECT_EQ(fenceMgr.getFreeFenceCount(), 1u);
        fenceMgr.releaseFence(freshSignaled);

        // Re-allocating an unsignaled fence reuses the pooled fence.
        VulkanFence* recycled = fenceMgr.allocateFence(false);
        ASSERT_NE(recycled, nullptr);
        EXPECT_EQ(fenceMgr.getFreeFenceCount(), 1u);
        EXPECT_EQ(fenceMgr.getUsedFenceCount(), 2u);

        fenceMgr.releaseFence(recycled);
        fenceMgr.waitAndReleaseFence(signaled, 0u);
        EXPECT_EQ(signaled, nullptr);
        EXPECT_EQ(fenceMgr.getUsedFenceCount(), 0u);
        EXPECT_EQ(fenceMgr.getFreeFenceCount(), 3u);

        fenceMgr.deinit();
        EXPECT_EQ(fenceMgr.getFreeFenceCount(), 0u);
    }

    // 2. VulkanSemaphore: binary semaphore creation + timeline semaphore counter/wait/signal.
    {
        const VulkanSemaphoreRef binarySem = VulkanSemaphore::create(device, VulkanSemaphoreFlags::ImmediateDeletion);
        ASSERT_NE(binarySem, nullptr);
        EXPECT_TRUE(binarySem->isValid());
        EXPECT_FALSE(binarySem->isTimeline());
        EXPECT_TRUE(binarySem->isImmediateDeletion());

        // Timeline operations on a binary semaphore safely return false.
        gp::UInt64 untouched = 77u;
        EXPECT_FALSE(binarySem->getTimelineSemaphoreValue(untouched));
        EXPECT_EQ(untouched, 77u);
        EXPECT_FALSE(binarySem->waitForTimelineSemaphoreValue(1u, 0u));
        EXPECT_FALSE(binarySem->signalTimelineSemaphoreValue(1u));

        const bool timelineSupported = (device.getOptionalExtensions().hasKHRTimelineSemaphore != 0u) ||
                                       (device.getFeatures().getCore12().timelineSemaphore == VK_TRUE);
        if (timelineSupported)
        {
            const VulkanSemaphoreRef timelineSem = VulkanSemaphore::create(
                device, VulkanSemaphoreFlags::Timeline | VulkanSemaphoreFlags::ImmediateDeletion, 10u
            );
            ASSERT_NE(timelineSem, nullptr);
            if (timelineSem->isValid())
            {
                EXPECT_TRUE(timelineSem->isTimeline());
                gp::UInt64 counter = 0u;
                EXPECT_TRUE(timelineSem->getTimelineSemaphoreValue(counter));
                EXPECT_EQ(counter, 10u);
                EXPECT_TRUE(timelineSem->waitForTimelineSemaphoreValue(10u, 0u));
                EXPECT_FALSE(timelineSem->waitForTimelineSemaphoreValue(15u, 0u));
                EXPECT_TRUE(timelineSem->signalTimelineSemaphoreValue(15u));
                EXPECT_TRUE(timelineSem->getTimelineSemaphoreValue(counter));
                EXPECT_EQ(counter, 15u);
                EXPECT_TRUE(timelineSem->waitForTimelineSemaphoreValue(15u, 0u));
            }
        }
    }

    // 3. VulkanGPUFence with fallback VkEvent path (useTimelineSemaphores = false).
    {
        VulkanGPUFence* const rawFence = new VulkanGPUFence(device, gp::Name("LiveEventGPUFence"), false);
        const gp::rhi::sync::GPUFenceRef fenceRef(rawFence);
        ASSERT_NE(rawFence->getEvent(), VK_NULL_HANDLE);
        EXPECT_FALSE(rawFence->poll());

        const VulkanSyncPointRef submitted = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "Submitted");
        const VulkanSyncPointRef completed = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "Completed");
        rawFence->setSyncPoints(submitted, completed);

        // Even if VkEvent is set, poll() remains false until submittedSyncPoint is signaled.
        if (dynamic_api::vkSetEvent != nullptr)
        {
            EXPECT_EQ(vkSetEvent(device.getHandle(), rawFence->getEvent()), VK_SUCCESS);
            EXPECT_FALSE(rawFence->poll());
            submitted->signal();
            EXPECT_TRUE(rawFence->poll());
        }

        rawFence->clear();
        EXPECT_FALSE(rawFence->poll());

        // Signaling completedSyncPoint directly makes poll() return true even when VkEvent is unset.
        const VulkanSyncPointRef completedOnly = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "Done");
        rawFence->setSyncPoints(nullptr, completedOnly);
        completedOnly->signal();
        EXPECT_TRUE(rawFence->poll());
    }

    gp::rhi::resource::ResourceDeletionQueue::flushPendingDeletes();

    device.destroy();
    instance.shutdown();
    VulkanLoader::freeLibrary();
}

}   // namespace gp::rhi::vulkan::tests
