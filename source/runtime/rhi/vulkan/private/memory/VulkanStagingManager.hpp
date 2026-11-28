// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Host-visible staging `VkBuffer` used for CPU-to-GPU uploads and GPU-to-CPU readbacks.
/// @details Lifetime is managed exclusively by `VulkanStagingManager`.
class GP_RHI_VULKAN_API VulkanStagingBuffer final
{
public:
    /// @brief Constructs an uninitialized staging buffer bound to `device` and `memoryManager`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    VulkanStagingBuffer(VulkanDevice& device, VulkanMemoryManager& memoryManager) noexcept;

    VulkanStagingBuffer(const VulkanStagingBuffer&) = delete;
    VulkanStagingBuffer& operator=(const VulkanStagingBuffer&) = delete;
    VulkanStagingBuffer(VulkanStagingBuffer&&) = delete;
    VulkanStagingBuffer& operator=(VulkanStagingBuffer&&) = delete;

    /// @brief Returns the native `VkBuffer` handle.
    /// @return Native `VkBuffer`, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkBuffer getHandle() const noexcept
    {
        return m_buffer;
    }

    /// @brief Returns the persistently mapped CPU pointer for this staging buffer.
    /// @return Mapped host pointer, or `nullptr`.
    [[nodiscard]] void* getMappedPointer() const noexcept;

    /// @brief Returns the logical byte size of this staging buffer.
    /// @return Buffer size in bytes.
    [[nodiscard]] gp::UInt32 getSize() const noexcept
    {
        return m_bufferSize;
    }

    /// @brief Returns the backing `VkDeviceMemory` handle.
    /// @return Native `VkDeviceMemory`.
    [[nodiscard]] VkDeviceMemory getDeviceMemoryHandle() const noexcept;

    /// @brief Returns the host memory property flag (`HOST_COHERENT` or `HOST_CACHED`) requested for this buffer.
    /// @return `VkMemoryPropertyFlagBits`.
    [[nodiscard]] VkMemoryPropertyFlagBits getMemoryReadFlags() const noexcept
    {
        return m_memoryReadFlags;
    }

    /// @brief Flushes CPU writes to make uploaded data visible to the GPU.
    void flushMappedMemory() const noexcept;

    /// @brief Invalidates CPU caches before reading GPU-written readback data.
    void invalidateMappedMemory() const noexcept;

private:
    /// @brief Private destructor enforcing cleanup through `VulkanStagingManager`.
    ~VulkanStagingBuffer() noexcept;

    /// @brief Immediately destroys `m_buffer` and frees `m_allocation`.
    void destroy() noexcept;

    VulkanDevice& m_device;
    VulkanMemoryManager& m_memoryManager;
    VulkanAllocation m_allocation{};
    VkBuffer m_buffer{ VK_NULL_HANDLE };
    VkMemoryPropertyFlagBits m_memoryReadFlags{ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
    gp::UInt32 m_bufferSize{ 0u };

    friend class VulkanStagingManager;
};

/// @brief Pool manager allocating, recycling, and retiring `VulkanStagingBuffer` instances gated by GPU sync points.
class GP_RHI_VULKAN_API VulkanStagingManager final
{
public:
    /// @brief Default constructor.
    VulkanStagingManager() noexcept = default;

    /// @brief Destructor. Calls `deinit()` to release any remaining free or pending staging buffers.
    ~VulkanStagingManager() noexcept;

    VulkanStagingManager(const VulkanStagingManager&) = delete;
    VulkanStagingManager& operator=(const VulkanStagingManager&) = delete;
    VulkanStagingManager(VulkanStagingManager&&) = delete;
    VulkanStagingManager& operator=(VulkanStagingManager&&) = delete;

    /// @brief Binds this staging manager to `device` and `memoryManager`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] memoryManager Active `VulkanMemoryManager`.
    void init(VulkanDevice* device, VulkanMemoryManager* memoryManager) noexcept;

    /// @brief Forces all pending and free staging buffers to be destroyed immediately.
    void deinit() noexcept;

