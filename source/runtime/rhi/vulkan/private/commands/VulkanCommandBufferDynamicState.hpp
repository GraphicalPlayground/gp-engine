// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "pipeline/VulkanGraphicsPipelineDescTypes.hpp"
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Largest number of viewports (and scissor rectangles) the pending graphics state tracks.
/// @details Matches the `maxViewports` value every desktop driver reports; multi-view rendering never needs more.
inline constexpr gp::UInt32 kVulkanMaxPendingViewports = 16u;

/// @brief Copy of the pipeline fields that are set through dynamic state commands rather than baked into the pipeline.
/// @details The last values recorded on a command buffer are kept here so that binding a second pipeline only records
///          the dynamic state commands whose values actually differ. Everything is stored inline in fixed arrays: the
///          snapshot is refreshed on every draw that changes pipelines, so it must never allocate.
struct GP_RHI_VULKAN_API VulkanDynamicPipelineStateSnapshot
{
    /// @brief `VkPrimitiveTopology`; `VK_PRIMITIVE_TOPOLOGY_MAX_ENUM` forces the next pipeline to set it.
    gp::UInt32 topology{ VK_PRIMITIVE_TOPOLOGY_MAX_ENUM };

    /// @brief Cull mode, polygon mode and depth bias.
    VulkanRasterizerDesc rasterizer{};

    /// @brief Depth and stencil test state.
    VulkanDepthStencilDesc depthStencil{};

    /// @brief Rasterization sample count.
    gp::UInt16 rasterizationSamples{ 0u };

    /// @brief Alpha-to-coverage enable.
    gp::UInt8 useAlphaToCoverage{ 0u };

    /// @brief Combined shader key; a different pixel shader may write a different set of render targets.
    gp::UInt64 shaderKeyShared{ 0u };

    /// @brief Blend state per color attachment; only the first `numColorAttachments` entries are meaningful.
    gp::Array<VulkanBlendAttachmentDesc, kMaxSimultaneousRenderTargets> colorAttachmentStates{};

    /// @brief Number of color attachments.
    gp::UInt32 numColorAttachments{ 0u };

    /// @brief Vertex bindings; only the first `numVertexBindings` entries are meaningful.
    gp::Array<VulkanVertexBindingDesc, kMaxVertexElementCount> vertexBindings{};

    /// @brief Number of vertex bindings.
    gp::UInt32 numVertexBindings{ 0u };

    /// @brief Vertex attributes; only the first `numVertexAttributes` entries are meaningful.
    gp::Array<VulkanVertexAttributeDesc, kMaxVertexElementCount> vertexAttributes{};

    /// @brief Number of vertex attributes.
    gp::UInt32 numVertexAttributes{ 0u };

    /// @brief Reports whether the color attachment blend states equal those of `desc`.
    /// @param[in] desc Pipeline description.
    /// @return True when count and every entry match.
    [[nodiscard]] bool hasSameColorAttachments(const VulkanGraphicsPipelineDesc& desc) const noexcept;

    /// @brief Copies the color attachment blend states of `desc`.
    /// @param[in] desc Pipeline description; at most `kMaxSimultaneousRenderTargets` attachments are kept.
    void captureColorAttachments(const VulkanGraphicsPipelineDesc& desc) noexcept;

    /// @brief Reports whether the vertex bindings and attributes equal those of `desc`.
    /// @param[in] desc Pipeline description.
    /// @return True when counts and every entry match.
    [[nodiscard]] bool hasSameVertexInput(const VulkanGraphicsPipelineDesc& desc) const noexcept;

    /// @brief Copies the vertex bindings and attributes of `desc`.
    /// @param[in] desc Pipeline description; at most `kMaxVertexElementCount` of each are kept.
    void captureVertexInput(const VulkanGraphicsPipelineDesc& desc) noexcept;
};

