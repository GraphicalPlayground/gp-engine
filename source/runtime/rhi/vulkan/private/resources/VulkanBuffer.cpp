// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBuffer.hpp"
#include "allocators/PoolAllocationData.hpp"   // IWYU pragma: keep
#include "commands/CommandListBase.hpp"
#include "commands/CommandListImmediate.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanPoolAllocator.hpp"
#include "memory/VulkanStagingManager.hpp"
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/VulkanBufferStats.hpp"
#include "resources/VulkanBufferUsage.hpp"
#include "resources/VulkanPendingBufferLocks.hpp"
#include "RHIVulkan.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "utils/VulkanDebugBarrier.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanSparseBufferAllocSizeMB(
    "r.Vulkan.SparseBufferAllocSizeMB",
    16,
    "Size in megabytes of the memory blocks backing reserved (sparse) buffers. Must be a multiple of the sparse page " "size of the device.",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanForceStagingBufferOnLock(
    "r.Vulkan.ForceStagingBufferOnLock",
    0,
    "When nonzero, every buffer write lock goes through a staging buffer instead of renaming host-visible buffers.\n"
    " 0: Rename dynamic buffers on lock (default)\n"
    " 1: Always use a staging buffer\n",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Byte size of one memory block backing a reserved buffer.
/// @return `r.Vulkan.SparseBufferAllocSizeMB` in bytes.
[[nodiscard]] gp::UInt64 getSparseBlockSize() noexcept
{
    return static_cast<gp::UInt64>(g_cvarVulkanSparseBufferAllocSizeMB.getValueOnAnyThread()) * 1024u * 1024u;
}

/// @brief Builds a global memory barrier.
/// @param[in] srcAccessMask Writes to make available.
/// @param[in] dstAccessMask Accesses the writes must be visible to.
/// @return Filled barrier.
[[nodiscard]] VkMemoryBarrier
    makeMemoryBarrier(const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask) noexcept
{
    VkMemoryBarrier barrier = zeroVulkanStruct<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    return barrier;
}

/// @brief Returns the context provider of `services`, asserting that one is bound.
/// @param[in] services Buffer services.
/// @return Context provider.
[[nodiscard]] IVulkanCommandContextProvider& getContextProvider(const VulkanResourceServices& services) noexcept
{
    GP_CHECKF(services.contextProvider != nullptr, "Recording buffer commands requires a command context provider.");
    return *services.contextProvider;
}

}   // namespace

VulkanBuffer::VulkanBuffer(
    const VulkanResourceServices& services, const resource::BufferCreateDesc& createDesc
) noexcept
    : resource::Buffer(createDesc)
    , m_services(services)
{
    GP_CHECKF(m_services.isValid(), "VulkanBuffer requires a device, a memory manager and a staging manager.");

    const bool isZeroSize = (createDesc.size == 0u);
    m_bufferUsageFlags = translateBufferUsageFlags(m_services.device->getOptionalExtensions(), getUsage(), isZeroSize);

    if (isZeroSize)
    {
        return;
    }

    if (hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::ReservedResource))
    {
        createReservedResource();
    }
    else
    {
        allocateMemory(m_currentBufferAlloc);
    }
}

VulkanBuffer::~VulkanBuffer() noexcept
{
    if (m_reservedResourceData.isValid())
    {
        updateVulkanBufferStats(getUsage(), m_currentBufferAlloc.alloc.size, false);
        m_services.enqueueBufferDeletion(getHandle());

        // Freeing the blocks is enough: the sparse buffer is destroyed, so the unbinds are never submitted.
        static_cast<void>(commitReservedResource(0u));
    }

    releaseOwnership();
}

