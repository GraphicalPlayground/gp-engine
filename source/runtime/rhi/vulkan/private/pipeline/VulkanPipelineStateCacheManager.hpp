// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetLayoutCache.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/PipelineForward.hpp"                  // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"            // IWYU pragma: keep
#include "pipeline/VulkanPipelineSettings.hpp"
#include "pipeline/VulkanPipelineTypes.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "RHIVulkan.hpp"
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/SharedMutex.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Creates, deduplicates and ages every graphics and compute pipeline of one device.
/// @details Three caches live here, from the coarsest to the finest:
///          1. **Pipeline maps.** Graphics pipelines are keyed by the serialized native description
///             (`VulkanPipelineKey`), compute pipelines by shader key and specialization constants. Two creation
///             requests that translate to the same native state share one `VkPipeline`; each request still gets its
///             own RHI wrapper, and the inner state leaves the map when its last wrapper dies.
///          2. **Pipeline layouts.** Shared by every pipeline whose descriptor layout matches, and only destroyed with
///             the manager.
///          3. **The global `VkPipelineCache`.** The driver's own compilation cache, fed by every creation.
///
///          With the LRU enabled, graphics pipelines are also kept in a most-recently-used list whose total driver size
///          is bounded. Evicted pipelines keep their description; `touchGraphicsPipeline` rebuilds them on their next
///          use. Frame numbers are passed in explicitly (`setFrameNumber`, `tickLRU`) so the manager has no hidden
///          dependency on a global frame counter.
/// @note Thread-safe. A graphics creation compiles outside the map lock; concurrent requests for the same key wait for
///       the first one instead of compiling twice.
class GP_RHI_VULKAN_API VulkanPipelineStateCacheManager
{
public:
    /// @brief Creates the manager and the global pipeline cache.
    /// @param[in] device Logical device.
    /// @param[in] deferredDeletionQueue Queue that releases pipelines and layouts; null destroys them immediately.
    /// @param[in] renderPassManager Render pass manager; required unless the device renders dynamically. May be null.
    /// @param[in] lruSettings Pipeline LRU configuration.
    VulkanPipelineStateCacheManager(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        VulkanRenderPassManager* renderPassManager,
        const VulkanPipelineLRUSettings& lruSettings = getVulkanPipelineLRUSettings()
    ) noexcept;

    /// @brief Destroys the caches, every pipeline layout and the global pipeline cache.
    /// @note Wrappers that outlive the manager stay valid; they simply stop reporting back.
    ~VulkanPipelineStateCacheManager() noexcept;

    VulkanPipelineStateCacheManager(const VulkanPipelineStateCacheManager&) = delete;
    VulkanPipelineStateCacheManager& operator=(const VulkanPipelineStateCacheManager&) = delete;
    VulkanPipelineStateCacheManager(VulkanPipelineStateCacheManager&&) = delete;
    VulkanPipelineStateCacheManager& operator=(VulkanPipelineStateCacheManager&&) = delete;

