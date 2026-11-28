// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/IComputeContext.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "contexts/VulkanContextCommon.hpp"
#include "contexts/VulkanContextDesc.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Context recording asynchronous resource uploads (staging copies, initial clears, layout setups).
/// @details Resource creation must not wait for the graphics context to be free: uploads are recorded into their
///          own command buffers and submitted ahead of the graphics work that consumes them. The context only exposes
///          a transfer recorder; payload, sync point and submission handling come from `VulkanContextCommon`.
/// @note Not thread-safe: acquire one per thread from `VulkanUploadContextPool`.
class GP_RHI_VULKAN_API VulkanUploadContext final
    : public command::IUploadContext
    , public VulkanContextCommon
{
public:
    /// @brief Creates an upload context recording on `services.queue`.
    /// @param[in] services Collaborators; only `device`, `queue` and `submitter` are used.
    explicit VulkanUploadContext(const VulkanContextServices& services) noexcept;

    /// @brief Drops the transfer recorder and discards unfinished payloads.
    virtual ~VulkanUploadContext() noexcept override;

    VulkanUploadContext(const VulkanUploadContext&) = delete;
    VulkanUploadContext& operator=(const VulkanUploadContext&) = delete;
    VulkanUploadContext(VulkanUploadContext&&) = delete;
    VulkanUploadContext& operator=(VulkanUploadContext&&) = delete;

public:
    /// @brief Returns the transfer recorder bound to the active command buffer, creating one if needed.
    /// @details The recorder is rebuilt whenever the context moved to a new command buffer (after a finalize).
    /// @return Recorder, or `nullptr` when no command buffer could be acquired.
    [[nodiscard]] IVulkanTransferCommandRecorder* getTransferRecorder() noexcept;

protected:
    /// @copydoc VulkanContextCommon::onFinalize()
    /// @note Drops the recorder: its command buffer is about to be submitted.
    virtual void onFinalize(VulkanPayload& signalPayload) noexcept override;

private:
    /// @brief Recorder bound to `m_recorderOwner`.
    gp::UniquePtr<VulkanCommandBufferTransferRecorder> m_recorder{};

    /// @brief Command buffer `m_recorder` records into.
    VulkanCommandBuffer* m_recorderOwner{ nullptr };
};

/// @brief Free list of upload contexts, so every recording thread reuses a warm context and its command pool.
/// @details Contexts are created on demand and never destroyed before the pool. `release()` returns a context to the
///          free list; it must have been finalized (or flushed) first.
/// @note Thread-safe: the RHI timeline acquires through the command context provider while any thread may acquire
///       through `DynamicRHI::acquireUploadContext()`. A context itself is used by one thread at a time.
class GP_RHI_VULKAN_API VulkanUploadContextPool final
{
public:
    /// @brief Creates an empty pool building contexts from `services`.
    /// @param[in] services Collaborators handed to every created context.
    explicit VulkanUploadContextPool(const VulkanContextServices& services) noexcept;

    /// @brief Destroys every context the pool created.
    ~VulkanUploadContextPool() noexcept;

    VulkanUploadContextPool(const VulkanUploadContextPool&) = delete;
    VulkanUploadContextPool& operator=(const VulkanUploadContextPool&) = delete;
    VulkanUploadContextPool(VulkanUploadContextPool&&) = delete;
    VulkanUploadContextPool& operator=(VulkanUploadContextPool&&) = delete;

public:
    /// @brief Returns a free context, creating one when the free list is empty.
    /// @return Context owned by the pool.
    [[nodiscard]] VulkanUploadContext& acquire() noexcept;

    /// @brief Returns `context` to the free list.
    /// @param[in] context Context previously returned by `acquire()`.
    void release(VulkanUploadContext& context) noexcept;

    /// @brief Returns how many contexts the pool created.
    /// @return Created context count.
    [[nodiscard]] gp::USize getNumContexts() const noexcept;

    /// @brief Returns how many contexts wait in the free list.
    /// @return Free context count.
    [[nodiscard]] gp::USize getNumFreeContexts() const noexcept;

private:
    VulkanContextServices m_services{};

    /// @brief Guards both context lists.
    mutable gp::thread::Mutex m_mutex{};

    gp::Vector<gp::UniquePtr<VulkanUploadContext>> m_contexts{};
    gp::Vector<VulkanUploadContext*> m_freeContexts{};
};

}   // namespace gp::rhi::vulkan