void VulkanBuffer::createReservedResource() noexcept
{
    GP_CHECKF(
        !isDynamicBufferUsage(getUsage()),
        "The ReservedResource flag is incompatible with dynamic and volatile buffers."
    );

    const gp::UInt64 sparseBlockSize = getSparseBlockSize();
    const gp::UInt64 tileSize = getRHIGlobals().reservedResources.tileSizeInBytes;
    const gp::UInt32 bufferSize = gp::memory::align(getSize(), tileSize);
    const VkBufferCreateFlags createFlags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;

    const VkBuffer bufferHandle = createVulkanBuffer(*m_services.device, bufferSize, m_bufferUsageFlags, createFlags);
    if (bufferHandle == VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to create a reserved buffer of {} bytes.", bufferSize);
        return;
    }

    m_currentBufferAlloc.alloc.size = bufferSize;
    m_currentBufferAlloc.alloc.vulkanHandle = reinterpret_cast<gp::UInt64>(bufferHandle);
    m_currentBufferAlloc.alloc.handleId = generateVulkanBufferHandleId();
    m_currentBufferAlloc.deviceAddress = getVulkanBufferDeviceAddress(*m_services.device, bufferHandle);

    m_reservedResourceData = gp::makeUnique<VulkanReservedResourceData>();
    VulkanReservedResourceData& reservedData = *m_reservedResourceData;
    reservedData.lastBlockSize = gp::memory::align(static_cast<gp::UInt64>(getSize()) % sparseBlockSize, tileSize);
    reservedData.blockCount = static_cast<gp::UInt32>(static_cast<gp::UInt64>(getSize()) / sparseBlockSize);
    if (reservedData.lastBlockSize == 0u)
    {
        reservedData.lastBlockSize = sparseBlockSize;
    }
    else
    {
        ++reservedData.blockCount;
    }

    vkGetBufferMemoryRequirements(m_services.device->getHandle(), bufferHandle, &reservedData.memoryRequirements);
    GP_CHECKF(
        (sparseBlockSize % reservedData.memoryRequirements.alignment) == 0u,
        "r.Vulkan.SparseBufferAllocSizeMB is not compatible with the sparse page size of this buffer."
    );
    GP_CHECKF(
        (reservedData.lastBlockSize % reservedData.memoryRequirements.alignment) == 0u,
        "The reserved resource tile size is not compatible with the sparse page size of this buffer."
    );

    updateVulkanBufferStats(getUsage(), bufferSize, true);
}

void VulkanBuffer::allocateMemory(VulkanBufferAlloc& outBufferAlloc) noexcept
{
    VulkanMemoryManager& memoryManager = *m_services.memoryManager;
    const bool isUnifiedMemory = m_services.hasUnifiedMemory();
    const bool isDynamic = isDynamicBufferUsage(getUsage());

    // The size stays a multiple of 4 so the whole range can be cleared with vkCmdFillBuffer().
    const gp::UInt32 bufferSize = gp::memory::align(getSize(), 4u);
    const gp::UInt32 bufferAlignment =
        VulkanMemoryManager::calculateBufferAlignment(*m_services.device, m_bufferUsageFlags);

    if (hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::AccelerationStructure))
    {
        outBufferAlloc.isPlacedResource = true;

        const VkBuffer bufferHandle = createVulkanBuffer(*m_services.device, bufferSize, m_bufferUsageFlags);
        const VulkanAllocationFlags allocFlags =
            VulkanAllocationFlags::AutoBind |
            (isDynamic ? VulkanAllocationFlags::HostVisible : VulkanAllocationFlags::None);
        const gp::Name name = getName();
        const char* const debugName = name.isNone() ? nullptr : name.baseCString();

        if (bufferHandle == VK_NULL_HANDLE ||
            !memoryManager.allocateBufferMemory(
                outBufferAlloc.alloc, bufferHandle, allocFlags, debugName, bufferAlignment
            ))
        {
            GP_LOG_ERROR(LogVulkanRHI, "Out of GPU memory allocating a placed buffer of {} bytes.", bufferSize);
        }

        outBufferAlloc.alloc.vulkanHandle = reinterpret_cast<gp::UInt64>(bufferHandle);
        outBufferAlloc.alloc.handleId = generateVulkanBufferHandleId();
    }
    else
    {
        VkMemoryPropertyFlags memoryFlags = 0u;
        if (isUnifiedMemory)
        {
            memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }
        else if (isDynamic)
        {
            memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }
        else
        {
            memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        if (!memoryManager.allocateBufferPooled(
                outBufferAlloc.alloc,
                bufferSize,
                bufferAlignment,
                m_bufferUsageFlags,
                memoryFlags,
                VulkanAllocationMetaType::MultiBuffer
            ))
        {
            GP_LOG_ERROR(LogVulkanRHI, "Out of GPU memory allocating a pooled buffer of {} bytes.", bufferSize);
        }
    }

    outBufferAlloc.hostPtr =
        (isUnifiedMemory || isDynamic) ? outBufferAlloc.alloc.getMappedPointer(memoryManager) : nullptr;

    const gp::UInt32 bufferOffset = outBufferAlloc.isPlacedResource ? 0u : outBufferAlloc.alloc.offset;
    const VkDeviceAddress baseAddress =
        getVulkanBufferDeviceAddress(*m_services.device, outBufferAlloc.alloc.getBufferHandle());
    outBufferAlloc.deviceAddress = (baseAddress != 0u) ? (baseAddress + bufferOffset) : 0u;

    updateVulkanBufferStats(getUsage(), outBufferAlloc.alloc.size, true);
}

