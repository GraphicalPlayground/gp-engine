// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/VulkanAllocation.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "templates/Function.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Frame-latency and GPU sync-point gated deferred deletion queue for native Vulkan handles and GPU memory.
/// @details Integrates with `gp::rhi::resource::ResourceDeletionQueue` and ensures Vulkan handles (`VkBuffer`,
///          `VkImage`, `VkPipeline`, `VkSampler`, etc.), `VulkanAllocation` sub-allocations, and raw
///          `VulkanDeviceMemoryAllocation` blocks are destroyed only after GPU execution passes their sync point.
class GP_RHI_VULKAN_API VulkanDeferredDeletionQueue final
{
public:
    /// @brief Discriminator identifying the Vulkan handle or memory resource stored in a queued entry.
    enum class EntryType : gp::UInt8
    {
        RenderPass = 0u,
        Buffer,
        BufferView,
        Image,
        ImageView,
        Pipeline,
        PipelineLayout,
        Framebuffer,
        DescriptorSetLayout,
        Sampler,
        Semaphore,
        ShaderModule,
        Event,
        AccelerationStructure,
        ResourceAllocation,
        DeviceMemoryAllocation,
        Func
    };

    /// @brief Constructs a deferred deletion queue bound to `device`, `deviceMemoryManager`, and `memoryManager`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] deviceMemoryManager Low-level device memory manager.
    /// @param[in] memoryManager High-level memory manager for pooled allocation frees.
    VulkanDeferredDeletionQueue(
        VulkanDevice& device, VulkanDeviceMemoryManager& deviceMemoryManager, VulkanMemoryManager& memoryManager
    ) noexcept;

    /// @brief Destructor. Flushes any remaining entries immediately to guarantee zero handle leaks on shutdown.
    ~VulkanDeferredDeletionQueue() noexcept;

    VulkanDeferredDeletionQueue(const VulkanDeferredDeletionQueue&) = delete;
    VulkanDeferredDeletionQueue& operator=(const VulkanDeferredDeletionQueue&) = delete;
    VulkanDeferredDeletionQueue(VulkanDeferredDeletionQueue&&) = delete;
    VulkanDeferredDeletionQueue& operator=(VulkanDeferredDeletionQueue&&) = delete;

    /// @brief Enqueues a raw 64-bit Vulkan object handle of `type` for deferred destruction.
    /// @param[in] type Vulkan object type discriminator.
    /// @param[in] handle Encoded 64-bit native handle (ignored if `0`).
    void enqueueGenericResource(EntryType type, gp::UInt64 handle) noexcept;

    /// @brief Enqueues a typed native Vulkan handle for deferred destruction.
    /// @tparam InHandleType Native Vulkan handle type (for example `VkBuffer`, `VkImage`, `VkSampler`).
    /// @param[in] type Matching `EntryType` discriminator.
    /// @param[in] handle Native Vulkan handle (ignored if `VK_NULL_HANDLE`).
    template <typename InHandleType>
    void enqueueHandle(const EntryType type, const InHandleType handle) noexcept
    {
        if (handle != VK_NULL_HANDLE)
        {
            enqueueGenericResource(type, reinterpret_cast<gp::UInt64>(handle));
        }
    }

    /// @brief Enqueues an arbitrary cleanup callback invoked with `VulkanDevice&` once GPU work retires.
    /// @param[in] func Move-only cleanup callback (ignored if unbound).
    void enqueueFunc(gp::UniqueFunction<void(VulkanDevice&)>&& func) noexcept;

    /// @brief Enqueues a raw `VulkanDeviceMemoryAllocation*` for deferred release.
    /// @param[in] deviceMemoryAllocation Raw device memory allocation (ignored if null).
    void enqueueDeviceAllocation(VulkanDeviceMemoryAllocation* deviceMemoryAllocation) noexcept;

    /// @brief Disowns `allocation` and enqueues it for deferred release through `VulkanMemoryManager`.
    /// @param[in,out] allocation Owned `VulkanAllocation` (disowned on return).
    void enqueueResourceAllocation(VulkanAllocation& allocation) noexcept;

    /// @brief Flushes `gp::rhi::resource::ResourceDeletionQueue` so destroyed RHI wrappers enqueue their Vulkan
    ///        handles, then advances the frame bucket and retires matured entries.
    /// @param[in] deleteImmediately When true, destroys all pending and in-flight entries immediately.
    /// @param[in] completionSyncPoint Optional GPU sync point gating destruction of the harvested batch.
    /// @param[in] currentFrame Current deletion frame counter.
    void releaseResources(
        bool deleteImmediately = false, VulkanSyncPointRef completionSyncPoint = nullptr, gp::UInt32 currentFrame = 0u
    ) noexcept;

    /// @brief Polls in-flight GPU-gated deletion batches and destroys every batch whose sync point is complete.
    /// @param[in] forceImmediate When true, destroys all in-flight batches regardless of sync point completion.
    /// @return Number of batches destroyed during this call.
    gp::UInt32 processCompletedBatches(bool forceImmediate = false) noexcept;

    /// @brief Flushes `gp::rhi::resource::ResourceDeletionQueue::flushPendingDeletes()` to collect RHI wrappers.
    /// @param[in] includeExtendedLifetimeResources Forwarded to `ResourceDeletionQueue::flushPendingDeletes()`.
    static void flushRHIDeletionQueue(bool includeExtendedLifetimeResources = true) noexcept;

    /// @brief Returns the total number of entries waiting in frame buckets.
    /// @return Queued entry count.
    [[nodiscard]] gp::USize getPendingEntryCount() const noexcept;

    /// @brief Returns the number of harvested batches currently waiting on GPU sync point completion.
    /// @return In-flight batch count.
    [[nodiscard]] gp::USize getInFlightBatchCount() const noexcept;

private:
    /// @brief Single deferred destruction record.
    struct Entry final
    {
        EntryType structureType{ EntryType::Buffer };
        gp::UInt64 handle{ 0u };
        VulkanDeviceMemoryAllocation* deviceMemoryAllocation{ nullptr };
        VulkanAllocation allocation{};
        gp::UniqueFunction<void(VulkanDevice&)>* func{ nullptr };
    };

    /// @brief Harvested frame batch awaiting GPU completion of `syncPoint`.
    struct InFlightBatch final
    {
        VulkanSyncPointRef syncPoint{};
        gp::Vector<Entry> entries{};
    };

    /// @brief Immediately destroys every entry in `entries`, batching pooled allocation frees per allocator.
    /// @param[in] entries Entries to destroy.
    void releaseResourcesImmediately(const gp::Vector<Entry>& entries) noexcept;

    VulkanDevice& m_device;
    VulkanDeviceMemoryManager& m_deviceMemoryManager;
    VulkanMemoryManager& m_memoryManager;

    mutable gp::thread::Mutex m_mutex{};
    gp::Vector<gp::Vector<Entry>> m_entriesPerFrame{};
    gp::Vector<InFlightBatch> m_inFlightBatches{};
    gp::UInt32 m_currentFrameIndex{ 0u };
    gp::UInt32 m_lastReleaseFrameNumber{ 0xFFFFFFFFu };
};

}   // namespace gp::rhi::vulkan
