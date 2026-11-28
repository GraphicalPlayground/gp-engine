// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/DynamicRHIPSOFallback.hpp"
#include "pipeline/PipelineStateFallbacks.hpp"
#include <utility>

namespace gp::rhi
{

pipeline::GraphicsPipelineStateRef DynamicRHIPSOFallback::createGraphicsPipelineState(
    const pipeline::GraphicsPipelineStateInitializer& initializer
) noexcept
{
    pipeline::BoundShaderStateRef boundState{};
    if (initializer.boundShaderState.usesMeshPipeline())
    {
        boundState = createBoundShaderState(
            initializer.boundShaderState.getAmplificationShader(),
            initializer.boundShaderState.getMeshShader(),
            initializer.boundShaderState.getPixelShader()
        );
    }
    else
    {
        boundState = createBoundShaderState(
            initializer.boundShaderState.getVertexDeclaration(),
            initializer.boundShaderState.getVertexShader(),
            initializer.boundShaderState.getPixelShader(),
            initializer.boundShaderState.getGeometryShader()
        );
    }
    return pipeline::GraphicsPipelineStateRef(
        new pipeline::GraphicsPipelineStateFallback(initializer, std::move(boundState))
    );
}

pipeline::ComputePipelineStateRef
    DynamicRHIPSOFallback::createComputePipelineState(shader::ComputeShader* const computeShader) noexcept
{
    return pipeline::ComputePipelineStateRef(new pipeline::ComputePipelineStateFallback(computeShader));
}

}   // namespace gp::rhi
