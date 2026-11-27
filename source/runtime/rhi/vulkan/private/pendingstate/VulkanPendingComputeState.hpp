// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                 // IWYU pragma: keep
#include "CoreMinimal.hpp"                              // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "pendingstate/VulkanPendingStateForward.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "pipeline/VulkanPipelineForward.hpp"           // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"                            // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Compute state of a command context, flushed into the command buffer before every dispatch.
/// @details Keeps one descriptor state per compute pipeline the context has bound, so returning to a pipeline finds
///          its bindings (and, on the cache path, its sets) where it left them. Setting a pipeline only switches the
///          current state; nothing is recorded until `prepareForDispatch()`, which uploads packed globals, updates
///          the sets, binds the pipeline and binds the sets, in that order.
/// @note Not thread-safe; owned by one command context on the recording thread.
class GP_RHI_VULKAN_API VulkanPendingComputeState
{
public:
    /// @brief Creates an empty state.
    /// @param[in] device Owning device; must outlive the state.
    /// @param[in] context Command context services; must outlive the state.
    /// @param[in] defaults Default and global samplers and views given to every descriptor state.
    VulkanPendingComputeState(
        VulkanDevice& device, IVulkanPendingStateContext& context, const VulkanDescriptorDefaults& defaults
    ) noexcept;

    /// @brief Destroys every descriptor state.
    ~VulkanPendingComputeState() noexcept;

    VulkanPendingComputeState(const VulkanPendingComputeState&) = delete;
    VulkanPendingComputeState& operator=(const VulkanPendingComputeState&) = delete;
    VulkanPendingComputeState(VulkanPendingComputeState&&) = delete;
    VulkanPendingComputeState& operator=(VulkanPendingComputeState&&) = delete;

    /// @brief Makes `pipeline` current, creating its descriptor state on first use.
    /// @details Switching to another pipeline resets the descriptor state it switches to: its sets and packed globals
    ///          may refer to memory recycled since it was last current.
    /// @param[in] pipeline Valid compute pipeline; must outlive its use by this state (see `notifyDeletedPipeline()`).
    void setComputePipeline(const VulkanComputePipelineState& pipeline) noexcept;

    /// @brief Records everything the current pipeline needs before a dispatch.
    /// @param[in,out] commandBuffer Command buffer in the recording state, outside a render pass.
    /// @return False when no pipeline is current or the descriptor sets could not be updated.
    [[nodiscard]] bool prepareForDispatch(VulkanCommandBuffer& commandBuffer) noexcept;

    /// @brief Drops the descriptor state of a pipeline that is being destroyed.
    /// @param[in] pipeline Pipeline being destroyed.
    void notifyDeletedPipeline(const VulkanComputePipelineState& pipeline) noexcept;

    /// @brief Forgets the current pipeline; the next dispatch needs a new `setComputePipeline()`.
    void reset() noexcept;

    /// @brief Returns the current pipeline.
    /// @return Pipeline, or null.
    [[nodiscard]] const VulkanComputePipelineState* getCurrentPipeline() const noexcept
    {
        return m_currentPipeline;
    }

    /// @brief Returns the descriptor state of the current pipeline; shader parameters are set through it.
    /// @return Descriptor state, or null when no pipeline is current.
    [[nodiscard]] VulkanComputePipelineDescriptorState* getCurrentState() const noexcept
    {
        return m_currentState;
    }

    /// @brief Returns the number of pipelines with a descriptor state.
    /// @return Descriptor state count.
    [[nodiscard]] gp::USize getNumPipelineStates() const noexcept
    {
        return static_cast<gp::USize>(m_pipelineStates.size());
    }

private:
    /// @brief Descriptor state of one pipeline.
    struct Entry
    {
        /// @brief Pipeline the state belongs to.
        const VulkanComputePipelineState* pipeline{ nullptr };

        /// @brief Descriptor state.
        gp::UniquePtr<VulkanComputePipelineDescriptorState> state{};
    };

    /// @brief Owning device.
    VulkanDevice& m_device;

    /// @brief Command context services.
    IVulkanPendingStateContext& m_context;

    /// @brief Default and global samplers and views.
    VulkanDescriptorDefaults m_defaults{};

    /// @brief Descriptor state per pipeline; a linear table, a context binds few compute pipelines.
    gp::Vector<Entry> m_pipelineStates{};

    /// @brief Current pipeline.
    const VulkanComputePipelineState* m_currentPipeline{ nullptr };

    /// @brief Descriptor state of the current pipeline.
    VulkanComputePipelineDescriptorState* m_currentState{ nullptr };

    /// @brief True when new descriptor states track hashable infos for the set cache.
    bool m_useDescriptorCache{ false };
};

}   // namespace gp::rhi::vulkan
