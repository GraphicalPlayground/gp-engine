// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanStateRHI.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"   // IWYU pragma: keep
#include "states/VulkanBlendState.hpp"
#include "states/VulkanDepthStencilState.hpp"
#include "states/VulkanRasterizerState.hpp"
#include "states/VulkanSamplerStateCache.hpp"
#include "states/VulkanVertexDeclaration.hpp"

namespace gp::rhi::vulkan
{

pipeline::SamplerStateRef
    createVulkanSamplerState(VulkanSamplerStateCache& cache, const pipeline::SamplerStateDesc& desc) noexcept
{
    return cache.findOrCreate(desc);
}

pipeline::RasterizerStateRef createVulkanRasterizerState(const pipeline::RasterizerStateDesc& desc) noexcept
{
    return pipeline::RasterizerStateRef(new VulkanRasterizerState(desc));
}

pipeline::DepthStencilStateRef createVulkanDepthStencilState(const pipeline::DepthStencilStateDesc& desc) noexcept
{
    return pipeline::DepthStencilStateRef(new VulkanDepthStencilState(desc));
}

pipeline::BlendStateRef createVulkanBlendState(const pipeline::BlendStateDesc& desc) noexcept
{
    return pipeline::BlendStateRef(new VulkanBlendState(desc));
}

pipeline::VertexDeclarationRef
    createVulkanVertexDeclaration(const pipeline::VertexDeclarationElementList& elements) noexcept
{
    return VulkanVertexDeclaration::findOrCreate(elements);
}

}   // namespace gp::rhi::vulkan
