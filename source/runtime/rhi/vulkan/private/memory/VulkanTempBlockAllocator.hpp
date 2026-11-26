// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "memory/VulkanAllocation.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/SharedMutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief One persistently mapped host-visible `VkBuffer` that the temp block allocator bump-allocates from.
/// @details A block is recycled once every reference taken outside a context was returned and every context sync
///          point that wrote into it has completed on the GPU.
struct VulkanTempMemoryBlock final
{
    /// @brief Dedicated host-visible allocation backing `buffer`.
    VulkanAllocation allocation{};

    /// @brief Native buffer covering the whole block.
    VkBuffer buffer{ VK_NULL_HANDLE };

    /// @brief Persistent CPU mapping of the block start.
    gp::UInt8* mappedPointer{ nullptr };

    /// @brief Device address of the block start, or 0 when buffer device address is unavailable.
    VkDeviceAddress bufferAddress{ 0u };

    /// @brief Context sync points that sub-allocated from this block; all must complete before reuse.
    gp::Vector<VulkanSyncPointRef> syncPoints{};

    /// @brief Live references taken by allocations made without a context (for example uniform buffers).
    gp::thread::Atomic<gp::UInt32> references{ 0u };

    /// @brief Bump offset of the next free byte.
    gp::thread::Atomic<gp::UInt32> currentOffset{ 0u };
};

/// @brief Ring of large host-visible blocks serving volatile allocations (single-draw/single-frame uniform buffers,
///        upload scratch and shader binding tables).
/// @details Allocation is a lock-free `fetchAdd` on the current block under a shared lock. Only switching to a new
///          block, or registering a new context sync point, takes the exclusive lock. Retired blocks sit in a busy list
///          until `updateBlocks()` sees them idle and moves them to the available list, so steady-state frames never
///          allocate device memory.
/// @note Thread-safe.
class GP_RHI_VULKAN_API VulkanTempBlockAllocator final
{
public:
    /// @brief Default block size in bytes (4 MB).
    static constexpr gp::UInt32 kDefaultBlockSize = 4u * 1024u * 1024u;

    /// @brief Creates the allocator and its first block.
    /// @param[in] memoryManager Memory manager that owns device memory and the device.
    /// @param[in] blockSize Size of each block in bytes.
    /// @param[in] blockAlignment Alignment of every sub-allocation; must be a power of two.
    /// @param[in] bufferUsage Usage flags of each block buffer.
    VulkanTempBlockAllocator(
        VulkanMemoryManager& memoryManager,
        gp::UInt32 blockSize,
        gp::UInt32 blockAlignment,
        VkBufferUsageFlags bufferUsage
    ) noexcept;

    /// @brief Destroys every block immediately.
    /// @note The GPU must be idle.
    ~VulkanTempBlockAllocator() noexcept;

    VulkanTempBlockAllocator(const VulkanTempBlockAllocator&) = delete;
    VulkanTempBlockAllocator& operator=(const VulkanTempBlockAllocator&) = delete;
    VulkanTempBlockAllocator(VulkanTempBlockAllocator&&) = delete;
    VulkanTempBlockAllocator& operator=(VulkanTempBlockAllocator&&) = delete;

    /// @brief Computes the default block alignment for `device`.
    /// @details Uses `max(minUniformBufferOffsetAlignment, 16)`, raised to the shader group base alignment when ray
    ///          tracing pipelines are enabled.
    /// @param[in] device Initialized device.
    /// @return Alignment in bytes.
    [[nodiscard]] static gp::UInt32 computeDefaultBlockAlignment(const VulkanDevice& device) noexcept;

    /// @brief Computes the default block buffer usage for `device`.
    /// @details Uniform and transfer-source usage, plus device address and shader binding table usage when enabled.
    /// @param[in] device Initialized device.
    /// @return Buffer usage flags.
    [[nodiscard]] static VkBufferUsageFlags computeDefaultBufferUsage(const VulkanDevice& device) noexcept;

    /// @brief Reads the block size from `r.Vulkan.TempBlockSizeKB`.
    /// @return Block size in bytes.
    [[nodiscard]] static gp::UInt32 getConfiguredBlockSize() noexcept;

    /// @brief Reports whether the first block was created.
    /// @return True when the allocator can serve allocations.
    [[nodiscard]] bool isValid() const noexcept
    {
        return (m_currentBlock != nullptr) && (m_currentBlock->buffer != VK_NULL_HANDLE);
    }

    /// @brief Sub-allocates `size` bytes and describes the range as a `VulkanAllocation`.
    /// @details With `contextSyncPoint`, the block lifetime follows that sync point. Without it, the returned
    ///          allocation is flagged `isTempBlockAllocator` and holds one reference that `freeReference()` returns.
    /// @param[in] size Requested size in bytes; its aligned size must be smaller than the block size.
    /// @param[in] alignment Caller alignment; must not exceed the block alignment.
    /// @param[in] contextSyncPoint Sync point of the recording context, or `nullptr` for reference counting.
    /// @param[out] outAllocation Receives a non-owning view of the block allocation, offset to the range.
    /// @param[out] outDescriptorAddressInfo Optional descriptor address info for the range.
    /// @return Mapped CPU pointer to the range, or `nullptr` when no block is available.
    [[nodiscard]] gp::UInt8* allocate(
        gp::UInt32 size,
        gp::UInt32 alignment,
        const VulkanSyncPointRef* contextSyncPoint,
        VulkanAllocation& outAllocation,
        VkDescriptorAddressInfoEXT* outDescriptorAddressInfo = nullptr
    ) noexcept;

