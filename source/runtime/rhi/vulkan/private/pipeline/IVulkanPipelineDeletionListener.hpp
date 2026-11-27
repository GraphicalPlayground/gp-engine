// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Receives a callback right before a native pipeline state object is destroyed.
/// @details Command contexts keep raw pointers to the pipeline they last bound so redundant binds can be skipped. When
///          the last wrapper of a pipeline dies, those pointers must be cleared before the memory is reused, otherwise
///          a new pipeline allocated at the same address would be mistaken for the old one. The pipeline cache manager
///          cannot do this itself: it forgets a pipeline before the pipeline is destroyed, so the notification is
///          raised by the pipeline destructors through `VulkanPipelineDeletionNotifier`.
/// @note Callbacks run on whichever thread drops the last reference; implementations must be thread-safe.
class GP_RHI_VULKAN_API IVulkanPipelineDeletionListener
{
public:
    /// @brief Destroys the listener.
    virtual ~IVulkanPipelineDeletionListener() noexcept = default;

public:
    /// @brief Called by a graphics pipeline state while it is being destroyed.
    /// @param[in] pipeline Pipeline being destroyed; must not be retained.
    virtual void onGraphicsPipelineDeleted(const VulkanGraphicsPipelineState& pipeline) noexcept = 0;

    /// @brief Called by a compute pipeline state while it is being destroyed.
    /// @param[in] pipeline Pipeline being destroyed; must not be retained.
    virtual void onComputePipelineDeleted(const VulkanComputePipelineState& pipeline) noexcept = 0;
};

}   // namespace gp::rhi::vulkan
