// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/PoolMoveRequest.hpp"
#include "allocators/PoolResource.hpp"
#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "memory/VulkanAllocation.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/VulkanBufferEnums.hpp"
#include "resources/VulkanBufferTypes.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "resources/VulkanViewableResource.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Vulkan implementation of a GPU buffer.
/// @details The buffer is a view into a range of memory, not necessarily a `VkBuffer` of its own:
///          - Regular buffers are sub-allocated from large pooled `VkBuffer`s; `getHandle()` returns the pool buffer
///          and
///            `getOffset()` the start of the range inside it.
///          - Acceleration structure buffers are "placed": they own a `VkBuffer` bound to their own memory range, and
///            `getOffset()` is always `0`.
///          - Reserved (sparse) buffers own a sparse `VkBuffer` whose pages are committed on demand with
///            `commitReservedResource()`.
///
///          Memory placement follows the usage:
///          - Static buffers live in device-local memory and are written through staging buffers.
///          - Dynamic and volatile buffers live in host-visible memory and are mapped persistently. Writing again
///            after the first lock "renames" the buffer: a new range is allocated, the CPU writes into it, and the
///            swap happens on the RHI timeline so in-flight GPU reads keep seeing the old data.
///          - On unified memory every buffer is host visible.
///
///          Whenever the backing range changes (rename, `takeOwnership()`, defragmentation), linked views and rename
///          listeners are refreshed through `updateLinkedViews()`.
/// @note `lock()`/`unlock()` run at the top of the pipe; every native command is deferred to the RHI timeline
///       through the command list.
class GP_RHI_VULKAN_API VulkanBuffer final
    : public resource::Buffer
    , public allocator::PoolResource
    , public VulkanViewableResource
{
public:
    /// @brief Creates the buffer and allocates its memory.
    /// @details A zero-size or null description creates a buffer with no memory. Reserved buffers create a sparse
    ///          `VkBuffer` and commit no memory.
    /// @param[in] services Device, memory and context services; copied into the buffer.
    /// @param[in] createDesc Creation description.
    VulkanBuffer(const VulkanResourceServices& services, const resource::BufferCreateDesc& createDesc) noexcept;

    /// @brief Returns the native buffer holding the data (the pool buffer for sub-allocated buffers).
    /// @return Buffer handle, or `VK_NULL_HANDLE` when no memory is bound.
    [[nodiscard]] VkBuffer getHandle() const noexcept
    {
        return m_currentBufferAlloc.alloc.getBufferHandle();
    }

    /// @brief Returns the byte offset of the data inside `getHandle()`.
    /// @return Offset of the range, or `0` for placed buffers.
    [[nodiscard]] gp::UInt32 getOffset() const noexcept
    {
        return m_currentBufferAlloc.isPlacedResource ? 0u : m_currentBufferAlloc.alloc.offset;
    }

    /// @brief Returns the size in bytes of the current allocation (the requested size rounded up to 4 bytes).
    /// @return Allocated byte count.
    [[nodiscard]] gp::UInt32 getCurrentSize() const noexcept
    {
        return m_currentBufferAlloc.alloc.size;
    }

    /// @brief Returns the persistently mapped pointer of the current allocation.
    /// @return Host pointer, or `nullptr` for device-local memory.
    [[nodiscard]] void* getCurrentHostPointer() const noexcept
    {
        return m_currentBufferAlloc.hostPtr;
    }

    /// @brief Returns the GPU virtual address of the first byte.
    /// @return Device address, or `0` without buffer device address support.
    [[nodiscard]] VkDeviceAddress getDeviceAddress() const noexcept
    {
        return m_currentBufferAlloc.deviceAddress;
    }

    /// @brief Returns the native usage flags the buffer was created with.
    /// @return Translated `VkBufferUsageFlags`.
    [[nodiscard]] VkBufferUsageFlags getBufferUsageFlags() const noexcept
    {
        return m_bufferUsageFlags;
    }

    /// @brief Returns the index type matching the buffer stride.
    /// @return `VK_INDEX_TYPE_UINT32` for a 4-byte stride, `VK_INDEX_TYPE_UINT16` otherwise.
    [[nodiscard]] VkIndexType getIndexType() const noexcept
    {
        return (getStride() == 4u) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    }

    /// @brief Returns the current allocation record.
    /// @return Allocation, host pointer and device address.
    [[nodiscard]] const VulkanBufferAlloc& getCurrentBufferAlloc() const noexcept
    {
        return m_currentBufferAlloc;
    }

    /// @brief Reports whether the buffer owns a `VkBuffer` bound to its own memory (acceleration structures).
    /// @return True for placed buffers.
    [[nodiscard]] bool isPlacedResource() const noexcept
    {
        return m_currentBufferAlloc.isPlacedResource;
    }

    /// @brief Reports whether the buffer is a reserved (sparse) buffer.
    /// @return True when pages are committed on demand.
    [[nodiscard]] bool isReservedResource() const noexcept
    {
        return m_reservedResourceData.isValid();
    }

    /// @brief Returns the sparse bookkeeping of a reserved buffer.
    /// @return Reserved data, or `nullptr` for regular buffers.
    [[nodiscard]] const VulkanReservedResourceData* getReservedResourceData() const noexcept
    {
        return m_reservedResourceData.get();
    }

    /// @brief Returns the current lock state.
    /// @return Lock status.
    [[nodiscard]] VulkanBufferLockStatus getLockStatus() const noexcept
    {
        return m_lockStatus;
    }

    /// @brief Returns how many times the buffer has been locked since creation.
    /// @return Lock count; `0` means the next lock is the first one.
    [[nodiscard]] gp::UInt32 getLockCounter() const noexcept
    {
        return m_lockCounter;
    }

    /// @brief Counts an initial write done through the host pointer as a lock.
    /// @details Initializers that write straight into persistently mapped memory call this so the next lock is not
    ///          treated as the first one and renames instead of overwriting data the GPU may read.
    void incrementLockCounter() noexcept
    {
        ++m_lockCounter;
    }

    /// @brief Maps a range of the buffer for CPU access.
    /// @details Picks the cheapest strategy for the usage and lock mode:
    ///          - `ReadOnly` on unified memory returns the persistent mapping. Otherwise it copies the range into a
    ///            readback staging buffer and blocks until the GPU is idle.
    ///          - Writes return the persistent mapping for host-visible buffers on the first lock or with
    ///            `WriteOnlyNoOverwrite`.
    ///          - Static buffers (unless the whole buffer is written on unified memory), reserved buffers, and every
    ///            buffer when `r.Vulkan.ForceStagingBufferOnLock` is set, write into a staging buffer copied on
    ///            `unlock()`.
    ///          - Any other write renames the buffer to a fresh allocation.
    /// @note Top of pipe. The buffer must be unlocked.
    /// @param[in,out] cmdList Command list the deferred work is recorded on.
    /// @param[in] lockMode Access requested.
    /// @param[in] lockSize Byte count to map.
    /// @param[in] offset Byte offset of the range inside the buffer.
    /// @return Pointer to the first byte of the range.
    [[nodiscard]] void* lock(
        command::CommandListBase& cmdList, resource::ResourceLockMode lockMode, gp::UInt32 lockSize, gp::UInt32 offset
    ) noexcept;

    /// @brief Ends a lock started by `lock()`.
    /// @details Persistent mappings need no work. Staging locks enqueue the copy into the buffer (writes) or the
    ///          release of the readback staging buffer (reads).
    /// @note Top of pipe. The buffer must be locked.
    /// @param[in,out] cmdList Command list the deferred work is recorded on.
    void unlock(command::CommandListBase& cmdList) noexcept;

    /// @brief Moves the allocation of `other` into this buffer, releasing the current one first.
    /// @details `other` is left without memory. Used by `replaceResources()`.
    /// @note Bottom of pipe. Both buffers must be unlocked.
    /// @param[in,out] other Buffer whose memory is taken.
    void takeOwnership(VulkanBuffer& other) noexcept;

    /// @brief Frees the current allocation and resets the description.
    /// @note Bottom of pipe. The buffer must be unlocked.
    void releaseOwnership() noexcept;

    /// @brief Registers this buffer as the owner of its pool range, so defragmentation can move it.
    /// @details Deferred to the RHI timeline, where pool ownership is tracked. Does nothing for buffers that are not
    ///          pool allocated.
    /// @param[in,out] cmdList Command list the registration is recorded on.
    void registerPoolOwner(command::CommandListBase& cmdList) noexcept;

    /// @brief Pins the current pool range so defragmentation never moves it.
    void makeAllocationUnmovable() noexcept;

    /// @brief Copies the data to the range picked by the defragmenter and switches to it.
    /// @details Declines when the move cannot be done safely: placed or reserved buffers, persistently mapped
    ///          buffers, no graphics context or a render pass open, or async compute still using the buffer.
    /// @note Bottom of pipe. Records on the graphics context of `request.contexts`.
    /// @param[in] request Destination range and active contexts.
    /// @return True when the copy was recorded and the buffer now points at the new range.
    [[nodiscard]] bool onAllocationMoved(const allocator::PoolMoveRequest& request) noexcept;

    /// @brief Commits or decommits pages so the first `requiredCommitSizeInBytes` bytes are backed by memory.
    /// @details Memory is allocated in blocks of `r.Vulkan.SparseBufferAllocSizeMB` megabytes and bound one tile at a
    ///          time. Shrinking releases whole blocks that no tile uses anymore. The returned binds must be submitted
    ///          with `vkQueueBindSparse()` by the caller.
    /// @note Reserved buffers only.
    /// @param[in] requiredCommitSizeInBytes Byte count that must be resident; at most `getSize()`.
    /// @return Tile binds to submit (unbinds have a null memory handle).
    [[nodiscard]] gp::Vector<VkSparseMemoryBind> commitReservedResource(gp::UInt64 requiredCommitSizeInBytes) noexcept;

    /// @brief Returns the services the buffer was created with.
    /// @return Device, memory and context services.
    [[nodiscard]] const VulkanResourceServices& getServices() const noexcept
    {
        return m_services;
    }

protected:
    /// @brief Releases the memory; reserved buffers also release their sparse handle and committed blocks.
    virtual ~VulkanBuffer() noexcept override;

private:
    /// @brief Allocates memory matching the buffer usage into `outBufferAlloc`.
    /// @details Placed buffers get their own `VkBuffer` bound to dedicated memory. Other buffers get a range of a
    ///          pooled buffer, device-local, host-visible or both depending on the usage and unified memory.
    /// @param[out] outBufferAlloc Receives the allocation, mapped pointer and device address.
    void allocateMemory(VulkanBufferAlloc& outBufferAlloc) noexcept;

    /// @brief Creates the sparse `VkBuffer` and the bookkeeping of a reserved buffer.
    void createReservedResource() noexcept;

    VulkanResourceServices m_services{};
    VulkanBufferAlloc m_currentBufferAlloc{};
    gp::UniquePtr<VulkanReservedResourceData> m_reservedResourceData{};
    VkBufferUsageFlags m_bufferUsageFlags{ 0u };
    gp::UInt32 m_lockCounter{ 0u };
    VulkanBufferLockStatus m_lockStatus{ VulkanBufferLockStatus::Unlocked };
};

}   // namespace gp::rhi::vulkan

