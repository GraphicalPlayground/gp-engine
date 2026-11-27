// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

class IVulkanPipelineDeletionListener;

/// @brief Installs (or clears with `nullptr`) the process-wide pipeline deletion listener.
/// @details There is one listener per process because there is one active RHI: the dynamic RHI registers itself in
///          `init()` and clears the slot in `shutdown()`. Pipelines created by standalone managers (tests, tools)
///          simply notify nobody when no listener is installed.
/// @param[in] listener Listener to install; `nullptr` disables notifications.
/// @return Listener that was installed before the call.
GP_RHI_VULKAN_API IVulkanPipelineDeletionListener*
    setVulkanPipelineDeletionListener(IVulkanPipelineDeletionListener* listener) noexcept;

/// @brief Returns the installed pipeline deletion listener.
/// @return Active listener, or `nullptr`.
[[nodiscard]] GP_RHI_VULKAN_API IVulkanPipelineDeletionListener* getVulkanPipelineDeletionListener() noexcept;

/// @brief Forwards the destruction of a graphics pipeline to the installed listener, if any.
/// @param[in] pipeline Pipeline being destroyed.
GP_RHI_VULKAN_API void notifyVulkanGraphicsPipelineDeleted(const VulkanGraphicsPipelineState& pipeline) noexcept;

/// @brief Forwards the destruction of a compute pipeline to the installed listener, if any.
/// @param[in] pipeline Pipeline being destroyed.
GP_RHI_VULKAN_API void notifyVulkanComputePipelineDeleted(const VulkanComputePipelineState& pipeline) noexcept;

}   // namespace gp::rhi::vulkan
