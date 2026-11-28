// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBufferInitializer.hpp"
#include "commands/CommandListBase.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/BufferInitializerHelpers.hpp"
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "VulkanCommon.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reference-counted handle to a Vulkan buffer.
using VulkanBufferRef = gp::RefCountedPtr<VulkanBuffer>;

/// @brief Builds the initializer of a zero-filled buffer.
/// @details Host-visible memory is cleared right away. Device-local memory is cleared by a `vkCmdFillBuffer()` on the
///          upload context, recorded when the initializer is finalized.
/// @param[in,out] cmdList Command list the deferred work is recorded on.
/// @param[in,out] buffer Newly created buffer.
/// @param[in] createDesc Creation description.
/// @return Initializer owning a reference to `buffer`.
[[nodiscard]] resource::BufferInitializer createZeroedBufferInitializer(
    command::CommandListBase& cmdList, VulkanBuffer* const buffer, const resource::BufferCreateDesc& createDesc
) noexcept
{
    void* const hostPointer = buffer->getCurrentHostPointer();
    if (hostPointer != nullptr)
    {
        gp::memory::zeroMemory(hostPointer, createDesc.size);
        buffer->registerPoolOwner(cmdList);
        return resource::DefaultBufferInitializer(cmdList, buffer);
    }

    return resource::CustomBufferInitializer(
        cmdList,
        buffer,
        nullptr,
        createDesc.size,
        [heldBuffer =
             VulkanBufferRef(buffer)](command::CommandListBase& finalizeCmdList) mutable noexcept -> resource::BufferRef
    {
        IVulkanCommandContextProvider* const provider = heldBuffer->getServices().contextProvider;
        GP_CHECKF(provider != nullptr, "Zero-initializing a device-local buffer requires a command context provider.");

        IVulkanTransferCommandRecorder* const recorder = provider->getUploadRecorder(finalizeCmdList);
        GP_CHECK(recorder != nullptr);

        recorder->fillBuffer(heldBuffer->getHandle(), heldBuffer->getOffset(), heldBuffer->getCurrentSize(), 0u);

        VkMemoryBarrier barrierAfter = zeroVulkanStruct<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
        barrierAfter.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierAfter.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        recorder->memoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, barrierAfter);

        heldBuffer->registerPoolOwner(finalizeCmdList);
        return resource::BufferRef(std::move(heldBuffer));
    }
    );
}

}   // namespace

resource::BufferInitializer createVulkanBufferInitializer(
    const VulkanResourceServices& services,
    command::CommandListBase& cmdList,
    const resource::BufferCreateDesc& createDesc
) noexcept
{
    VulkanBuffer* const buffer = new VulkanBuffer(services, createDesc);

    if (createDesc.isNull() || createDesc.initAction == resource::BufferInitAction::Default)
    {
        buffer->registerPoolOwner(cmdList);
        return resource::DefaultBufferInitializer(cmdList, buffer);
    }

    if (createDesc.initAction == resource::BufferInitAction::Zeroed)
    {
        return createZeroedBufferInitializer(cmdList, buffer, createDesc);
    }

    // Host-visible buffers are written in place. The write counts as the first lock, so the next lock renames
    // instead of overwriting memory the GPU may be reading. Device-local buffers go through a staging lock.
    void* writableData = nullptr;
    bool isUsingLock = false;
    void* const hostPointer = buffer->getCurrentHostPointer();
    if (hostPointer != nullptr)
    {
        writableData = hostPointer;
        buffer->incrementLockCounter();
    }
    else
    {
        writableData = buffer->lock(cmdList, resource::ResourceLockMode::WriteOnly, createDesc.size, 0u);
        isUsingLock = true;
    }

    if (createDesc.initAction == resource::BufferInitAction::ResourceArray)
    {
        GP_CHECKF(createDesc.initialData != nullptr, "ResourceArray initialization requires initial data.");
        gp::memory::copyMemory(
            writableData, createDesc.initialData->getResourceData(), createDesc.initialData->getResourceDataSize()
        );
        createDesc.initialData->discard();

        if (isUsingLock)
        {
            buffer->unlock(cmdList);
        }
        buffer->registerPoolOwner(cmdList);
        return resource::DefaultBufferInitializer(cmdList, buffer);
    }

    if (createDesc.initAction == resource::BufferInitAction::Initializer)
    {
        return resource::CustomBufferInitializer(
            cmdList,
            buffer,
            writableData,
            createDesc.size,
            [heldBuffer = VulkanBufferRef(buffer),
             isUsingLock](command::CommandListBase& finalizeCmdList) mutable noexcept -> resource::BufferRef
        {
            if (isUsingLock)
            {
                heldBuffer->unlock(finalizeCmdList);
            }
            heldBuffer->registerPoolOwner(finalizeCmdList);
            return resource::BufferRef(std::move(heldBuffer));
        }
        );
    }

    return resource::handleUnknownBufferInitializerInitAction(cmdList, createDesc);
}

}   // namespace gp::rhi::vulkan