gp::Vector<VkSparseMemoryBind> VulkanBuffer::commitReservedResource(const gp::UInt64 requiredCommitSizeInBytes) noexcept
{
    GP_CHECKF(requiredCommitSizeInBytes <= getSize(), "Commit size too large for the reserved buffer's maximum size.");
    GP_CHECKF(
        m_reservedResourceData.isValid(),
        "commitReservedResource() called on a buffer created without the ReservedResource flag."
    );

    VulkanReservedResourceData& reservedData = *m_reservedResourceData;
    VulkanMemoryManager& memoryManager = *m_services.memoryManager;

    // Block needs.
    const gp::UInt64 sparseBlockSize = getSparseBlockSize();
    const gp::UInt64 requiredBlockCount =
        gp::memory::align(requiredCommitSizeInBytes, sparseBlockSize) / sparseBlockSize;
    const gp::UInt64 currentBlockCount = reservedData.allocations.size();

    // Tile needs.
    const gp::UInt64 tileSize = getRHIGlobals().reservedResources.tileSizeInBytes;
    const gp::UInt64 requiredTileCount = gp::memory::align(requiredCommitSizeInBytes, tileSize) / tileSize;
    const gp::UInt64 currentTileCount = reservedData.numCommittedTiles;

    gp::Vector<VkSparseMemoryBind> binds{};
    if (requiredTileCount > currentTileCount)
    {
        // Allocate every block the new tiles land in.
        reservedData.allocations.reserve(static_cast<gp::USize>(requiredBlockCount));
        for (gp::UInt64 blockIndex = currentBlockCount; blockIndex < requiredBlockCount; ++blockIndex)
        {
            const gp::UInt64 blockSize = (blockIndex == static_cast<gp::UInt64>(reservedData.blockCount) - 1u)
                                             ? reservedData.lastBlockSize
                                             : sparseBlockSize;

            VulkanSparseBufferAllocation& newBlock = reservedData.allocations.emplaceBack();
            if (!memoryManager.allocateBufferPooled(
                    newBlock.allocation,
                    static_cast<gp::UInt32>(blockSize),
                    static_cast<gp::UInt32>(reservedData.memoryRequirements.alignment),
                    m_bufferUsageFlags,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    VulkanAllocationMetaType::MultiBuffer
                ))
            {
                GP_LOG_ERROR(
                    LogVulkanRHI, "Out of GPU memory committing a reserved buffer block of {} bytes.", blockSize
                );
            }
            newBlock.resourceOffset = blockIndex * sparseBlockSize;
        }

        // Bind the tiles one by one.
        binds.reserve(static_cast<gp::USize>(requiredTileCount - currentTileCount));
        for (gp::UInt64 tileIndex = currentTileCount; tileIndex < requiredTileCount; ++tileIndex)
        {
            const gp::UInt64 tileOffset = tileIndex * tileSize;
            const gp::USize allocIndex = static_cast<gp::USize>(tileOffset / sparseBlockSize);
            const VulkanSparseBufferAllocation& sparseAlloc = reservedData.allocations[allocIndex];

            VkSparseMemoryBind newBind{};
            newBind.size = tileSize;
            newBind.memory = sparseAlloc.allocation.getDeviceMemoryHandle(memoryManager);
            newBind.memoryOffset = sparseAlloc.allocation.offset + tileOffset - sparseAlloc.resourceOffset;
            newBind.resourceOffset = tileOffset;
            binds.pushBack(newBind);
        }

        updateVulkanReservedBufferStatsOnCommit(tileSize * binds.size(), true);
    }
    else if (requiredTileCount < currentTileCount)
    {
        // Unbind from the last tile down.
        binds.reserve(static_cast<gp::USize>(currentTileCount - requiredTileCount));
        for (gp::UInt64 tileIndex = currentTileCount; tileIndex > requiredTileCount; --tileIndex)
        {
            VkSparseMemoryBind newBind{};
            newBind.size = tileSize;
            newBind.memory = VK_NULL_HANDLE;
            newBind.memoryOffset = 0u;
            newBind.resourceOffset = (tileIndex - 1u) * tileSize;
            binds.pushBack(newBind);
        }

        // Release blocks no tile uses anymore. Frees are deferred, so the unbinds need not be submitted first.
        while (reservedData.allocations.size() > requiredBlockCount)
        {
            reservedData.allocations.back().allocation.free(memoryManager);
            reservedData.allocations.popBack();
        }

        updateVulkanReservedBufferStatsOnCommit(tileSize * binds.size(), false);
    }

    reservedData.numCommittedTiles = static_cast<gp::UInt32>(requiredTileCount);
    return binds;
}

