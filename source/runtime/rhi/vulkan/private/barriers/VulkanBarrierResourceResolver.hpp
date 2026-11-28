// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/IVulkanBarrierResourceResolver.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "RHIVulkan.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep
#include "VulkanForward.hpp"      // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Translates RHI transition targets into the Vulkan handles the barrier builder needs.
/// @details Textures resolve to their image and aspect data (or to the CPU readback buffer for readback textures),
///          buffers to handle, offset, size and usage, and UAVs to the resource they view. BVHs resolve to `None`
///          since only global memory barriers order them. Every cast is static: transition targets on this
///          backend are always Vulkan objects.
/// @note Stateless and thread-safe.
class GP_RHI_VULKAN_API VulkanBarrierResourceResolver final : public IVulkanBarrierResourceResolver
{
public:
    /// @brief Default constructor.
    VulkanBarrierResourceResolver() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~VulkanBarrierResourceResolver() noexcept override = default;

public:
    /// @copydoc IVulkanBarrierResourceResolver::resolve()
    /// @note Returns a `None` resource for null or unknown targets, which the transition processor skips.
    [[nodiscard]] virtual VulkanBarrierResource resolve(const sync::TransitionInfo& info) const noexcept override;

    /// @brief Describes `texture` for the barrier builder.
    /// @param[in] texture Vulkan texture.
    /// @return Texture description, or a `ReadbackTexture` one for CPU readback textures.
    [[nodiscard]] static VulkanBarrierResource resolveTexture(const VulkanTexture& texture) noexcept;

    /// @brief Describes `buffer` for the barrier builder.
    /// @param[in] buffer Vulkan buffer.
    /// @return Buffer description covering its current allocation.
    [[nodiscard]] static VulkanBarrierResource resolveBuffer(const VulkanBuffer& buffer) noexcept;
};

}   // namespace gp::rhi::vulkan