    /// @brief Sub-allocates `size` bytes for a descriptor buffer binding.
    /// @param[in] size Requested size in bytes.
    /// @param[in] contextSyncPoint Sync point of the recording context.
    /// @param[out] outBindingInfo Receives the block address and buffer usage.
    /// @param[out] outOffset Receives the byte offset of the range inside the block.
    /// @return Mapped CPU pointer to the range, or `nullptr` when no block is available.
    [[nodiscard]] gp::UInt8* allocate(
        gp::UInt32 size,
        const VulkanSyncPointRef& contextSyncPoint,
        VkDescriptorBufferBindingInfoEXT& outBindingInfo,
        VkDeviceSize& outOffset
    ) noexcept;

    /// @brief Sub-allocates `size` bytes for a strided device address region (shader binding tables).
    /// @param[in] size Requested size in bytes.
    /// @param[in] contextSyncPoint Sync point of the recording context.
    /// @param[out] outRegion Receives the address, size and stride of the range.
    /// @return Mapped CPU pointer to the range, or `nullptr` when no block is available.
    [[nodiscard]] gp::UInt8* allocate(
        gp::UInt32 size, const VulkanSyncPointRef& contextSyncPoint, VkStridedDeviceAddressRegionKHR& outRegion
    ) noexcept;

    /// @brief Returns the reference held by an allocation made without a context.
    /// @param[in] allocation Allocation returned by `allocate()` with a null context sync point.
    void freeReference(const VulkanAllocation& allocation) noexcept;

    /// @brief Moves every busy block that is no longer referenced and whose sync points completed to the free list.
    /// @note Call once per frame, after GPU completions were processed.
    void updateBlocks() noexcept;

    /// @brief Returns the block size in bytes.
    /// @return Block size.
    [[nodiscard]] gp::UInt32 getBlockSize() const noexcept
    {
        return m_blockSize;
    }

    /// @brief Returns the sub-allocation alignment in bytes.
    /// @return Block alignment.
    [[nodiscard]] gp::UInt32 getBlockAlignment() const noexcept
    {
        return m_blockAlignment;
    }

    /// @brief Returns the usage flags of every block buffer.
    /// @return Buffer usage flags.
    [[nodiscard]] VkBufferUsageFlags getBufferUsage() const noexcept
    {
        return m_bufferUsage;
    }

    /// @brief Returns the number of retired blocks waiting for the GPU or for references.
    /// @return Busy block count.
    [[nodiscard]] gp::USize getBusyBlockCount() const noexcept;

    /// @brief Returns the number of idle blocks ready for reuse.
    /// @return Available block count.
    [[nodiscard]] gp::USize getAvailableBlockCount() const noexcept;

    /// @brief Returns the references currently held on the active block.
    /// @return Reference count of the current block.
    [[nodiscard]] gp::UInt32 getCurrentBlockReferences() const noexcept;

private:
    /// @brief Result of an internal sub-allocation.
    struct InternalAllocation final
    {
        /// @brief Block that served the allocation.
        VulkanTempMemoryBlock* block{ nullptr };

        /// @brief Byte offset inside `block`.
        gp::UInt32 offset{ 0u };
    };

    /// @brief Creates and maps a new block.
    /// @return New block, or `nullptr` on failure.
    [[nodiscard]] VulkanTempMemoryBlock* allocateBlock() noexcept;

    /// @brief Destroys `block` and frees its memory immediately.
    /// @param[in] block Block to destroy (may be null).
    void destroyBlock(VulkanTempMemoryBlock* block) noexcept;

    /// @brief Bump-allocates from the current block, switching blocks when it is full.
    /// @param[in] size Requested size in bytes.
    /// @param[in] contextSyncPoint Sync point of the recording context, or `nullptr` to take a reference.
    /// @return Block and offset; the block is null on failure.
    [[nodiscard]] InternalAllocation
        allocateInternal(gp::UInt32 size, const VulkanSyncPointRef* contextSyncPoint) noexcept;

    /// @brief Reports whether `block` already tracks `syncPoint`.
    /// @param[in] block Block to inspect.
    /// @param[in] syncPoint Sync point to look for.
    /// @return True when the sync point is registered.
    [[nodiscard]] static bool
        hasSyncPoint(const VulkanTempMemoryBlock& block, const VulkanSyncPointRef& syncPoint) noexcept;

    /// @brief Registers `syncPoint` on `block` once.
    /// @param[in,out] block Block to update.
    /// @param[in] syncPoint Sync point to add.
    static void addUniqueSyncPoint(VulkanTempMemoryBlock& block, const VulkanSyncPointRef& syncPoint) noexcept;

    /// @brief Reports whether `allocation` points into `block`.
    /// @param[in] allocation Allocation to test.
    /// @param[in] block Block to compare against.
    /// @return True when handle and allocator slots match.
    [[nodiscard]] static bool
        isAllocationFromBlock(const VulkanAllocation& allocation, const VulkanTempMemoryBlock& block) noexcept;

private:
    /// @brief Memory manager that owns the block allocations.
    VulkanMemoryManager& m_memoryManager;

    /// @brief Size of each block in bytes.
    const gp::UInt32 m_blockSize;

    /// @brief Sub-allocation alignment in bytes.
    const gp::UInt32 m_blockAlignment;

    /// @brief Usage flags of each block buffer.
    const VkBufferUsageFlags m_bufferUsage;

    /// @brief Block currently serving allocations.
    VulkanTempMemoryBlock* m_currentBlock{ nullptr };

    /// @brief Retired blocks still referenced or in flight.
    gp::Vector<VulkanTempMemoryBlock*> m_busyBlocks{};

    /// @brief Idle blocks ready to become current.
    gp::Vector<VulkanTempMemoryBlock*> m_availableBlocks{};

    /// @brief Shared for bump allocations, exclusive for block switches and list updates.
    mutable gp::thread::SharedMutex m_lock{};
};

}   // namespace gp::rhi::vulkan
