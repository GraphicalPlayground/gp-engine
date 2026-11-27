// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"                  // IWYU pragma: keep
#include "pipeline/VulkanGraphicsPipelineDescTypes.hpp"
#include "pipeline/VulkanPipelineKey.hpp"
#include "pipeline/VulkanPipelineTypes.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"                     // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native description of a graphics pipeline, the unit of graphics pipeline caching.
/// @details Built once per creation request from the RHI initializer (`buildVulkanGraphicsPipelineDesc`), then
///          serialized into a `VulkanPipelineKey`. Two initializers that translate to the same native state share one
///          `VkPipeline`, even when they use different RHI state objects. The description is kept by the cached
///          pipeline so an evicted pipeline can be recreated without the original initializer.
/// @note Every field is a plain value or an array of plain values; shader identity is the 64-bit shader key of each
///       stage (CRC and size of its SPIR-V), not the shader object.
struct GP_RHI_VULKAN_API VulkanGraphicsPipelineDesc
{
    /// @brief Rasterization sample count (0 or 1 for single sampled).
    gp::UInt16 rasterizationSamples{ 0u };

    /// @brief `VkPrimitiveTopology`.
    gp::UInt32 topology{ 0u };

    /// @brief One blend attachment per color render target.
    gp::Vector<VulkanBlendAttachmentDesc> colorAttachmentStates{};

    /// @brief Descriptor bindings per descriptor set of the pipeline layout.
    gp::Vector<gp::Vector<VulkanDescriptorSetLayoutBindingDesc>> descriptorSetLayoutBindings{};

    /// @brief Vertex input bindings.
    gp::Vector<VulkanVertexBindingDesc> vertexBindings{};

    /// @brief Vertex input attributes.
    gp::Vector<VulkanVertexAttributeDesc> vertexAttributes{};

    /// @brief Bit per vertex stream that has a binding.
    gp::UInt32 vertexStreamMask{ 0u };

    /// @brief Rasterization state.
    VulkanRasterizerDesc rasterizer{};

    /// @brief Depth-stencil state.
    VulkanDepthStencilDesc depthStencil{};

    /// @brief Shader key per graphics stage, indexed by `shader_stage::Stage`; 0 for an unused stage.
    gp::Array<gp::UInt64, kNumGraphicsPipelineStages> shaderKeys{};

    /// @brief Order-dependent combination of `shaderKeys`; cheap first filter between descriptions.
    gp::UInt64 shaderKeyShared{ 0u };

    /// @brief Hash of the specialization constants of every stage (VS, PS, GS, MS, AS order); 0 without constants.
    gp::UInt64 specializationKey{ 0u };

    /// @brief Render target layout snapshot.
    VulkanRenderTargetsDesc renderTargets{};

    /// @brief Subpass the pipeline is used in.
    gp::UInt8 subpassIndex{ 0u };

    /// @brief Alpha to coverage is enabled (multisampled targets only).
    gp::UInt8 useAlphaToCoverage{ 0u };

    /// @brief Pipeline fragment shading rate.
    pipeline::VRSShadingRate shadingRate{ pipeline::VRSShadingRate::Rate1x1 };

    /// @brief Pipeline to primitive shading rate combiner.
    pipeline::VRSRateCombiner combiner{ pipeline::VRSRateCombiner::Passthrough };

    /// @brief Serializes every field into a cache key.
    /// @details Field order: samples, topology, blend attachments, descriptor bindings, vertex bindings, vertex
    ///          attributes, stream mask, rasterizer, depth-stencil, shader keys, specialization key, render targets,
    ///          shading rate, combiner, alpha to coverage, subpass index. The subpass index is part of the key, since a
    ///          pipeline is only valid for the subpass it was created for.
    /// @return Key with its hash.
    [[nodiscard]] VulkanPipelineKey createKey() const noexcept;

    /// @brief Reports whether the description has a mesh shader (no vertex input, no topology).
    /// @return True when the mesh stage key is non-zero.
    [[nodiscard]] bool hasMeshShader() const noexcept
    {
        return shaderKeys[static_cast<gp::USize>(shader_stage::Stage::Mesh)] != 0u;
    }

    /// @brief Field-by-field equality.
    /// @param[in] rhs Description to compare against.
    /// @return True when every field matches.
    [[nodiscard]] bool operator==(const VulkanGraphicsPipelineDesc& rhs) const noexcept;
};

/// @brief Combines per-stage shader keys into `VulkanGraphicsPipelineDesc::shaderKeyShared`.
/// @details Sum of `key * prime[stage]`: the stage of a key matters, so swapping two stages changes the result.
/// @param[in] shaderKeys Shader key per graphics stage.
/// @return Combined key.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64
    combineVulkanShaderKeys(const gp::Array<gp::UInt64, kNumGraphicsPipelineStages>& shaderKeys) noexcept;

/// @brief Translates an RHI graphics pipeline initializer into its native description.
/// @details Gathers the descriptor bindings of every stage into `outLayoutInfo` (finalized for the graphics bind
///          point), reads the blend, rasterizer and depth-stencil states, builds the vertex input from the vertex
///          declaration and the vertex shader's input mask, snapshots the render target layout and records the shader
///          and specialization keys.
/// @note The initializer must have a blend, rasterizer and depth-stencil state, and at least one shader.
/// @param[in] limits Device limits (descriptor counts).
/// @param[in] caps Render target capabilities used to build the render target layout.
/// @param[in] initializer RHI initializer.
/// @param[out] outLayoutInfo Descriptor set layout info of the pipeline layout.
/// @param[out] outDesc Native description.
/// @return False when the initializer has no shader, a missing fixed-function state, or its bindings cannot be
///         finalized.
[[nodiscard]] GP_RHI_VULKAN_API bool buildVulkanGraphicsPipelineDesc(
    const VkPhysicalDeviceLimits& limits,
    const VulkanRenderTargetCaps& caps,
    const pipeline::GraphicsPipelineStateInitializer& initializer,
    VulkanDescriptorSetsLayoutInfo& outLayoutInfo,
    VulkanGraphicsPipelineDesc& outDesc
) noexcept;

}   // namespace gp::rhi::vulkan
