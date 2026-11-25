// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/StagingBuffer.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/SyncForward.hpp"                  // IWYU pragma: keep
#include "VulkanForward.hpp"

namespace gp::rhi::vulkan
{

/// @brief RHI staging buffer used to read GPU buffer data back on the CPU.
/// @details Starts empty. Each `copyFromBuffer()` records a copy into a host-cached staging buffer, acquiring a larger
///          one when needed, and remembers how many bytes were queued. Once the GPU work is done (the caller waits on a
///          fence), `lock()` invalidates the CPU cache and returns the mapped data.
/// @note Named apart from `VulkanStagingBuffer`, the internal host-visible buffer it wraps.
class GP_RHI_VULKAN_API VulkanReadbackStagingBuffer final : public resource::StagingBuffer
{
public:
    /// @brief Constructs an empty staging buffer; memory is acquired by the first copy.
    VulkanReadbackStagingBuffer() noexcept = default;

    /// @copydoc resource::StagingBuffer::lock()
    /// @note The range must lie inside the bytes queued by the last copy. Invalidates the mapping first so the CPU
    ///       sees what the GPU wrote.
    [[nodiscard]] virtual void* lock(gp::UInt32 offset, gp::UInt32 numBytes) noexcept override;

    /// @copydoc resource::StagingBuffer::unlock()
    virtual void unlock() noexcept override;

    /// @copydoc resource::StagingBuffer::getGPUSizeBytes()
    /// @note Returns the size of the internal staging buffer, which can be larger than the last copy.
    [[nodiscard]] virtual gp::UInt64 getGPUSizeBytes() const noexcept override;

    /// @brief Records a copy of `numBytes` bytes of `sourceBuffer`, starting at `offset`, into this staging buffer.
    /// @details Reuses the current internal buffer when it is large enough, otherwise releases it and acquires a new
    ///          host-cached one. A barrier makes the copy visible to host reads.
    /// @note Bottom of pipe. Backs `IComputeContext::copyToStagingBuffer()`; transfers are illegal inside a render
    /// pass.
    /// @param[in,out] recorder Recorder of the executing context.
    /// @param[in] sourceBuffer Buffer to read from.
    /// @param[in] offset Byte offset inside `sourceBuffer`.
    /// @param[in] numBytes Byte count to copy.
    void copyFromBuffer(
        IVulkanTransferCommandRecorder& recorder,
        const VulkanBuffer& sourceBuffer,
        gp::UInt32 offset,
        gp::UInt32 numBytes
    ) noexcept;

    /// @brief Returns the number of bytes queued by the last copy.
    /// @return Readable byte count.
    [[nodiscard]] gp::UInt32 getQueuedNumBytes() const noexcept
    {
        return m_queuedNumBytes;
    }

protected:
    /// @brief Returns the internal staging buffer to its pool.
    virtual ~VulkanReadbackStagingBuffer() noexcept override;

private:
    VulkanStagingBuffer* m_stagingBuffer{ nullptr };
    VulkanStagingManager* m_stagingManager{ nullptr };
    gp::UInt32 m_queuedNumBytes{ 0u };
};

/// @brief Creates an empty readback staging buffer.
/// @note Backs `DynamicRHI::createStagingBuffer()`.
/// @return Reference to the new staging buffer.
[[nodiscard]] GP_RHI_VULKAN_API resource::StagingBufferRef createVulkanStagingBuffer() noexcept;

/// @brief Maps a range of `stagingBuffer` for CPU reads.
/// @note Backs `DynamicRHI::lockStagingBuffer()`. `fence`, when given, must already be signaled.
/// @param[in,out] stagingBuffer Staging buffer created by `createVulkanStagingBuffer()`.
/// @param[in] fence Optional fence guarding the copy.
/// @param[in] offset Byte offset of the range.
/// @param[in] numBytes Byte count to map.
/// @return Pointer to the first byte of the range.
[[nodiscard]] GP_RHI_VULKAN_API void* lockVulkanStagingBuffer(
    resource::StagingBuffer* stagingBuffer, const sync::GPUFence* fence, gp::UInt32 offset, gp::UInt32 numBytes
) noexcept;

/// @brief Ends a lock started by `lockVulkanStagingBuffer()`.
/// @note Backs `DynamicRHI::unlockStagingBuffer()`.
/// @param[in,out] stagingBuffer Locked staging buffer.
GP_RHI_VULKAN_API void unlockVulkanStagingBuffer(resource::StagingBuffer* stagingBuffer) noexcept;

}   // namespace gp::rhi::vulkan