    /// @brief Returns a graphics pipeline for an initializer, creating the native pipeline on a cache miss.
    /// @details Translates the initializer into a native description and looks its key up. On a hit the existing
    ///          pipeline is wrapped (and touched in the LRU unless the request is a precache). On a miss the entry is
    ///          inserted first, marked as initializing, and compiled outside the lock; precache requests with
    ///          `evictPrecachedImmediately` only record the entry.
    /// @param[in] initializer RHI initializer.
    /// @return New wrapper, or null when the initializer is invalid or the pipeline could not be created.
    [[nodiscard]] pipeline::GraphicsPipelineStateRef
        createGraphicsPipelineState(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept;

    /// @brief Returns a compute pipeline for an initializer, creating the native pipeline on a cache miss.
    /// @details Looks the key up under a shared lock, compiles outside the lock on a miss, then inserts under an
    ///          exclusive lock unless another thread inserted the same key meanwhile. A failed compilation is cached
    ///          too, and reported through `ComputePipelineState::isPipelineValid`.
    /// @param[in] initializer RHI initializer.
    /// @return New wrapper, or null when the initializer has no shader.
    [[nodiscard]] pipeline::ComputePipelineStateRef
        createComputePipelineState(const pipeline::ComputePipelineStateInitializer& initializer) noexcept;

    /// @brief Returns the pipeline layout for a descriptor layout, creating and compiling it on first use.
    /// @param[in] layoutInfo Finalized descriptor layout.
    /// @param[in] isGfxLayout True for graphics pipelines.
    /// @return Layout owned by the manager, or null when it could not be compiled.
    [[nodiscard]] VulkanPipelineLayout*
        findOrAddLayout(const VulkanDescriptorSetsLayoutInfo& layoutInfo, bool isGfxLayout) noexcept;

    /// @brief Marks a graphics pipeline as used this frame, recreating it if the LRU evicted it.
    /// @details Call before binding a pipeline when the LRU is enabled. Does nothing while the LRU is disabled or the
    ///          pipeline is still being created.
    /// @param[in,out] state Pipeline to touch.
    void touchGraphicsPipeline(VulkanGraphicsPipelineState& state) noexcept;

    /// @brief Sets the frame number LRU touches are stamped with.
    /// @param[in] frameNumber Current frame.
    void setFrameNumber(gp::UInt64 frameNumber) noexcept;

    /// @brief Returns the frame number LRU touches are stamped with.
    /// @return Current frame.
    [[nodiscard]] gp::UInt64 getFrameNumber() const noexcept;

    /// @brief Advances to `frameNumber` and evicts up to `kMaxPipelineEvictsPerTick` pipelines unused for too long.
    /// @details Only active with the LRU enabled and `evictAfterUnusedFrames != 0`; walks from the least recently used
    ///          end and stops at the first pipeline used recently enough.
    /// @param[in] frameNumber Current frame.
    void tickLRU(gp::UInt64 frameNumber) noexcept;

    /// @brief Drops every pipeline from the caches.
    /// @details Pipelines still referenced by wrappers keep their state alive, but are marked evicted from the cache
    /// and
    ///          detached from the manager. Pipeline layouts are kept.
    void destroyCache() noexcept;

    /// @brief Uncounts one wrapper of a graphics pipeline; drops the pipeline from the cache with its last wrapper.
    /// @param[in,out] state Inner state of the dying wrapper.
    void releaseGraphicsWrapper(VulkanGraphicsPipelineState& state) noexcept;

    /// @brief Uncounts one wrapper of a compute pipeline; drops the pipeline from the cache with its last wrapper.
    /// @param[in,out] state Inner state of the dying wrapper.
    void releaseComputeWrapper(VulkanComputePipelineState& state) noexcept;

    /// @brief Returns the number of cached graphics pipelines.
    /// @return Entry count.
    [[nodiscard]] gp::USize getNumGraphicsPipelines() const noexcept;

    /// @brief Returns the number of cached compute pipelines.
    /// @return Entry count.
    [[nodiscard]] gp::USize getNumComputePipelines() const noexcept;

    /// @brief Returns the number of pipeline layouts.
    /// @return Layout count.
    [[nodiscard]] gp::USize getNumLayouts() const noexcept;

    /// @brief Returns the number of graphics pipelines in the LRU.
    /// @return Pipeline count.
    [[nodiscard]] gp::UInt32 getLRUCount() const noexcept;

    /// @brief Returns the total size the LRU charges for its pipelines.
    /// @return Size in bytes.
    [[nodiscard]] gp::UInt64 getLRUSizeInBytes() const noexcept;

    /// @brief Returns the LRU configuration.
    /// @return Settings given at construction.
    [[nodiscard]] const VulkanPipelineLRUSettings& getLRUSettings() const noexcept
    {
        return m_lruSettings;
    }

    /// @brief Reports whether graphics pipelines are created for dynamic rendering instead of render passes.
    /// @return True with dynamic rendering.
    [[nodiscard]] bool usesDynamicRendering() const noexcept
    {
        return m_useDynamicRendering;
    }

    /// @brief Returns the global driver pipeline cache.
    /// @return Cache handle, or `VK_NULL_HANDLE` on an offline device.
    [[nodiscard]] VkPipelineCache getGlobalPipelineCache() const noexcept
    {
        return m_globalPipelineCache;
    }

private:
    /// @brief One cached graphics pipeline.
    struct GraphicsEntry
    {
        /// @brief Hash of the key; cheap first filter.
        gp::UInt64 hash{ 0u };

        /// @brief Cached pipeline.
        gp::RefCountedPtr<VulkanGraphicsPipelineState> state{};
    };

    /// @brief One cached compute pipeline.
    struct ComputeEntry
    {
        /// @brief Compute shader; compared too, so two shaders with colliding keys never share a pipeline.
        const VulkanShader* shader{ nullptr };

        /// @brief Shader key and specialization constants hash.
        gp::UInt64 key{ 0u };

        /// @brief Cached pipeline.
        gp::RefCountedPtr<VulkanComputePipelineState> state{};
    };

    /// @brief Finds a graphics entry by key; graphics lock held.
    /// @param[in] key Key to look for.
    /// @return Entry, or null.
    [[nodiscard]] GraphicsEntry* findGraphicsEntry(const VulkanPipelineKey& key) noexcept;

    /// @brief Removes a graphics entry by state; graphics lock held.
    /// @param[in] state State to remove.
    void removeGraphicsEntry(const VulkanGraphicsPipelineState& state) noexcept;

    /// @brief Finds a compute entry; compute lock held (shared or exclusive).
    /// @param[in] shader Compute shader.
    /// @param[in] key Cache key.
    /// @return Entry, or null.
    [[nodiscard]] const ComputeEntry* findComputeEntry(const VulkanShader* shader, gp::UInt64 key) const noexcept;

    /// @brief Fills the parts of a new graphics state that are not part of its key: layout, render pass, shaders.
    /// @param[in] initializer RHI initializer.
    /// @param[in] layoutInfo Finalized descriptor layout.
    /// @param[in,out] state New state.
    /// @return False when the layout or the render pass could not be created.
    [[nodiscard]] bool prepareGraphicsState(
        const pipeline::GraphicsPipelineStateInitializer& initializer,
        const VulkanDescriptorSetsLayoutInfo& layoutInfo,
        VulkanGraphicsPipelineState& state
    ) noexcept;

    /// @brief Builds the native graphics pipeline of a state from its description.
    /// @details With the LRU, the pipeline is compiled into a private `VkPipelineCache` whose blob size becomes the
    ///          LRU charge, then merged into the global cache. Without it, the global cache is used directly.
    /// @param[in,out] state State to build; receives the handle and the LRU charge.
    /// @return False when the device is offline or the driver failed.
    [[nodiscard]] bool createGraphicsVkPipeline(VulkanGraphicsPipelineState& state) noexcept;

    /// @brief Builds the native compute pipeline of a state, with its layout.
    /// @param[in,out] state State to build.
    /// @return False when the layout or the pipeline could not be created.
    [[nodiscard]] bool createComputeVkPipeline(VulkanComputePipelineState& state) noexcept;

    /// @brief Wraps a counted graphics state.
    /// @param[in] state Inner state, already counted.
    /// @return New wrapper.
    [[nodiscard]] static pipeline::GraphicsPipelineStateRef
        makeGraphicsWrapper(VulkanGraphicsPipelineState* state) noexcept;

    /// @brief Wraps a counted compute state.
    /// @param[in] state Inner state, already counted.
    /// @return New wrapper.
    [[nodiscard]] static pipeline::ComputePipelineStateRef
        makeComputeWrapper(VulkanComputePipelineState* state) noexcept;

    /// @brief Links a pipeline at the most recently used end; graphics lock held.
    /// @param[in,out] state Pipeline with a native handle.
    void lruAdd(VulkanGraphicsPipelineState& state) noexcept;

    /// @brief Unlinks a pipeline and destroys its native handle; graphics lock held.
    /// @details The handle is destroyed immediately when its last use is old enough for the GPU to be done with it,
    ///          otherwise through the deferred deletion queue.
    /// @param[in,out] state Pipeline to evict; ignored when not linked.
    void lruRemove(VulkanGraphicsPipelineState& state) noexcept;

    /// @brief Evicts the least recently used pipeline; graphics lock held.
    /// @return False when the LRU is empty.
    bool lruEvictOne() noexcept;

    /// @brief Evicts until `neededBytes` more fit in the budget and one more pipeline fits in the capacity.
    /// @param[in] neededBytes Size of the pipeline about to be added.
    void lruTrim(gp::UInt32 neededBytes) noexcept;

    /// @brief Moves a linked pipeline to the most recently used end, or rebuilds an evicted one; graphics lock held.
    /// @param[in,out] state Pipeline to touch.
    void lruTouch(VulkanGraphicsPipelineState& state) noexcept;

    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Queue that releases pipelines and layouts; null destroys them immediately.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Render pass manager; null with dynamic rendering only.
    VulkanRenderPassManager* m_renderPassManager{ nullptr };

    /// @brief Pipeline LRU configuration.
    VulkanPipelineLRUSettings m_lruSettings{};

    /// @brief Render target capabilities used to build render target layouts.
    VulkanRenderTargetCaps m_renderTargetCaps;

    /// @brief Dynamic state extensions of the device.
    VulkanDynamicStateFeatures m_dynamicStateFeatures{};

    /// @brief Graphics pipelines target dynamic rendering instead of render passes.
    bool m_useDynamicRendering{ false };

    /// @brief Descriptor set layouts shared by every pipeline layout.
    VulkanDescriptorSetLayoutCache m_descriptorSetLayoutCache;

    /// @brief Guards `m_layouts`.
    mutable gp::thread::Mutex m_layoutMutex{};

    /// @brief Pipeline layouts, owned.
    gp::Vector<VulkanPipelineLayout*> m_layouts{};

    /// @brief Shared for pipeline creation, exclusive for merges and destruction.
    gp::thread::SharedMutex m_globalPipelineCacheMutex{};

    /// @brief Global driver pipeline cache.
    VkPipelineCache m_globalPipelineCache{ VK_NULL_HANDLE };

    /// @brief Guards the graphics map and the LRU.
    mutable gp::thread::Mutex m_graphicsMutex{};

    /// @brief Graphics pipelines.
    gp::Vector<GraphicsEntry> m_graphicsPipelines{};

    /// @brief Guards the compute map.
    mutable gp::thread::SharedMutex m_computeMutex{};

    /// @brief Compute pipelines.
    gp::Vector<ComputeEntry> m_computePipelines{};

    /// @brief Most recently used pipeline.
    VulkanGraphicsPipelineState* m_lruHead{ nullptr };

    /// @brief Least recently used pipeline.
    VulkanGraphicsPipelineState* m_lruTail{ nullptr };

    /// @brief Number of linked pipelines.
    gp::UInt32 m_lruCount{ 0u };

    /// @brief Total charge of linked pipelines.
    gp::UInt64 m_lruSizeInBytes{ 0u };

    /// @brief Frame LRU touches are stamped with.
    gp::thread::Atomic<gp::UInt64> m_frameNumber{ 0u };
};

}   // namespace gp::rhi::vulkan