/// @brief Dynamic state last recorded on one command buffer.
/// @details Vulkan keeps dynamic state per command buffer, not per pipeline: a value set once stays valid across
///          pipeline binds until the command buffer ends. Several pending states may record into the same command
///          buffer, so this cache lives on the command buffer and is reset whenever recording restarts.
///
///          Every `update*` method compares the requested value with the cached one, stores it, and reports whether
///          the caller has to record the matching `vkCmdSet*` command. The `has*` flags make the first update of a
///          command buffer always record, whatever the cached bytes happen to hold.
struct GP_RHI_VULKAN_API VulkanCommandBufferDynamicState
{
    /// @brief Viewports as requested by the RHI (not Y-flipped); first `numViewports` entries are meaningful.
    gp::Array<VkViewport, kVulkanMaxPendingViewports> viewports{};

    /// @brief Number of recorded viewports.
    gp::UInt32 numViewports{ 0u };

    /// @brief Scissor rectangles; first `numScissors` entries are meaningful.
    gp::Array<VkRect2D, kVulkanMaxPendingViewports> scissors{};

    /// @brief Number of recorded scissor rectangles.
    gp::UInt32 numScissors{ 0u };

    /// @brief Stencil reference for both faces.
    gp::UInt32 stencilRef{ 0u };

    /// @brief Blend constants (RGBA).
    gp::Array<gp::Float32, 4u> blendConstants{};

    /// @brief Lower depth bound.
    gp::Float32 minDepthBounds{ 0.0f };

    /// @brief Upper depth bound.
    gp::Float32 maxDepthBounds{ 1.0f };

    /// @brief Pipeline fields last recorded through dynamic state commands.
    VulkanDynamicPipelineStateSnapshot lastPipelineState{};

    /// @brief Viewports were recorded since the last reset.
    bool hasViewport{ false };

    /// @brief Scissors were recorded since the last reset.
    bool hasScissor{ false };

    /// @brief The stencil reference was recorded since the last reset.
    bool hasStencilRef{ false };

    /// @brief Blend constants were recorded since the last reset.
    bool hasBlendConstants{ false };

    /// @brief Depth bounds were recorded since the last reset.
    bool hasDepthBounds{ false };

    /// @brief Every pipeline dynamic state must be recorded on the next draw, whatever `lastPipelineState` holds.
    bool needsFullUpdate{ true };

    /// @brief Forgets everything recorded; the next draw records every dynamic state again.
    void reset() noexcept;

    /// @brief Stores the viewports and reports whether they must be recorded.
    /// @param[in] newViewports Viewports to set; `count` entries.
    /// @param[in] count Number of viewports; clamped to `kVulkanMaxPendingViewports`.
    /// @return True when nothing was recorded yet or the viewports differ.
    [[nodiscard]] bool updateViewports(const VkViewport* newViewports, gp::UInt32 count) noexcept;

    /// @brief Stores the scissor rectangles and reports whether they must be recorded.
    /// @param[in] newScissors Rectangles to set; `count` entries.
    /// @param[in] count Number of rectangles; clamped to `kVulkanMaxPendingViewports`.
    /// @return True when nothing was recorded yet or the rectangles differ.
    [[nodiscard]] bool updateScissors(const VkRect2D* newScissors, gp::UInt32 count) noexcept;

    /// @brief Stores the stencil reference and reports whether it must be recorded.
    /// @param[in] newStencilRef Reference value.
    /// @return True when nothing was recorded yet or the value differs.
    [[nodiscard]] bool updateStencilRef(gp::UInt32 newStencilRef) noexcept;

    /// @brief Stores the blend constants and reports whether they must be recorded.
    /// @param[in] newBlendConstants RGBA constants.
    /// @return True when nothing was recorded yet or a component differs.
    [[nodiscard]] bool updateBlendConstants(const gp::Array<gp::Float32, 4u>& newBlendConstants) noexcept;

    /// @brief Stores the depth bounds and reports whether they must be recorded.
    /// @param[in] newMinDepthBounds Lower bound.
    /// @param[in] newMaxDepthBounds Upper bound.
    /// @return True when nothing was recorded yet or a bound differs.
    [[nodiscard]] bool updateDepthBounds(gp::Float32 newMinDepthBounds, gp::Float32 newMaxDepthBounds) noexcept;
};

}   // namespace gp::rhi::vulkan