void* VulkanBuffer::lock(
    command::CommandListBase& cmdList,
    const resource::ResourceLockMode lockMode,
    const gp::UInt32 lockSize,
    const gp::UInt32 offset
) noexcept
{
    void* data = nullptr;
    gp::UInt32 dataOffset = 0u;

    GP_CHECKF(m_lockStatus == VulkanBufferLockStatus::Unlocked, "Buffer is already locked.");

    m_lockStatus = VulkanBufferLockStatus::Locked;
    const bool isFirstLock = (m_lockCounter++ == 0u);

    // Dynamic: a new host-visible range is allocated and swapped in on the RHI timeline, then views are updated. The
    //          GPU reads host memory directly, so no copy is needed and the buffer stays usable inside render passes.
    // Static:  one device-local range is allocated at creation. Writes go to a staging buffer that is copied into the
    //          device-local memory on unlock.
    const bool isUnifiedMemory = m_services.hasUnifiedMemory();
    const bool isDynamic = isDynamicBufferUsage(getUsage());
    const bool isStatic = hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::Static) || !isDynamic;
    const bool isUAV = hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::UnorderedAccess);
    const bool isSRV = hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::ShaderResource);
    const bool isSparseResource = hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::ReservedResource);

    GP_CHECK(isStatic || isDynamic || isUAV || isSRV);

    if (lockMode == resource::ResourceLockMode::ReadOnly)
    {
        GP_CHECKF(lockSize <= getSize(), "Requested lock size is larger than the total size of the buffer.");

        if (isUnifiedMemory)
        {
            data = m_currentBufferAlloc.hostPtr;
            dataOffset = offset;
            m_lockStatus = VulkanBufferLockStatus::PersistentMapping;
        }
        else
        {
            VulkanStagingBuffer* stagingBuffer = m_services.stagingManager->acquireBuffer(
                lockSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT
            );
            GP_CHECKF(stagingBuffer != nullptr, "Failed to acquire a readback staging buffer.");

            command::CommandListImmediate& immCmdList = cmdList.getAsImmediate();
            immCmdList.enqueueLambda([buffer = this, stagingBuffer, lockSize, offset](
                                         command::CommandListImmediate& executingCmdList
                                     ) noexcept
            {
                IVulkanTransferCommandRecorder* const recorder =
                    getContextProvider(buffer->m_services).getCommandListRecorder(executingCmdList);
                GP_CHECK(recorder != nullptr);

                // Make prior writes available before the copy reads the buffer.
                recorder->memoryBarrier(
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT)
                );

                VkBufferCopy region{};
                region.size = lockSize;
                region.srcOffset = static_cast<VkDeviceSize>(offset) + buffer->getOffset();
                region.dstOffset = 0u;
                recorder->copyBuffer(
                    buffer->getHandle(), stagingBuffer->getHandle(), gp::ConstVectorView<VkBufferCopy>(&region, 1u)
                );

                // Make the copy visible to the host.
                recorder->memoryBarrier(
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT,
                    makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_HOST_READ_BIT)
                );
            });

            // The readback must be finished before the CPU reads the staging buffer.
            getContextProvider(m_services).submitAndBlockUntilGPUIdle(immCmdList);

            stagingBuffer->flushMappedMemory();
            data = stagingBuffer->getMappedPointer();

            VulkanPendingBufferLock pendingLock{};
            pendingLock.offset = 0u;
            pendingLock.size = lockSize;
            pendingLock.lockMode = lockMode;
            pendingLock.stagingBuffer = stagingBuffer;
            getVulkanPendingBufferLocks().add(this, pendingLock);
        }
    }
    else
    {
        GP_CHECK(
            (lockMode == resource::ResourceLockMode::WriteOnly) ||
            (lockMode == resource::ResourceLockMode::WriteOnlyNoOverwrite)
        );

        // Writing the whole buffer of a unified memory static buffer renames it instead of going through staging.
        const bool isEntireBuffer = isUnifiedMemory && (lockSize == getSize()) && (offset == 0u);
        const bool isHostVisible = isUnifiedMemory || isDynamic;

        if (isHostVisible && (isFirstLock || (lockMode == resource::ResourceLockMode::WriteOnlyNoOverwrite)))
        {
            // Nothing can be reading the memory yet (first lock), or the caller promised not to overwrite in-flight
            // data: write straight into the mapping.
            GP_CHECK(m_currentBufferAlloc.hostPtr != nullptr);
            data = m_currentBufferAlloc.hostPtr;
            dataOffset = offset;
            m_lockStatus = VulkanBufferLockStatus::PersistentMapping;
        }
        else if (
            (isStatic && !isEntireBuffer) || (g_cvarVulkanForceStagingBufferOnLock.getValueOnAnyThread() != 0) ||
            isSparseResource
        )
        {
            VulkanPendingBufferLock pendingLock{};
            pendingLock.offset = offset;
            pendingLock.size = lockSize;
            pendingLock.lockMode = lockMode;
            pendingLock.isFirstLock = isFirstLock;

            VulkanStagingBuffer* const stagingBuffer =
                m_services.stagingManager->acquireBuffer(lockSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            GP_CHECKF(stagingBuffer != nullptr, "Failed to acquire an upload staging buffer.");
            pendingLock.stagingBuffer = stagingBuffer;
            data = stagingBuffer->getMappedPointer();

            getVulkanPendingBufferLocks().add(this, pendingLock);
        }
        else
        {
            // Rename: write into a fresh range now, swap it in on the RHI timeline.
            VulkanBufferAlloc newAlloc{};
            allocateMemory(newAlloc);
            newAlloc.alloc.disown();

            cmdList.enqueueLambdaMultiPipe([buffer = this, newAlloc](const allocator::ContextArray& contexts) noexcept
            {
                VulkanMemoryManager& memoryManager = *buffer->m_services.memoryManager;

                updateVulkanBufferStats(buffer->getUsage(), buffer->m_currentBufferAlloc.alloc.size, false);
                if (buffer->isPlacedResource() && buffer->getHandle() != VK_NULL_HANDLE)
                {
                    buffer->m_services.enqueueBufferDeletion(buffer->getHandle());
                }
                buffer->m_currentBufferAlloc.alloc.free(memoryManager);

                buffer->m_currentBufferAlloc = newAlloc;
                buffer->m_currentBufferAlloc.alloc.own();

                buffer->updateLinkedViews(contexts);
                memoryManager.setPoolAllocationOwner(buffer->m_currentBufferAlloc.alloc, buffer);
            });

            data = newAlloc.hostPtr;
            dataOffset = offset;
            m_lockStatus = VulkanBufferLockStatus::PersistentMapping;
        }
    }

    GP_CHECK(data != nullptr);
    return static_cast<gp::UInt8*>(data) + dataOffset;
}

