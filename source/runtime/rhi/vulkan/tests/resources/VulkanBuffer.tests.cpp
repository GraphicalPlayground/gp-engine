// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBuffer.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/IVulkanResourceRenameListener.hpp"
#include "resources/VulkanBufferEnums.hpp"
#include "resources/VulkanBufferLocking.hpp"
#include "resources/VulkanBufferTypes.hpp"
#include "resources/VulkanLinkedView.hpp"
#include "resources/VulkanPendingBufferLocks.hpp"
#include "sync/SyncEnums.hpp"
#include "utils/VulkanDebugBarrier.hpp"
#include "VulkanResourceTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

using VulkanBufferRef = gp::RefCountedPtr<VulkanBuffer>;

constexpr gp::UInt32 kBufferSize = 256u;

/// @brief Linked view that counts refreshes, standing in for an SRV/UAV on the buffer.
class CountingView final : public VulkanLinkedView
{
public:
    CountingView() noexcept = default;
    virtual ~CountingView() noexcept override = default;

    virtual void updateView([[maybe_unused]] const allocator::ContextArray& contexts) noexcept override
    {
        ++updateCount;
    }

    gp::Int32 updateCount{ 0 };
};

/// @brief Rename listener that counts notifications.
class CountingListener final : public IVulkanResourceRenameListener
{
public:
    virtual void resourceRenamed(
        [[maybe_unused]] const allocator::ContextArray& contexts, VulkanViewableResource* const renamedResource
    ) noexcept override
    {
        lastResource = renamedResource;
        ++renameCount;
    }

    VulkanViewableResource* lastResource{ nullptr };
    gp::Int32 renameCount{ 0 };
};

}   // namespace

class VulkanBufferLiveTest : public VulkanResourceLiveTest
{
protected:
    [[nodiscard]] VulkanBufferRef makeBuffer(
        const resource::BufferUsageFlags usage, const gp::UInt32 size = kBufferSize, const gp::UInt32 stride = 0u
    ) noexcept
    {
        const resource::BufferCreateDesc desc = resource::BufferCreateDesc::create("TestBuffer", size, stride, usage);
        return VulkanBufferRef(new VulkanBuffer(m_services, desc));
    }
};

TEST_F(VulkanBufferLiveTest, StaticBufferIsDeviceLocalAndUnlocked)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);

    EXPECT_NE(buffer->getHandle(), VK_NULL_HANDLE);
    EXPECT_GE(buffer->getCurrentSize(), kBufferSize);
    EXPECT_NE(buffer->getBufferUsageFlags() & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 0u);
    EXPECT_NE(buffer->getBufferUsageFlags() & VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0u);
    EXPECT_FALSE(buffer->isPlacedResource());
    EXPECT_FALSE(buffer->isReservedResource());
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::Unlocked);
    EXPECT_EQ(buffer->getLockCounter(), 0u);
    EXPECT_EQ(buffer->getOffset(), buffer->getCurrentBufferAlloc().alloc.offset);

    if (isUnifiedMemory())
    {
        EXPECT_NE(buffer->getCurrentHostPointer(), nullptr);
    }
    else
    {
        EXPECT_EQ(buffer->getCurrentHostPointer(), nullptr);
    }
}

TEST_F(VulkanBufferLiveTest, DynamicBufferIsHostVisible)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);

    EXPECT_NE(buffer->getHandle(), VK_NULL_HANDLE);
    EXPECT_NE(buffer->getCurrentHostPointer(), nullptr);
}

TEST_F(VulkanBufferLiveTest, ZeroSizeBufferOwnsNoMemory)
{
    const VulkanBufferRef buffer = makeBuffer(resource::BufferUsageFlags::VertexBuffer, 0u);

    EXPECT_EQ(buffer->getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(buffer->getCurrentSize(), 0u);
    EXPECT_EQ(buffer->getCurrentHostPointer(), nullptr);
    EXPECT_EQ(buffer->getDeviceAddress(), 0u);
}

TEST_F(VulkanBufferLiveTest, DeviceAddressOnlyWithBufferDeviceAddressUsage)
{
    const VulkanBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::StructuredBuffer | resource::BufferUsageFlags::UnorderedAccess, kBufferSize, 16u
    );

    const bool hasAddressUsage = (buffer->getBufferUsageFlags() & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0u;
    if (buffer->getDeviceAddress() != 0u)
    {
        EXPECT_TRUE(hasAddressUsage);
    }
    if (m_device->getOptionalExtensions().hasBufferDeviceAddress && hasAddressUsage)
    {
        EXPECT_NE(buffer->getDeviceAddress(), 0u);
    }
}

TEST_F(VulkanBufferLiveTest, IndexTypeFollowsStride)
{
    const VulkanBufferRef indices16 = makeBuffer(resource::BufferUsageFlags::IndexBuffer, kBufferSize, 2u);
    const VulkanBufferRef indices32 = makeBuffer(resource::BufferUsageFlags::IndexBuffer, kBufferSize, 4u);

    EXPECT_EQ(indices16->getIndexType(), VK_INDEX_TYPE_UINT16);
    EXPECT_EQ(indices32->getIndexType(), VK_INDEX_TYPE_UINT32);
}

TEST_F(VulkanBufferLiveTest, DynamicFirstWriteLockMapsInPlace)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    gp::UInt8* const hostPointer = static_cast<gp::UInt8*>(buffer->getCurrentHostPointer());
    ASSERT_NE(hostPointer, nullptr);

    void* const data = buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, 64u, 32u);

    EXPECT_EQ(data, hostPointer + 32u);
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::PersistentMapping);
    EXPECT_EQ(buffer->getLockCounter(), 1u);
    EXPECT_FALSE(getVulkanPendingBufferLocks().contains(buffer.get()));

    buffer->unlock(cmdList());

    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::Unlocked);
    EXPECT_EQ(m_provider.uploadRecorder.totalCommandCount(), 0u);
    EXPECT_EQ(m_provider.commandListRecorder.totalCommandCount(), 0u);
}

