// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/PoolAllocatorEnums.hpp"   // IWYU pragma: keep
#include "allocators/PoolResource.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanPoolAllocator.hpp"
#include "memory/VulkanStagingManager.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanMemoryEnumsTest, StringsFlagsRangeCoalescingAndAtomSize)
{
    EXPECT_STREQ(vulkanAllocationTypeToString(VulkanAllocationType::Empty), "Empty");
    EXPECT_STREQ(vulkanAllocationTypeToString(VulkanAllocationType::PooledBuffer), "PooledBuffer");
    EXPECT_STREQ(vulkanAllocationTypeToString(VulkanAllocationType::Buffer), "Buffer");
    EXPECT_STREQ(vulkanAllocationTypeToString(VulkanAllocationType::Image), "Image");
    EXPECT_STREQ(vulkanAllocationTypeToString(VulkanAllocationType::ImageDedicated), "ImageDedicated");
    EXPECT_STREQ(vulkanAllocationTypeToString(VulkanAllocationType::Count), "Unknown");

    EXPECT_STREQ(vulkanAllocationMetaTypeToString(VulkanAllocationMetaType::UniformBuffer), "UBO");
    EXPECT_STREQ(vulkanAllocationMetaTypeToString(VulkanAllocationMetaType::ImageRenderTarget), "ImageRT");
    EXPECT_STREQ(vulkanAllocationMetaTypeToString(VulkanAllocationMetaType::BufferStaging), "BufferStg");
    EXPECT_STREQ(vulkanAllocationMetaTypeToString(VulkanAllocationMetaType::Count), "Unknown");

    EXPECT_EQ(getVulkanPoolDefragMode(), VulkanPoolDefragMode::All);

    // Memory property flag translation.
    EXPECT_EQ(getMemoryPropertyFlags(VulkanAllocationFlags::None, false), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    EXPECT_EQ(
        getMemoryPropertyFlags(VulkanAllocationFlags::None, true),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    EXPECT_EQ(
        getMemoryPropertyFlags(VulkanAllocationFlags::HostVisible, false),
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    EXPECT_EQ(
        getMemoryPropertyFlags(VulkanAllocationFlags::HostVisible | VulkanAllocationFlags::PreferBAR, false),
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    EXPECT_EQ(
        getMemoryPropertyFlags(VulkanAllocationFlags::HostCached, false),
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    );
    EXPECT_EQ(
        getMemoryPropertyFlags(VulkanAllocationFlags::Protected, false),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT
    );
    EXPECT_EQ(
        getMemoryPropertyFlags(VulkanAllocationFlags::Memoryless, false), VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
    );

    // Resource heap stats accumulation.
    VulkanResourceHeapStats statsA{};
    statsA.bufferAllocations = 2u;
    statsA.usedBufferMemory = 1024u;
    statsA.totalMemory = 4096u;
    statsA.pages = 1u;
    statsA.memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VulkanResourceHeapStats statsB{};
    statsB.imageAllocations = 3u;
    statsB.usedImageMemory = 2048u;
    statsB.totalMemory = 8192u;
    statsB.pages = 2u;
    statsB.memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    statsA += statsB;
    EXPECT_EQ(statsA.bufferAllocations, 2u);
    EXPECT_EQ(statsA.imageAllocations, 3u);
    EXPECT_EQ(statsA.usedBufferMemory, 1024u);
    EXPECT_EQ(statsA.usedImageMemory, 2048u);
    EXPECT_EQ(statsA.totalMemory, 12288u);
    EXPECT_EQ(statsA.pages, 3u);
    EXPECT_EQ(statsA.memoryFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // VulkanMemoryRange coalescing and sub-allocation.
    gp::Vector<VulkanMemoryRange> ranges{};
    VulkanMemoryRange::addAndMerge(ranges, VulkanMemoryRange{ 0u, 100u });
    VulkanMemoryRange::addAndMerge(ranges, VulkanMemoryRange{ 200u, 100u });
    ASSERT_EQ(ranges.size(), 2u);

    // Bridge the gap [100, 200) -> collapses into single [0, 300).
    VulkanMemoryRange::addAndMerge(ranges, VulkanMemoryRange{ 100u, 100u });
    ASSERT_EQ(ranges.size(), 1u);
    EXPECT_EQ(ranges[0u].offset, 0u);
    EXPECT_EQ(ranges[0u].size, 300u);

    VulkanMemoryRange::allocateFromEntry(ranges, 0u, 100u);
    ASSERT_EQ(ranges.size(), 1u);
    EXPECT_EQ(ranges[0u].offset, 100u);
    EXPECT_EQ(ranges[0u].size, 200u);

    VulkanMemoryRange::allocateFromEntry(ranges, 0u, 200u);
    EXPECT_TRUE(ranges.isEmpty());

    // Non-coherent atom size adjustment (including VK_WHOLE_SIZE).
    EXPECT_EQ(adjustToNonCoherentAtomSize(0u, 100u, 1024u, 64u), 128u);
    EXPECT_EQ(adjustToNonCoherentAtomSize(960u, 100u, 1024u, 64u), 64u);
    EXPECT_EQ(adjustToNonCoherentAtomSize(0u, VK_WHOLE_SIZE, 1024u, 64u), VK_WHOLE_SIZE);
    EXPECT_EQ(adjustToNonCoherentAtomSize(1024u, 64u, 1024u, 64u), 0u);
    EXPECT_EQ(adjustToNonCoherentAtomSize(0u, 0u, 1024u, 64u), 0u);

    // joinConsecutiveRanges merges adjacent sorted entries.
    gp::Vector<VulkanMemoryRange> consecutive{};
    consecutive.pushBack(VulkanMemoryRange{ 0u, 50u });
    consecutive.pushBack(VulkanMemoryRange{ 50u, 75u });
    consecutive.pushBack(VulkanMemoryRange{ 200u, 10u });
    VulkanMemoryRange::joinConsecutiveRanges(consecutive);
    ASSERT_EQ(consecutive.size(), 2u);
    EXPECT_EQ(consecutive[0u].offset, 0u);
    EXPECT_EQ(consecutive[0u].size, 125u);
    EXPECT_EQ(consecutive[1u].offset, 200u);

    // Buffer priority calculation.
    EXPECT_FLOAT_EQ(
        VulkanMemoryManager::calculateBufferPriority(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), kVulkanMemoryHighestPriority
    );
    EXPECT_FLOAT_EQ(
        VulkanMemoryManager::calculateBufferPriority(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), kVulkanMemoryMediumPriority
    );
    EXPECT_FLOAT_EQ(
        VulkanMemoryManager::calculateBufferPriority(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), kVulkanMemoryLowPriority
    );
    EXPECT_FLOAT_EQ(
        VulkanMemoryManager::calculateBufferPriority(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), kVulkanMemoryHigherPriority
    );
}

TEST(VulkanMemoryOfflineTest, NullDeviceGuardsAndAllocationSemantics)
{
    VulkanDevice nullDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, 0u, false);
    VulkanDeviceMemoryManager deviceMemMgr{};
    deviceMemMgr.init(&nullDevice);

    EXPECT_EQ(deviceMemMgr.getNumMemoryTypes(), 0u);
    EXPECT_EQ(deviceMemMgr.getTotalMemory(true), 0u);
    EXPECT_EQ(deviceMemMgr.getTotalMemory(false), 0u);
    EXPECT_FALSE(deviceMemMgr.supportsMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    EXPECT_EQ(deviceMemMgr.getBaseHeapSize(0u), 0u);
    EXPECT_EQ(deviceMemMgr.getHeapIndex(0u), 0u);

    gp::UInt64 primaryAlloc = 99u;
    gp::UInt64 primaryLimit = 99u;
    deviceMemMgr.getPrimaryHeapStatus(primaryAlloc, primaryLimit);
    EXPECT_EQ(primaryAlloc, 0u);
    EXPECT_EQ(primaryLimit, 1u);

    gp::UInt32 outType = 0u;
    EXPECT_EQ(
        deviceMemMgr.getMemoryTypeFromProperties(0xFFFFFFFFu, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &outType),
        VK_ERROR_FEATURE_NOT_PRESENT
    );
    EXPECT_EQ(
        deviceMemMgr.getMemoryTypeFromPropertiesExcluding(
            0xFFFFFFFFu, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0u, &outType
        ),
        VK_ERROR_FEATURE_NOT_PRESENT
    );
    EXPECT_EQ(deviceMemMgr.alloc(true, 1024u, 0u), nullptr);
    deviceMemMgr.trimMemory(true, 0u);

    VulkanMemoryManager memMgr(nullDevice, deviceMemMgr);
    memMgr.init();
    EXPECT_EQ(createVulkanBuffer(nullDevice, 1024u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), VK_NULL_HANDLE);

    // Allocation ownership and reference semantics.
    VulkanAllocation allocA{};
    EXPECT_FALSE(allocA.isValid());
    EXPECT_FALSE(allocA.hasAllocation());

    allocA.init(
        VulkanAllocationType::PooledBuffer, VulkanAllocationMetaType::UniformBuffer, 0x1234u, 256u, 64u, 1u, 2u, 7u
    );
    EXPECT_TRUE(allocA.isValid());
    EXPECT_TRUE(allocA.hasAllocation());
    EXPECT_EQ(allocA.getType(), VulkanAllocationType::PooledBuffer);
    EXPECT_EQ(allocA.getBufferHandle(), reinterpret_cast<VkBuffer>(0x1234u));

    VulkanAllocation refB{};
    refB.reference(allocA);
    EXPECT_TRUE(refB.isValid());
    EXPECT_FALSE(refB.hasAllocation());
    EXPECT_EQ(refB.size, 256u);
    EXPECT_EQ(refB.offset, 64u);

    VulkanAllocation allocC{};
    allocC.swap(allocA);
    EXPECT_TRUE(allocC.hasAllocation());
    EXPECT_FALSE(allocA.hasAllocation());
    allocC.disown();
    EXPECT_FALSE(allocC.hasAllocation());

    // Staging manager null guards.
    VulkanStagingManager stagingMgr{};
    stagingMgr.init(&nullDevice, &memMgr);
    EXPECT_EQ(stagingMgr.acquireBuffer(1024u), nullptr);
    VulkanStagingBuffer* nullStaging = nullptr;
    stagingMgr.releaseBuffer(nullptr, nullStaging);
    stagingMgr.processPendingFree(true, true, 0u);
    stagingMgr.deinit();

    // Deferred deletion queue offline callback execution and RHI flush integration.
    VulkanDeferredDeletionQueue deletionQueue(nullDevice, deviceMemMgr, memMgr);
    deletionQueue.enqueueGenericResource(VulkanDeferredDeletionQueue::EntryType::Buffer, 0u);
    EXPECT_EQ(deletionQueue.getPendingEntryCount(), 0u);

    bool callbackInvoked = false;
    deletionQueue.enqueueFunc(
        [&callbackInvoked](VulkanDevice&) noexcept -> void
    {
        callbackInvoked = true;
    }
    );
    EXPECT_EQ(deletionQueue.getPendingEntryCount(), 1u);

    VulkanDeferredDeletionQueue::flushRHIDeletionQueue(true);
    deletionQueue.releaseResources(true, nullptr, 0u);
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(deletionQueue.getPendingEntryCount(), 0u);
}

class VulkanMemoryLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Host Vulkan loader library unavailable; skipping live memory test.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-memory-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping live memory test.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No Vulkan physical device found; skipping live memory test.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping live memory test.";
        }

        m_deviceMemoryManager = new VulkanDeviceMemoryManager();
        m_deviceMemoryManager->init(m_device);

        m_memoryManager = new VulkanMemoryManager(*m_device, *m_deviceMemoryManager);
        m_memoryManager->init();
    }

    virtual void TearDown() override
    {
        if (m_memoryManager != nullptr)
        {
            m_memoryManager->setDeferredDeletionQueue(nullptr);
            m_memoryManager->deinit();
            delete m_memoryManager;
            m_memoryManager = nullptr;
        }

        if (m_deviceMemoryManager != nullptr)
        {
            m_deviceMemoryManager->deinit();
            delete m_deviceMemoryManager;
            m_deviceMemoryManager = nullptr;
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
    VulkanDeviceMemoryManager* m_deviceMemoryManager{ nullptr };
    VulkanMemoryManager* m_memoryManager{ nullptr };
};

TEST_F(VulkanMemoryLiveTest, DeviceMemoryManagerBudgetBlockCachingAndMapping)
{
    EXPECT_GT(m_deviceMemoryManager->getNumMemoryTypes(), 0u);
    EXPECT_GT(m_deviceMemoryManager->getTotalMemory(true), 0u);

    m_deviceMemoryManager->updateMemoryProperties();
    const VulkanMemoryStats& stats = m_deviceMemoryManager->getMemoryStats();
    EXPECT_GT(stats.budgetLocal, 0u);

    gp::UInt32 hostMemType = 0u;
    const VkMemoryPropertyFlags hostFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    ASSERT_EQ(m_deviceMemoryManager->getMemoryTypeFromProperties(0xFFFFFFFFu, hostFlags, &hostMemType), VK_SUCCESS);

    // Allocate, persistently map, write/read, flush/invalidate, cache-free, and reuse.
    constexpr VkDeviceSize kAllocBytes = 4096u;
    VulkanDeviceMemoryAllocation* memAlloc =
        m_deviceMemoryManager->alloc(true, kAllocBytes, hostMemType, nullptr, kVulkanMemoryMediumPriority, false);
    ASSERT_NE(memAlloc, nullptr);
    EXPECT_NE(memAlloc->getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(memAlloc->getSize(), kAllocBytes);
    EXPECT_EQ(memAlloc->getMemoryTypeIndex(), hostMemType);
    EXPECT_TRUE(memAlloc->canBeMapped());
    EXPECT_TRUE(memAlloc->isCoherent());
    EXPECT_FALSE(memAlloc->isDedicated());

    void* const mappedPtr = memAlloc->map(kAllocBytes, 0u);
    ASSERT_NE(mappedPtr, nullptr);
    EXPECT_TRUE(memAlloc->isMapped());
    EXPECT_EQ(memAlloc->getMappedPointer(), mappedPtr);

    gp::UInt32* const words = static_cast<gp::UInt32*>(mappedPtr);
    words[0u] = 0xCAFEBABEu;
    words[1u] = 0x12345678u;
    flushMappedMemoryRange(*m_device, *memAlloc, 0u, 64u);
    invalidateMappedMemoryRange(*m_device, *memAlloc, 0u, 64u);
    EXPECT_EQ(words[0u], 0xCAFEBABEu);
    EXPECT_EQ(words[1u], 0x12345678u);

    const gp::UInt32 allocCountBeforeFree = m_deviceMemoryManager->getNumAllocations();
    EXPECT_GE(allocCountBeforeFree, 1u);

    m_deviceMemoryManager->free(memAlloc, 1u);
    EXPECT_EQ(memAlloc, nullptr);

    // Re-allocating the same (type, size) reuses the cached block without increasing vkAllocateMemory count.
    VulkanDeviceMemoryAllocation* reusedAlloc =
        m_deviceMemoryManager->alloc(true, kAllocBytes, hostMemType, nullptr, kVulkanMemoryMediumPriority, false);
    ASSERT_NE(reusedAlloc, nullptr);
    EXPECT_EQ(m_deviceMemoryManager->getNumAllocations(), allocCountBeforeFree);

    gp::Vector<VulkanResourceHeapStats> heapDump{};
    m_deviceMemoryManager->getMemoryDump(heapDump);
    EXPECT_EQ(heapDump.size(), m_deviceMemoryManager->getMemoryProperties().memoryHeapCount);

    m_deviceMemoryManager->free(reusedAlloc, 2u);
    m_deviceMemoryManager->trimMemory(true, 3u);
}

TEST_F(VulkanMemoryLiveTest, PoolAllocatorMemoryManagerBuffersAndImages)
{
    EXPECT_GE(m_memoryManager->getImagePoolAlignment(), 256u);
    EXPECT_GE(m_memoryManager->getBufferPoolAlignment(), 256u);

    // Sub-allocate two uniform buffers from the pre-created manual sub-allocation pool.
    const VkBufferUsageFlags uboUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkMemoryPropertyFlags hostFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VulkanAllocation uboA{};
    VulkanAllocation uboB{};
    ASSERT_TRUE(m_memoryManager->allocateBufferPooled(
        uboA, 512u, 256u, uboUsage, hostFlags, VulkanAllocationMetaType::UniformBuffer
    ));
    ASSERT_TRUE(m_memoryManager->allocateBufferPooled(
        uboB, 512u, 256u, uboUsage, hostFlags, VulkanAllocationMetaType::UniformBuffer
    ));
    EXPECT_TRUE(uboA.isValid());
    EXPECT_TRUE(uboB.isValid());
    EXPECT_NE(uboA.getBufferHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(uboA.getBufferHandle(), uboB.getBufferHandle());
    EXPECT_NE(uboA.offset, uboB.offset);

    void* const ptrA = uboA.getMappedPointer(*m_memoryManager);
    void* const ptrB = uboB.getMappedPointer(*m_memoryManager);
    ASSERT_NE(ptrA, nullptr);
    ASSERT_NE(ptrB, nullptr);
    EXPECT_NE(ptrA, ptrB);
    *static_cast<gp::UInt32*>(ptrA) = 0x11111111u;
    *static_cast<gp::UInt32*>(ptrB) = 0x22222222u;
    EXPECT_EQ(*static_cast<gp::UInt32*>(ptrA), 0x11111111u);
    EXPECT_EQ(*static_cast<gp::UInt32*>(ptrB), 0x22222222u);

    // Batch free pooled allocations.
    VulkanAllocation batch[2u] = { uboA, uboB };
    uboA.disown();
    uboB.disown();
    m_memoryManager->freePoolAllocations(gp::VectorView<VulkanAllocation>(batch, 2u));
    EXPECT_FALSE(batch[0u].isValid());
    EXPECT_FALSE(batch[1u].isValid());

    // Standalone VkBuffer with AutoBind + HostVisible.
    VkBuffer rawBuffer =
        createVulkanBuffer(*m_device, 2048u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    ASSERT_NE(rawBuffer, VK_NULL_HANDLE);

    VulkanAllocation bufAlloc{};
    ASSERT_TRUE(m_memoryManager->allocateBufferMemory(
        bufAlloc, rawBuffer, VulkanAllocationFlags::AutoBind | VulkanAllocationFlags::HostVisible, "TestAutoBindBuffer"
    ));
    EXPECT_NE(bufAlloc.getDeviceMemoryHandle(*m_memoryManager), VK_NULL_HANDLE);
    EXPECT_NE(bufAlloc.getMemoryPropertyFlags(*m_memoryManager) & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0u);
    EXPECT_NE(bufAlloc.getMappedPointer(*m_memoryManager), nullptr);

    vkDestroyBuffer(m_device->getHandle(), rawBuffer, GP_VULKAN_CPU_ALLOCATOR);
    bufAlloc.free(*m_memoryManager, VulkanFreeFlags::DontDefer);
    EXPECT_FALSE(bufAlloc.hasAllocation());

    // Placed and dedicated VkImage allocations.
    VkImageCreateInfo imgInfo = zeroVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = VkExtent3D{ 64u, 64u, 1u };
    imgInfo.mipLevels = 1u;
    imgInfo.arrayLayers = 1u;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage placedImage = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateImage(m_device->getHandle(), &imgInfo, GP_VULKAN_CPU_ALLOCATOR, &placedImage), VK_SUCCESS);

    VkMemoryRequirements imgReqs{};
    vkGetImageMemoryRequirements(m_device->getHandle(), placedImage, &imgReqs);

    VulkanAllocation placedImgAlloc{};
    ASSERT_TRUE(m_memoryManager->allocateImageMemory(
        placedImgAlloc, imgReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VulkanAllocationMetaType::ImageOther
    ));
    EXPECT_EQ(placedImgAlloc.bindImage(*m_memoryManager, placedImage), VK_SUCCESS);

    VkImage dedicatedImage = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateImage(m_device->getHandle(), &imgInfo, GP_VULKAN_CPU_ALLOCATOR, &dedicatedImage), VK_SUCCESS);

    VulkanAllocation dedicatedImgAlloc{};
    ASSERT_TRUE(m_memoryManager->allocateDedicatedImageMemory(
        dedicatedImgAlloc,
        dedicatedImage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VulkanAllocationMetaType::ImageRenderTarget,
        false,
        true
    ));
    EXPECT_EQ(dedicatedImgAlloc.getType(), VulkanAllocationType::ImageDedicated);
    EXPECT_EQ(dedicatedImgAlloc.bindImage(*m_memoryManager, dedicatedImage), VK_SUCCESS);

    VulkanResourceHeapStats memDump{};
    m_memoryManager->getMemoryDump(memDump);
    EXPECT_GT(memDump.totalMemory, 0u);
    EXPECT_GE(memDump.imageAllocations, 2u);

    vkDestroyImage(m_device->getHandle(), dedicatedImage, GP_VULKAN_CPU_ALLOCATOR);
    vkDestroyImage(m_device->getHandle(), placedImage, GP_VULKAN_CPU_ALLOCATOR);
    dedicatedImgAlloc.free(*m_memoryManager, VulkanFreeFlags::DontDefer);
    placedImgAlloc.free(*m_memoryManager, VulkanFreeFlags::DontDefer);

    m_memoryManager->releaseFreedPages(10u);
}

TEST_F(VulkanMemoryLiveTest, StagingManagerSyncPointGatingAndDeferredDeletion)
{
    VulkanDeferredDeletionQueue deletionQueue(*m_device, *m_deviceMemoryManager, *m_memoryManager);
    m_memoryManager->setDeferredDeletionQueue(&deletionQueue);

    VulkanStagingManager stagingManager{};
    stagingManager.init(m_device, m_memoryManager);

    // Acquire upload (HOST_COHERENT) and readback (HOST_CACHED) staging buffers.
    VulkanStagingBuffer* uploadBuf =
        stagingManager.acquireBuffer(1024u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    ASSERT_NE(uploadBuf, nullptr);
    EXPECT_NE(uploadBuf->getHandle(), VK_NULL_HANDLE);
    EXPECT_NE(uploadBuf->getDeviceMemoryHandle(), VK_NULL_HANDLE);
    ASSERT_NE(uploadBuf->getMappedPointer(), nullptr);
    EXPECT_EQ(uploadBuf->getSize(), 1024u);
    EXPECT_EQ(stagingManager.getUsedBufferCount(), 1u);
    EXPECT_GE(stagingManager.getUsedMemory(), 1024u);
    EXPECT_GE(stagingManager.getPeakUsedMemory(), 1024u);

    *static_cast<gp::UInt32*>(uploadBuf->getMappedPointer()) = 0xDEADBEEFu;
    uploadBuf->flushMappedMemory();

    VulkanStagingBuffer* readbackBuf =
        stagingManager.acquireBuffer(256u, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    ASSERT_NE(readbackBuf, nullptr);
    EXPECT_GE(readbackBuf->getSize(), 256u);
    readbackBuf->invalidateMappedMemory();

    // Release uploadBuf gated by an unsignaled sync point: must stay pending until signaled.
    const VulkanSyncPointRef uploadSp = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "StagingUpload");
    VulkanStagingBuffer* const originalUploadPtr = uploadBuf;
    stagingManager.releaseBuffer(uploadSp, uploadBuf, 1u);
    EXPECT_EQ(uploadBuf, nullptr);
    EXPECT_EQ(stagingManager.getPendingFreeBufferCount(), 1u);
    EXPECT_EQ(stagingManager.getFreeBufferCount(), 0u);

    stagingManager.processPendingFree(false, false, 1u);
    EXPECT_EQ(stagingManager.getPendingFreeBufferCount(), 1u);
    EXPECT_EQ(stagingManager.getFreeBufferCount(), 0u);

    // Signal sync point -> buffer promotes to free list and is reused on matching acquire.
    uploadSp->signal();
    stagingManager.processPendingFree(false, false, 2u);
    EXPECT_EQ(stagingManager.getPendingFreeBufferCount(), 0u);
    EXPECT_EQ(stagingManager.getFreeBufferCount(), 1u);

    VulkanStagingBuffer* recycledUpload =
        stagingManager.acquireBuffer(1024u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EXPECT_EQ(recycledUpload, originalUploadPtr);

    VulkanResourceHeapStats stagingStats{};
    stagingManager.getMemoryDump(stagingStats);
    EXPECT_EQ(stagingStats.bufferAllocations, 2u);

    stagingManager.releaseBuffer(nullptr, recycledUpload, 2u);
    stagingManager.releaseBuffer(nullptr, readbackBuf, 2u);
    stagingManager.processPendingFree(false, true, 100u);
    EXPECT_EQ(stagingManager.getFreeBufferCount(), 0u);
    EXPECT_EQ(stagingManager.getUsedMemory(), 0u);
    stagingManager.deinit();

    // Deferred deletion queue gating native VkBuffer, VulkanAllocation, and callback on a VulkanSyncPoint.
    VkBuffer deferredBuffer = createVulkanBuffer(*m_device, 512u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    ASSERT_NE(deferredBuffer, VK_NULL_HANDLE);

    VulkanAllocation deferredAlloc{};
    ASSERT_TRUE(m_memoryManager->allocateBufferMemory(
        deferredAlloc, deferredBuffer, VulkanAllocationFlags::AutoBind | VulkanAllocationFlags::HostVisible
    ));

    bool deferredCallbackRan = false;
    deletionQueue.enqueueHandle(VulkanDeferredDeletionQueue::EntryType::Buffer, deferredBuffer);
    deferredAlloc.free(*m_memoryManager, VulkanFreeFlags::None);
    deletionQueue.enqueueFunc(
        [&deferredCallbackRan](VulkanDevice&) noexcept -> void
    {
        deferredCallbackRan = true;
    }
    );
    EXPECT_EQ(deletionQueue.getPendingEntryCount(), 3u);

    const VulkanSyncPointRef deleteSp = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "DeferredDelete");
    deletionQueue.releaseResources(false, deleteSp, 1u);
    EXPECT_EQ(deletionQueue.getPendingEntryCount(), 0u);
    EXPECT_EQ(deletionQueue.getInFlightBatchCount(), 1u);
    EXPECT_FALSE(deferredCallbackRan);

    EXPECT_EQ(deletionQueue.processCompletedBatches(false), 0u);
    EXPECT_FALSE(deferredCallbackRan);

    deleteSp->signal();
    EXPECT_EQ(deletionQueue.processCompletedBatches(false), 1u);
    EXPECT_TRUE(deferredCallbackRan);
    EXPECT_EQ(deletionQueue.getInFlightBatchCount(), 0u);

    // Deferring 8+ pooled allocations across distinct pool allocators + enqueueDeviceAllocation().
    const VkBufferUsageFlags uboUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkMemoryPropertyFlags hostFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (gp::UInt32 idx = 0u; idx < 8u; ++idx)
    {
        VulkanAllocation pooledAlloc{};
        ASSERT_TRUE(m_memoryManager->allocateBufferPooled(
            pooledAlloc, 256u, 256u, uboUsage, hostFlags, VulkanAllocationMetaType::UniformBuffer
        ));
        pooledAlloc.free(*m_memoryManager, VulkanFreeFlags::None);
    }

    VulkanDeviceMemoryAllocation* rawAlloc =
        m_deviceMemoryManager->alloc(true, 4096u, 0xFFFFFFFFu, hostFlags, nullptr, kVulkanMemoryMediumPriority, false);
    ASSERT_NE(rawAlloc, nullptr);
    deletionQueue.enqueueDeviceAllocation(rawAlloc);

    // Direct pool allocator owner & non-empty releaseEmptyPools(forceFree=true) safety.
    VulkanAllocation liveOwnedAlloc{};
    ASSERT_TRUE(m_memoryManager->allocateBufferPooled(
        liveOwnedAlloc, 256u, 256u, uboUsage, hostFlags, VulkanAllocationMetaType::UniformBuffer
    ));
    VulkanPoolAllocator* const uboPoolAlloc = m_memoryManager->getPoolAllocator(liveOwnedAlloc.allocatorIndex);
    ASSERT_NE(uboPoolAlloc, nullptr);
    EXPECT_EQ(m_memoryManager->getPoolAllocator(0xFFFEu), nullptr);
    EXPECT_NE(uboPoolAlloc->getEffectiveMemoryTypeIndex(), 0xFFFFFFFFu);
    EXPECT_NE(uboPoolAlloc->getPool(liveOwnedAlloc), nullptr);
    EXPECT_GE(uboPoolAlloc->getLiveBlockCount(), 1u);

    gp::rhi::allocator::PoolResource ownerA{};
    gp::rhi::allocator::PoolResource ownerB{};
    uboPoolAlloc->setAllocationOwner(liveOwnedAlloc, &ownerA);
    uboPoolAlloc->transferAllocationOwner(liveOwnedAlloc, &ownerB);
    uboPoolAlloc->clearAllocationOwner(liveOwnedAlloc);

    // Force-freeing empty pools while liveOwnedAlloc is active must preserve its pool.
    uboPoolAlloc->releaseEmptyPools(0u, true, 100u);
    EXPECT_NE(uboPoolAlloc->getPool(liveOwnedAlloc), nullptr);
    liveOwnedAlloc.free(*m_memoryManager, VulkanFreeFlags::None);

    // Buffer alignment, eviction threshold, and VK_WHOLE_SIZE flush/invalidate.
    EXPECT_GE(VulkanMemoryManager::calculateBufferAlignment(*m_device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), 16u);
    [[maybe_unused]] const bool evicting = m_memoryManager->updateEvictThreshold();

    deletionQueue.releaseResources(true, nullptr, 2u);
    EXPECT_EQ(deletionQueue.getPendingEntryCount(), 0u);
    EXPECT_EQ(deletionQueue.getInFlightBatchCount(), 0u);

    m_memoryManager->setDeferredDeletionQueue(nullptr);
}

}   // namespace gp::rhi::vulkan::tests
