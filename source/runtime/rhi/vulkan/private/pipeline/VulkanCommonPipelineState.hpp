// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"            // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "threading/atomics/Atomic.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief State shared by cached graphics and compute pipelines: the native handle, its layout and its cache links.
/// @details A cached pipeline is the *inner* object of a two-level scheme:
///          - The inner state (this class and its derived types) is owned by the cache manager's map, keyed by the
///            native description. It holds the `VkPipeline`.
///          - The RHI sees *wrappers* (`VulkanWrappedGraphicsPipelineState`, ...), one per creation request. Each
///            wrapper holds a reference to the inner state and counts itself in `m_numWrappers`.
///          When the last wrapper dies, the manager drops the inner state from its map, so pipelines no one uses stop
///          taking memory, while two requests for the same native state still share one `VkPipeline`.
/// @note The pipeline layout is owned by the cache manager and outlives every pipeline that uses it.
class GP_RHI_VULKAN_API VulkanCommonPipelineState
{
public:
    VulkanCommonPipelineState(const VulkanCommonPipelineState&) = delete;
    VulkanCommonPipelineState& operator=(const VulkanCommonPipelineState&) = delete;
    VulkanCommonPipelineState(VulkanCommonPipelineState&&) = delete;
    VulkanCommonPipelineState& operator=(VulkanCommonPipelineState&&) = delete;

    /// @brief Returns the native pipeline.
    /// @return Handle, or `VK_NULL_HANDLE` when creation failed, was skipped, or the pipeline was evicted.
    [[nodiscard]] VkPipeline getPipelineHandle() const noexcept
    {
        return m_pipeline;
    }

    /// @brief Returns the pipeline layout the pipeline was created with.
    /// @return Layout owned by the cache manager.
    [[nodiscard]] const VulkanPipelineLayout* getLayout() const noexcept
    {
        return m_layout;
    }

    /// @brief Returns the number of RHI wrappers that reference this pipeline.
    /// @return Wrapper count.
    [[nodiscard]] gp::Int32 getNumWrappers() const noexcept
    {
        return m_numWrappers.load(gp::memory::MemoryOrder::Acquire);
    }

    /// @brief Reports whether `VulkanPipelineStateCacheManager::destroyCache` dropped this pipeline from its map.
    /// @return True once the cache no longer tracks the pipeline.
    [[nodiscard]] bool isEvictedFromCache() const noexcept
    {
        return m_isEvictedFromCache;
    }

    /// @brief Returns the cache manager that tracks the pipeline.
    /// @return Manager, or null once the manager destroyed its cache.
    [[nodiscard]] VulkanPipelineStateCacheManager* getOwner() const noexcept
    {
        return m_owner.load(gp::memory::MemoryOrder::Acquire);
    }

protected:
    /// @brief Creates a state without a native pipeline.
    /// @param[in] device Logical device that owns the pipeline.
    /// @param[in] deferredDeletionQueue Queue that releases the pipeline; null destroys it immediately.
    /// @param[in] owner Cache manager that tracks the state.
    VulkanCommonPipelineState(
        VulkanDevice& device, VulkanDeferredDeletionQueue* deferredDeletionQueue, VulkanPipelineStateCacheManager* owner
    ) noexcept;

    /// @brief Releases the native pipeline through the deferred deletion queue.
    ~VulkanCommonPipelineState() noexcept;

    /// @brief Releases the native pipeline and resets the handle.
    /// @details The pipeline may still be referenced by command buffers in flight, so it normally goes through the
    ///          deferred deletion queue. `immediate` skips the queue; the LRU uses it for pipelines it knows the GPU
    ///          stopped using (last used more than `kPipelineImmediateDeleteFrameDelay` frames ago).
    /// @param[in] immediate Destroy now instead of deferring.
    void destroyPipeline(bool immediate) noexcept;

    /// @brief Logical device that owns the pipeline.
    VulkanDevice& m_device;

    /// @brief Queue that releases the pipeline; null destroys it immediately.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Native pipeline.
    VkPipeline m_pipeline{ VK_NULL_HANDLE };

    /// @brief Pipeline layout, owned by the cache manager.
    VulkanPipelineLayout* m_layout{ nullptr };

    /// @brief Number of RHI wrappers; incremented under the manager's map lock, decremented when a wrapper dies.
    gp::thread::Atomic<gp::Int32> m_numWrappers{ 0 };

    /// @brief Manager that tracks the state; null once the manager destroyed its cache.
    gp::thread::Atomic<VulkanPipelineStateCacheManager*> m_owner{ nullptr };

    /// @brief The manager dropped the state from its map.
    bool m_isEvictedFromCache{ false };

    friend class VulkanPipelineStateCacheManager;
};

}   // namespace gp::rhi::vulkan