TEST_F(VulkanBufferLiveTest, DynamicSecondWriteLockRenamesAndRefreshesViews)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    CountingView view{};
    CountingListener listener{};
    buffer->linkView(view);
    buffer->addRenameListener(&listener);

    static_cast<void>(buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, kBufferSize, 0u));
    buffer->unlock(cmdList());
    EXPECT_EQ(view.updateCount, 0);

    // Copying the allocation would create a second owner, so only its identity is kept.
    const gp::UInt32 previousOffset = buffer->getCurrentBufferAlloc().alloc.offset;
    const gp::UInt64 previousHandle = buffer->getCurrentBufferAlloc().alloc.vulkanHandle;
    void* const data = buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, kBufferSize, 0u);

    // Bypass mode runs the rename inline, so the new range is already current.
    EXPECT_EQ(data, buffer->getCurrentHostPointer());
    const VulkanAllocation& currentAlloc = buffer->getCurrentBufferAlloc().alloc;
    EXPECT_TRUE((currentAlloc.offset != previousOffset) || (currentAlloc.vulkanHandle != previousHandle));
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::PersistentMapping);
    EXPECT_EQ(view.updateCount, 1);
    EXPECT_EQ(listener.renameCount, 1);
    EXPECT_EQ(listener.lastResource, static_cast<VulkanViewableResource*>(buffer.get()));

    buffer->unlock(cmdList());
    EXPECT_EQ(buffer->getLockCounter(), 2u);

    buffer->removeRenameListener(&listener);
    view.unlink();
}

TEST_F(VulkanBufferLiveTest, NoOverwriteLockNeverRenames)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    CountingView view{};
    buffer->linkView(view);

    static_cast<void>(buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, kBufferSize, 0u));
    buffer->unlock(cmdList());

    gp::UInt8* const hostPointer = static_cast<gp::UInt8*>(buffer->getCurrentHostPointer());
    void* const data = buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnlyNoOverwrite, 16u, 8u);

    EXPECT_EQ(data, hostPointer + 8u);
    EXPECT_EQ(buffer->getCurrentHostPointer(), hostPointer);
    EXPECT_EQ(view.updateCount, 0);

    buffer->unlock(cmdList());
    view.unlink();
}

TEST_F(VulkanBufferLiveTest, StaticFirstWriteGoesThroughStagingOnUploadRecorder)
{
    if (isUnifiedMemory())
    {
        GTEST_SKIP() << "Unified memory maps static buffers directly.";
    }

    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);

    void* const data = buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, 64u, 16u);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::Locked);
    EXPECT_TRUE(getVulkanPendingBufferLocks().contains(buffer.get()));

    buffer->unlock(cmdList());

    EXPECT_FALSE(getVulkanPendingBufferLocks().contains(buffer.get()));
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::Unlocked);

    const FakeTransferRecorder& upload = m_provider.uploadRecorder;
    ASSERT_EQ(upload.copyCount, 1u);
    EXPECT_EQ(upload.copies[0].dstBuffer, buffer->getHandle());
    EXPECT_NE(upload.copies[0].srcBuffer, VK_NULL_HANDLE);
    EXPECT_EQ(upload.copies[0].regionCount, 1u);
    EXPECT_EQ(upload.copies[0].region.srcOffset, 0u);
    EXPECT_EQ(upload.copies[0].region.dstOffset, 16u + static_cast<VkDeviceSize>(buffer->getOffset()));
    EXPECT_EQ(upload.copies[0].region.size, 64u);
    EXPECT_EQ(upload.barrierCount, 0u);
    EXPECT_EQ(upload.debugBarrierCount, 1u);
    EXPECT_EQ(upload.lastDebugCategory, kVulkanDebugBarrierBufferUpload);
    EXPECT_EQ(m_provider.commandListRecorder.totalCommandCount(), 0u);
}