void VulkanBuffer::unlock(command::CommandListBase& cmdList) noexcept
{
    const bool isDynamic = isDynamicBufferUsage(getUsage());
    const bool isStatic = hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::Static) || !isDynamic;
    const bool isSRV = hasAnyBufferUsage(getUsage(), resource::BufferUsageFlags::ShaderResource);

    GP_CHECKF(m_lockStatus != VulkanBufferLockStatus::Unlocked, "Unlocking a buffer that is not locked.");

    if (m_lockStatus != VulkanBufferLockStatus::PersistentMapping)
    {
        GP_CHECK(isStatic || isDynamic || isSRV);

        VulkanPendingBufferLock pendingLock{};
        const bool isFound = getVulkanPendingBufferLocks().take(this, pendingLock);
        GP_CHECKF(isFound, "Mismatched buffer lock/unlock.");

        const bool needsTransitions = getContextProvider(m_services).needsExtraTransitions(cmdList);

        cmdList.enqueueLambda(
            [buffer = this, pendingLock, needsTransitions](command::CommandListBase& executingCmdList) noexcept
        {
            VulkanStagingBuffer* stagingBuffer = pendingLock.stagingBuffer;
            GP_CHECK(stagingBuffer != nullptr);
            stagingBuffer->flushMappedMemory();

            if (pendingLock.lockMode == resource::ResourceLockMode::ReadOnly)
            {
                buffer->m_services.stagingManager->releaseBuffer(VulkanSyncPointRef{}, stagingBuffer);
            }
            else if (pendingLock.lockMode == resource::ResourceLockMode::WriteOnly)
            {
                // A first write cannot race with GPU reads, so it goes on the upload context, which runs ahead of the
                // graphics work. Later writes must stay ordered with the command list.
                IVulkanCommandContextProvider& provider = getContextProvider(buffer->m_services);
                IVulkanTransferCommandRecorder* const recorder =
                    pendingLock.isFirstLock ? provider.getUploadRecorder(executingCmdList)
                                            : provider.getCommandListRecorder(executingCmdList);
                GP_CHECK(recorder != nullptr);
                GP_CHECK(recorder->isOutsideRenderPass());

                recorder->debugHeavyWeightBarrier(kVulkanDebugBarrierBufferUpload);

                VkBufferCopy region{};
                region.size = pendingLock.size;
                region.dstOffset = static_cast<VkDeviceSize>(pendingLock.offset) + buffer->getOffset();
                recorder->copyBuffer(
                    stagingBuffer->getHandle(),
                    buffer->m_currentBufferAlloc.alloc.getBufferHandle(),
                    gp::ConstVectorView<VkBufferCopy>(&region, 1u)
                );

                // High-level code expects the copy to be complete by the next command.
                if (needsTransitions)
                {
                    recorder->memoryBarrier(
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        makeMemoryBarrier(
                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT
                        )
                    );
                }

                buffer->m_services.stagingManager->releaseBuffer(recorder->getCompletionSyncPoint(), stagingBuffer);
            }
        }
        );
    }

    m_lockStatus = VulkanBufferLockStatus::Unlocked;
}

