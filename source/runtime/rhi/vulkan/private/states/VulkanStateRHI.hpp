// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclarationDesc.hpp"
#include "RHIVulkan.hpp"
#include "states/VulkanStateForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Creates (or reuses) a sampler state.
/// @details Entry point behind `DynamicRHI::createSamplerState`. Identical native create infos share one sampler.
/// @param[in,out] cache Device sampler cache.
/// @param[in] desc RHI sampler description.
/// @return Shared sampler state.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::SamplerStateRef
    createVulkanSamplerState(VulkanSamplerStateCache& cache, const pipeline::SamplerStateDesc& desc) noexcept;

/// @brief Creates a rasterizer state.
/// @details Entry point behind `DynamicRHI::createRasterizerState`.
/// @param[in] desc RHI rasterizer description.
/// @return New rasterizer state.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::RasterizerStateRef
    createVulkanRasterizerState(const pipeline::RasterizerStateDesc& desc) noexcept;

/// @brief Creates a depth-stencil state.
/// @details Entry point behind `DynamicRHI::createDepthStencilState`.
/// @param[in] desc RHI depth-stencil description.
/// @return New depth-stencil state.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::DepthStencilStateRef
    createVulkanDepthStencilState(const pipeline::DepthStencilStateDesc& desc) noexcept;

/// @brief Creates a blend state.
/// @details Entry point behind `DynamicRHI::createBlendState`.
/// @param[in] desc RHI blend description.
/// @return New blend state.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::BlendStateRef
    createVulkanBlendState(const pipeline::BlendStateDesc& desc) noexcept;

/// @brief Creates (or reuses) a vertex declaration.
/// @details Entry point behind `DynamicRHI::createVertexDeclaration`. Identical element lists share one declaration.
/// @param[in] elements Vertex declaration elements.
/// @return Shared vertex declaration.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::VertexDeclarationRef
    createVulkanVertexDeclaration(const pipeline::VertexDeclarationElementList& elements) noexcept;

}   // namespace gp::rhi::vulkan
