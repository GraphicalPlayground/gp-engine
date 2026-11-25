// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/PoolMoveRequest.hpp"
#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Observer notified when the backing allocation of a viewable resource changes.
/// @details Used by objects that cache a resource's handle or device address without being a view, such as
///          bindless descriptor tables or ray tracing geometry.
/// @note Registration and notification happen on the RHI timeline only.
class IVulkanResourceRenameListener
{
public:
    /// @brief Virtual destructor.
    virtual ~IVulkanResourceRenameListener() noexcept = default;

    /// @brief Called after `renamedResource` switched to a new allocation.
    /// @param[in] contexts Pipeline contexts active during the rename.
    /// @param[in] renamedResource Resource whose allocation changed.
    virtual void
        resourceRenamed(const allocator::ContextArray& contexts, VulkanViewableResource* renamedResource) noexcept = 0;
};

}   // namespace gp::rhi::vulkan

