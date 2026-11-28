// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/strings/Name.hpp"
#include "core/GPUMask.hpp"
#include "core/Resource.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "RHIBase.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::sync
{

/// @brief GPU-to-CPU synchronization fence signaled from GPU command queues.
/// @details Tracks an atomic count of queued write commands so callers can verify whether every pending signal
///          command has been submitted to the backend before polling or waiting on completion.
class GP_RHI_API GPUFence : public resource::Resource
{
public:
    GPUFence() = delete;

    /// @brief Constructs a GPU fence resource identified by `name`.
    /// @param[in] name Diagnostic name attached to this fence.
    explicit GPUFence(gp::Name name) noexcept;

    GPUFence(const GPUFence&) = delete;
    GPUFence& operator=(const GPUFence&) = delete;
    GPUFence(GPUFence&&) = delete;
    GPUFence& operator=(GPUFence&&) = delete;

public:
    /// @brief Resets the fence back to the unsignaled state for reuse.
    virtual void clear() noexcept = 0;

    /// @brief Queries whether the GPU has reached and signaled this fence.
    /// @return True if the fence has been signaled; false otherwise.
    [[nodiscard]] virtual bool poll() const noexcept = 0;

    /// @brief Queries whether the specified GPU nodes have signaled this fence.
    /// @details The default implementation supports single-GPU configurations by asserting `gpuMask == GPUMask::gpu0()`
    ///          and delegating to `poll()`. Multi-GPU backends may override this method.
    /// @param[in] gpuMask Bitmask of GPU nodes to query.
    /// @return True if the queried GPU nodes have signaled this fence; false otherwise.
    [[nodiscard]] virtual bool poll(GPUMask gpuMask) const noexcept;

    /// @brief Blocks the calling thread until the specified GPU nodes signal this fence.
    /// @param[in,out] cmdList Immediate command list used to flush queued commands if required by the backend.
    /// @param[in] gpuMask Bitmask of GPU nodes to wait on.
    virtual void wait(command::CommandListImmediate& cmdList, GPUMask gpuMask) const noexcept = 0;

    /// @brief Returns the diagnostic name assigned to this fence at construction.
    /// @return Immutable `gp::Name` of this fence.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Name getName() const noexcept
    {
        return m_fenceName;
    }

    /// @brief Increments the atomic count of pending GPU write-fence commands.
    /// @details Called when a write-fence command is enqueued on a command list prior to RHI thread execution.
    /// @return Updated pending write command count after incrementing.
    GP_FORCEINLINE_HINT gp::Int32 incrementPendingWriteCommands() noexcept
    {
        return m_numPendingWriteCommands.fetchAdd(1, gp::memory::MemoryOrder::AcqRel) + 1;
    }

    /// @brief Decrements the atomic count of pending GPU write-fence commands.
    /// @details Called when the backend finishes executing a queued write-fence command.
    /// @note Asserts that the resulting count does not underflow below zero.
    /// @return Updated pending write command count after decrementing.
    GP_FORCEINLINE_HINT gp::Int32 decrementPendingWriteCommands() noexcept
    {
        const gp::Int32 newCount = m_numPendingWriteCommands.fetchSub(1, gp::memory::MemoryOrder::AcqRel) - 1;
        GP_CHECKF(newCount >= 0, "GPUFence pending write command count underflowed below zero.");
        return newCount;
    }

    /// @brief Reads the number of queued write-fence commands not yet executed by the backend.
    /// @return Current pending write command count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Int32 getNumPendingWriteCommands() const noexcept
    {
        return m_numPendingWriteCommands.load(gp::memory::MemoryOrder::Acquire);
    }

    /// @brief Reports whether any write-fence commands are still pending execution.
    /// @return True if `getNumPendingWriteCommands() > 0`; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasPendingWriteCommands() const noexcept
    {
        return getNumPendingWriteCommands() > 0;
    }

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~GPUFence() noexcept override;

private:
    /// @brief Atomic count of queued write commands targeting this fence.
    gp::thread::Atomic<gp::Int32> m_numPendingWriteCommands{ 0 };

    /// @brief Diagnostic name stored at fence creation.
    const gp::Name m_fenceName{};
};

}   // namespace gp::rhi::sync
