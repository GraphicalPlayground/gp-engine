// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/SyncForward.hpp"

namespace gp::rhi::vulkan
{

/// @brief Interface mapping RHI resources referenced by transitions to their Vulkan handles.
/// @details Barrier generation needs the `VkImage` or `VkBuffer` behind each `sync::TransitionInfo`, plus a few
///          format and usage facts. This interface keeps that lookup out of the conversion code, so the conversion
///          does not depend on the concrete Vulkan texture, buffer and view classes and can be tested with fakes.
class GP_RHI_VULKAN_API IVulkanBarrierResourceResolver
{
public:
    /// @brief Virtual destructor.
    virtual ~IVulkanBarrierResourceResolver() noexcept = default;

    /// @brief Resolves the Vulkan object behind `info`.
    /// @details Expected results by `info.type`:
    ///          - `Texture`: `Texture`, or `ReadbackTexture` for CPU readback textures backed by a buffer.
    ///          - `Buffer`: `Buffer`.
    ///          - `UAV`: `Texture` or `Buffer`, depending on the viewed resource.
    ///          - `BVH`: `BVH`.
    /// @param[in] info Transition entry with a non-null resource.
    /// @return Resolved Vulkan resource.
    [[nodiscard]] virtual VulkanBarrierResource resolve(const sync::TransitionInfo& info) const noexcept = 0;
};

}   // namespace gp::rhi::vulkan