TEST_F(VulkanBufferLiveTest, StaticLaterWriteStaysOnCommandListAndAddsBarrier)
{
    if (isUnifiedMemory())
    {
        GTEST_SKIP() << "Unified memory maps static buffers directly.";
    }

    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);

    static_cast<void>(buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, kBufferSize, 0u));
    buffer->unlock(cmdList());

    m_provider.needsTransitions = true;
    static_cast<void>(buffer->lock(cmdList(), resource::ResourceLockMode::WriteOnly, 32u, 0u));
    buffer->unlock(cmdList());

    EXPECT_EQ(m_provider.uploadRecorder.copyCount, 1u);

    const FakeTransferRecorder& graphics = m_provider.commandListRecorder;
    ASSERT_EQ(graphics.copyCount, 1u);
    EXPECT_EQ(graphics.copies[0].region.size, 32u);
    ASSERT_EQ(graphics.barrierCount, 1u);
    EXPECT_EQ(graphics.barriers[0].srcStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT));
    EXPECT_EQ(graphics.barriers[0].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
    EXPECT_EQ(graphics.barriers[0].srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT));
}

TEST_F(VulkanBufferLiveTest, ReadOnlyLockCopiesToStagingAndWaitsForGPU)
{
    if (isUnifiedMemory())
    {
        GTEST_SKIP() << "Unified memory reads buffers in place.";
    }

    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);

    void* const data = buffer->lock(cmdList(), resource::ResourceLockMode::ReadOnly, 128u, 64u);
    ASSERT_NE(data, nullptr);

    EXPECT_EQ(m_provider.submitCount, 1u);
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::Locked);
    EXPECT_TRUE(getVulkanPendingBufferLocks().contains(buffer.get()));

    const FakeTransferRecorder& graphics = m_provider.commandListRecorder;
    ASSERT_EQ(graphics.copyCount, 1u);
    EXPECT_EQ(graphics.copies[0].srcBuffer, buffer->getHandle());
    EXPECT_EQ(graphics.copies[0].region.srcOffset, 64u + static_cast<VkDeviceSize>(buffer->getOffset()));
    EXPECT_EQ(graphics.copies[0].region.dstOffset, 0u);
    EXPECT_EQ(graphics.copies[0].region.size, 128u);
    ASSERT_EQ(graphics.barrierCount, 2u);
    EXPECT_EQ(graphics.barriers[0].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT));
    EXPECT_EQ(graphics.barriers[1].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_HOST_BIT));
    EXPECT_EQ(graphics.barriers[1].dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_HOST_READ_BIT));

    buffer->unlock(cmdList());

    EXPECT_FALSE(getVulkanPendingBufferLocks().contains(buffer.get()));
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::Unlocked);
    EXPECT_EQ(graphics.copyCount, 1u);
}

TEST_F(VulkanBufferLiveTest, BottomOfPipeFreeFunctionsForwardToBuffer)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    gp::UInt8* const hostPointer = static_cast<gp::UInt8*>(buffer->getCurrentHostPointer());

    void* const data =
        lockVulkanBufferBottomOfPipe(cmdList(), buffer.get(), 4u, 8u, resource::ResourceLockMode::WriteOnly);
    EXPECT_EQ(data, hostPointer + 4u);
    unlockVulkanBufferBottomOfPipe(cmdList(), buffer.get());
    EXPECT_EQ(buffer->getLockStatus(), VulkanBufferLockStatus::Unlocked);

    void* const dataAgain =
        lockVulkanBuffer(cmdList(), buffer.get(), 0u, 8u, resource::ResourceLockMode::WriteOnlyNoOverwrite);
    EXPECT_EQ(dataAgain, buffer->getCurrentHostPointer());
    unlockVulkanBuffer(cmdList(), buffer.get());
    EXPECT_EQ(buffer->getLockCounter(), 2u);
}

TEST_F(VulkanBufferLiveTest, TakeOwnershipMovesAllocation)
{
    const VulkanBufferRef destination =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);
    const VulkanBufferRef source =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);

    const VkBuffer sourceHandle = source->getHandle();
    const gp::UInt32 sourceOffset = source->getOffset();
    const VkBufferUsageFlags sourceUsage = source->getBufferUsageFlags();

    destination->takeOwnership(*source);

    EXPECT_EQ(destination->getHandle(), sourceHandle);
    EXPECT_EQ(destination->getOffset(), sourceOffset);
    EXPECT_EQ(destination->getBufferUsageFlags(), sourceUsage);
    EXPECT_EQ(source->getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(source->getBufferUsageFlags(), 0u);
    EXPECT_FALSE(source->getCurrentBufferAlloc().alloc.hasAllocation());
}