void VulkanBuffer::takeOwnership(VulkanBuffer& other) noexcept
{
    GP_CHECKF(other.m_lockStatus == VulkanBufferLockStatus::Unlocked, "Cannot take ownership of a locked buffer.");

    releaseOwnership();
    resource::Buffer::takeOwnership(other);

    m_bufferUsageFlags = other.m_bufferUsageFlags;
    m_currentBufferAlloc = other.m_currentBufferAlloc;
    m_services.memoryManager->transferPoolAllocationOwner(m_currentBufferAlloc.alloc, this);

    other.m_bufferUsageFlags = 0u;
    other.m_currentBufferAlloc = VulkanBufferAlloc{};
}

void VulkanBuffer::releaseOwnership() noexcept
{
    GP_CHECKF(m_lockStatus == VulkanBufferLockStatus::Unlocked, "Cannot release the memory of a locked buffer.");

    if (isPlacedResource() && getHandle() != VK_NULL_HANDLE)
    {
        m_services.enqueueBufferDeletion(getHandle());
        m_currentBufferAlloc.alloc.vulkanHandle = 0u;
    }

    if (m_currentBufferAlloc.alloc.hasAllocation())
    {
        updateVulkanBufferStats(getUsage(), m_currentBufferAlloc.alloc.size, false);
        m_services.memoryManager->freeVulkanAllocation(m_currentBufferAlloc.alloc);
    }

    resource::Buffer::releaseOwnership();
}