    /// @brief Acquires an idle staging buffer matching `(size, memoryReadFlags)` or allocates a new one.
    /// @param[in] size Requested logical byte size.
    /// @param[in] usageFlags Native `VkBufferUsageFlags` (defaults to `VK_BUFFER_USAGE_TRANSFER_SRC_BIT`).
    /// @param[in] memoryReadFlags Host memory property (`HOST_COHERENT` for uploads, `HOST_CACHED` for readback).
    /// @return Pointer to an active `VulkanStagingBuffer`, or `nullptr` on failure.
    [[nodiscard]] VulkanStagingBuffer* acquireBuffer(
        gp::UInt32 size,
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VkMemoryPropertyFlagBits memoryReadFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    ) noexcept;

    /// @brief Releases `stagingBuffer` back to the pool once `syncPoint` completes (or immediately if `syncPoint` is
    ///        null) and nulls the caller's pointer.
    /// @param[in] syncPoint Optional GPU sync point gating reuse of `stagingBuffer`.
    /// @param[in,out] stagingBuffer Reference to staging buffer pointer; set to `nullptr` on return.
    /// @param[in] currentFrame Current frame index used when aging free buffers.
    void releaseBuffer(
        VulkanSyncPointRef syncPoint, VulkanStagingBuffer*& stagingBuffer, gp::UInt32 currentFrame = 0u
    ) noexcept;

    /// @brief Promotes completed sync-point staging buffers to the free list and optionally frees aged buffers to OS.
    /// @param[in] immediately When true, promotes and (when `freeToOS`) destroys all buffers regardless of sync state.
    /// @param[in] freeToOS When true, destroys aged free staging buffers back to `VulkanMemoryManager`.
    /// @param[in] currentFrame Current frame index used for free-buffer aging.
    void processPendingFree(bool immediately, bool freeToOS, gp::UInt32 currentFrame = 0u) noexcept;

    /// @brief Accumulates active, pending, and free staging buffer metrics into `outStats`.
    /// @param[in,out] outStats Destination stats snapshot.
    void getMemoryDump(VulkanResourceHeapStats& outStats) const noexcept;

    /// @brief Returns the number of staging buffers currently checked out to callers.
    /// @return Used staging buffer count.
    [[nodiscard]] gp::USize getUsedBufferCount() const noexcept;

    /// @brief Returns the number of staging buffers waiting on GPU sync-point completion.
    /// @return Pending-free staging buffer count.
    [[nodiscard]] gp::USize getPendingFreeBufferCount() const noexcept;

    /// @brief Returns the number of idle staging buffers available for immediate reuse.
    /// @return Free staging buffer count.
    [[nodiscard]] gp::USize getFreeBufferCount() const noexcept;

    /// @brief Returns total live staging buffer bytes currently allocated.
    /// @return Used staging memory in bytes.
    [[nodiscard]] gp::UInt64 getUsedMemory() const noexcept;

    /// @brief Returns peak live staging buffer bytes observed.
    /// @return Peak staging memory in bytes.
    [[nodiscard]] gp::UInt64 getPeakUsedMemory() const noexcept;

private:
    /// @brief Staging buffers waiting on a shared GPU `VulkanSyncPoint`.
    struct PendingFreeBatch final
    {
        VulkanSyncPointRef syncPoint{};
        gp::Vector<VulkanStagingBuffer*> buffers{};
    };

    /// @brief Idle staging buffer tracked with the frame it became free.
    struct FreeEntry final
    {
        VulkanStagingBuffer* stagingBuffer{ nullptr };
        gp::UInt32 frameNumber{ 0u };
    };

    /// @brief Unlocked implementation of `processPendingFree()` (caller holds `m_mutex`).
    /// @param[in] immediately True to bypass sync points and frame lag.
    /// @param[in] freeToOS True to destroy aged free staging buffers.
    /// @param[in] currentFrame Current frame index.
    void processPendingFreeLocked(bool immediately, bool freeToOS, gp::UInt32 currentFrame) noexcept;

    VulkanDevice* m_device{ nullptr };
    VulkanMemoryManager* m_memoryManager{ nullptr };

    mutable gp::thread::Mutex m_mutex{};
    gp::Vector<VulkanStagingBuffer*> m_usedStagingBuffers{};
    gp::Vector<PendingFreeBatch> m_pendingFreeStagingBuffers{};
    gp::Vector<FreeEntry> m_freeStagingBuffers{};

    gp::UInt64 m_usedMemory{ 0u };
    gp::UInt64 m_peakUsedMemory{ 0u };
};

}   // namespace gp::rhi::vulkan