TEST_F(VulkanBufferLiveTest, ReleaseOwnershipFreesAllocation)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);
    ASSERT_TRUE(buffer->getCurrentBufferAlloc().alloc.hasAllocation());

    buffer->releaseOwnership();

    EXPECT_FALSE(buffer->getCurrentBufferAlloc().alloc.hasAllocation());
}

TEST_F(VulkanBufferLiveTest, MakeAllocationUnmovableFlagsAllocation)
{
    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);
    EXPECT_EQ(buffer->getCurrentBufferAlloc().alloc.isUnmovable, 0u);

    buffer->makeAllocationUnmovable();

    EXPECT_NE(buffer->getCurrentBufferAlloc().alloc.isUnmovable, 0u);
}

TEST_F(VulkanBufferLiveTest, AllocationMoveIsDeclinedWhenUnsafe)
{
    const allocator::ContextArray contexts{};
    const allocator::PoolMoveRequest request{ contexts, nullptr };

    // Mapped memory may be held by the CPU.
    const VulkanBufferRef dynamicBuffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    ASSERT_NE(dynamicBuffer->getCurrentBufferAlloc().alloc.isPoolAllocated, 0u);
    EXPECT_FALSE(dynamicBuffer->onAllocationMoved(request));

    if (isUnifiedMemory())
    {
        return;
    }

    const VulkanBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);
    ASSERT_NE(buffer->getCurrentBufferAlloc().alloc.isPoolAllocated, 0u);

    // No context to record the copy on.
    m_provider.hasContextRecorder = false;
    EXPECT_FALSE(buffer->onAllocationMoved(request));
    m_provider.hasContextRecorder = true;

    // Transfers are illegal inside a render pass.
    m_provider.commandListRecorder.outsideRenderPass = false;
    EXPECT_FALSE(buffer->onAllocationMoved(request));
    m_provider.commandListRecorder.outsideRenderPass = true;

    // Async compute may still be reading the old range.
    m_provider.trackedPipelines = sync::PipelineStage::Graphics | sync::PipelineStage::AsyncCompute;
    EXPECT_FALSE(buffer->onAllocationMoved(request));

    EXPECT_EQ(m_provider.commandListRecorder.totalCommandCount(), 0u);
}

TEST_F(VulkanBufferLiveTest, ReservedBufferCommitsAndDecommitsTiles)
{
    const ReservedResourceCapabilities& reserved = getRHIGlobals().reservedResources;
    if (!reserved.supported || reserved.tileSizeInBytes == 0u)
    {
        GTEST_SKIP() << "Sparse buffer residency unsupported on this device.";
    }

    const gp::UInt64 tileSize = reserved.tileSizeInBytes;
    const gp::UInt32 reservedSize = static_cast<gp::UInt32>(tileSize * 8u);
    const VulkanBufferRef buffer = makeBuffer(
        resource::BufferUsageFlags::StructuredBuffer | resource::BufferUsageFlags::ReservedResource |
            resource::BufferUsageFlags::Static,
        reservedSize,
        16u
    );
    ASSERT_TRUE(buffer->isReservedResource());
    ASSERT_NE(buffer->getHandle(), VK_NULL_HANDLE);

    const gp::Vector<VkSparseMemoryBind> commitBinds = buffer->commitReservedResource(tileSize * 3u);
    ASSERT_EQ(commitBinds.size(), 3u);
    for (gp::USize index = 0u; index < commitBinds.size(); ++index)
    {
        EXPECT_NE(commitBinds[index].memory, VK_NULL_HANDLE);
        EXPECT_EQ(commitBinds[index].size, tileSize);
        EXPECT_EQ(commitBinds[index].resourceOffset, tileSize * index);
    }
    EXPECT_EQ(buffer->getReservedResourceData()->numCommittedTiles, 3u);

    const gp::Vector<VkSparseMemoryBind> sameBinds = buffer->commitReservedResource(tileSize * 3u);
    EXPECT_EQ(sameBinds.size(), 0u);

    const gp::Vector<VkSparseMemoryBind> decommitBinds = buffer->commitReservedResource(tileSize);
    ASSERT_EQ(decommitBinds.size(), 2u);
    EXPECT_EQ(decommitBinds[0].memory, VK_NULL_HANDLE);
    EXPECT_EQ(decommitBinds[0].resourceOffset, tileSize * 2u);
    EXPECT_EQ(decommitBinds[1].resourceOffset, tileSize);
    EXPECT_EQ(buffer->getReservedResourceData()->numCommittedTiles, 1u);
}

}   // namespace gp::rhi::vulkan::tests