void VulkanBuffer::registerPoolOwner(command::CommandListBase& cmdList) noexcept
{
    if (m_currentBufferAlloc.alloc.isPoolAllocated == 0u)
    {
        return;
    }

    cmdList.enqueueLambda([buffer = gp::RefCountedPtr<VulkanBuffer>(this)](command::CommandListBase&) noexcept
    {
        buffer->m_services.memoryManager->setPoolAllocationOwner(buffer->m_currentBufferAlloc.alloc, buffer.get());
    });
}

void VulkanBuffer::makeAllocationUnmovable() noexcept
{
    m_services.memoryManager->setAllocationUnmovable(m_currentBufferAlloc.alloc);
}

bool VulkanBuffer::onAllocationMoved(const allocator::PoolMoveRequest& request) noexcept
{
    GP_CHECK(m_currentBufferAlloc.alloc.isPoolAllocated != 0u);

    // Placed and reserved buffers own their handle; there is nothing to move inside a pool.
    if (isPlacedResource() || m_reservedResourceData.isValid())
    {
        return false;
    }

    // The CPU may be holding the mapped pointer.
    if (m_currentBufferAlloc.hostPtr != nullptr)
    {
        return false;
    }

    GP_CHECK(m_currentBufferAlloc.alloc.isUnmovable == 0u);

    if (m_services.contextProvider == nullptr)
    {
        return false;
    }

    // Transfers are illegal inside a render pass.
    command::IComputeContext* const graphicsContext = request.contexts[sync::PipelineStage::Graphics];
    IVulkanTransferCommandRecorder* const recorder = m_services.contextProvider->getContextRecorder(graphicsContext);
    if (recorder == nullptr || !recorder->isOutsideRenderPass())
    {
        return false;
    }

    // Async compute may still read the old range.
    const sync::PipelineStage trackedPipelines = m_services.contextProvider->getTrackedPipelines(graphicsContext, this);
    if ((trackedPipelines & sync::PipelineStage::AsyncCompute) != sync::PipelineStage::None)
    {
        return false;
    }

    VulkanAllocation destAllocation{};
    m_services.memoryManager->initPoolAllocation(destAllocation, m_currentBufferAlloc.alloc, *request.newData);
    GP_CHECK(destAllocation.size >= m_currentBufferAlloc.alloc.size);

    recorder->memoryBarrier(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)
    );

    VkBufferCopy region{};
    region.srcOffset = m_currentBufferAlloc.alloc.offset;
    region.dstOffset = destAllocation.offset;
    region.size = m_currentBufferAlloc.alloc.size;
    recorder->copyBuffer(
        m_currentBufferAlloc.alloc.getBufferHandle(),
        destAllocation.getBufferHandle(),
        gp::ConstVectorView<VkBufferCopy>(&region, 1u)
    );

    recorder->memoryBarrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)
    );

    m_currentBufferAlloc.alloc.offset = destAllocation.offset;
    m_currentBufferAlloc.alloc.vulkanHandle = destAllocation.vulkanHandle;
    m_currentBufferAlloc.alloc.handleId = destAllocation.handleId;

    const VkDeviceAddress baseAddress =
        getVulkanBufferDeviceAddress(*m_services.device, m_currentBufferAlloc.alloc.getBufferHandle());
    m_currentBufferAlloc.deviceAddress = (baseAddress != 0u) ? (baseAddress + m_currentBufferAlloc.alloc.offset) : 0u;

    updateLinkedViews(request.contexts);
    return true;
}

}   // namespace gp::rhi::vulkan

