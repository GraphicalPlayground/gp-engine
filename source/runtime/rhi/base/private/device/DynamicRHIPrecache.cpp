// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/DynamicRHI.hpp"
#include "hashing/Fnv1a.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "shaders/Shader.hpp"

namespace gp::rhi
{

namespace
{

void hashShaderStage(gp::hashing::Fnv1a64& hasher, const shader::Shader* const shader) noexcept
{
    if (shader == nullptr)
    {
        hasher.append(static_cast<gp::UInt64>(0u));
        hasher.append(static_cast<gp::UInt64>(0u));
        return;
    }

    const shader::ShaderHash hash = shader->getHash();
    if (hash.isValid())
    {
        hasher.append(hash.low);
        hasher.append(hash.high);
    }
    else
    {
        hasher.append(reinterpret_cast<gp::UIntPtr>(shader));
    }
}

void hashSpecializationConstants(
    gp::hashing::Fnv1a64& hasher, const shader::SpecializationContainer& constants
) noexcept
{
    hasher.append(static_cast<gp::UInt64>(constants.size()));
    for (const shader::SpecializationKey key: constants)
    {
        hasher.append(key);
    }
}

template <typename InStateType, typename InDescType>
[[nodiscard]] bool matchStateContent(const InStateType* const lhs, const InStateType* const rhs) noexcept
{
    if (lhs == rhs)
    {
        return true;
    }
    if (lhs == nullptr || rhs == nullptr)
    {
        return false;
    }
    InDescType lhsDesc{};
    InDescType rhsDesc{};
    if (lhs->getInitializer(lhsDesc) && rhs->getInitializer(rhsDesc))
    {
        return lhsDesc == rhsDesc;
    }
    return false;
}

[[nodiscard]] bool matchShaderStage(const shader::Shader* const lhs, const shader::Shader* const rhs) noexcept
{
    if (lhs == rhs)
    {
        return true;
    }
    if (lhs == nullptr || rhs == nullptr)
    {
        return false;
    }
    const shader::ShaderHash lhsHash = lhs->getHash();
    const shader::ShaderHash rhsHash = rhs->getHash();
    if (lhsHash.isValid() && rhsHash.isValid())
    {
        return lhsHash == rhsHash;
    }
    return false;
}

}   // namespace

gp::UInt64
    DynamicRHI::computeStatePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept
{
    gp::hashing::Fnv1a64 hasher{};

    const pipeline::BoundShaderStateInput& bss = initializer.boundShaderState;
    if (const pipeline::VertexDeclaration* const vdecl = bss.getVertexDeclaration(); vdecl != nullptr)
    {
        const gp::UInt32 declHash = vdecl->getPrecachePSOHash();
        if (declHash != 0u)
        {
            hasher.append(declHash);
        }
        else
        {
            pipeline::VertexDeclarationElementList elements{};
            if (vdecl->getInitializer(elements))
            {
                hasher.append(static_cast<gp::UInt64>(elements.size()));
                for (const pipeline::VertexElement& elem: elements)
                {
                    hasher.append(elem.streamIndex);
                    hasher.append(elem.offset);
                    hasher.append(static_cast<gp::UInt8>(elem.type));
                    hasher.append(elem.attributeIndex);
                    hasher.append(elem.stride);
                    hasher.append(elem.useInstanceIndex);
                }
            }
            else
            {
                hasher.append(reinterpret_cast<gp::UIntPtr>(vdecl));
            }
        }
    }

    hashShaderStage(hasher, bss.getVertexShader());
    hashShaderStage(hasher, bss.getPixelShader());
    hashShaderStage(hasher, bss.getGeometryShader());
    hashShaderStage(hasher, bss.getMeshShader());
    hashShaderStage(hasher, bss.getAmplificationShader());
    hashSpecializationConstants(hasher, bss.vertexShaderSpecializationConstants);
    hashSpecializationConstants(hasher, bss.pixelShaderSpecializationConstants);
    hashSpecializationConstants(hasher, bss.meshShaderSpecializationConstants);
    hashSpecializationConstants(hasher, bss.amplificationShaderSpecializationConstants);
    hashSpecializationConstants(hasher, bss.geometryShaderSpecializationConstants);

    pipeline::BlendStateDesc blendDesc{};
    if (initializer.blendState != nullptr && initializer.blendState->getInitializer(blendDesc))
    {
        hasher.append(blendDesc.useIndependentRenderTargetBlendStates);
        hasher.append(blendDesc.useAlphaToCoverage);
        for (gp::UInt32 i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            const pipeline::BlendStateDesc::RenderTarget& rt = blendDesc.renderTargets[i];
            hasher.append(static_cast<gp::UInt8>(rt.colorBlendOp));
            hasher.append(static_cast<gp::UInt8>(rt.colorSrcBlend));
            hasher.append(static_cast<gp::UInt8>(rt.colorDestBlend));
            hasher.append(static_cast<gp::UInt8>(rt.alphaBlendOp));
            hasher.append(static_cast<gp::UInt8>(rt.alphaSrcBlend));
            hasher.append(static_cast<gp::UInt8>(rt.alphaDestBlend));
            hasher.append(static_cast<gp::UInt8>(rt.colorWriteMask));
        }
    }
    else
    {
        hasher.append(reinterpret_cast<gp::UIntPtr>(initializer.blendState));
    }

    pipeline::RasterizerStateDesc rasterDesc{};
    if (initializer.rasterizerState != nullptr && initializer.rasterizerState->getInitializer(rasterDesc))
    {
        hasher.append(static_cast<gp::UInt8>(rasterDesc.fillMode));
        hasher.append(static_cast<gp::UInt8>(rasterDesc.cullMode));
        hasher.append(rasterDesc.depthBias);
        hasher.append(rasterDesc.slopeScaleDepthBias);
        hasher.append(static_cast<gp::UInt8>(rasterDesc.depthClipMode));
        hasher.append(rasterDesc.allowMSAA);
    }
    else
    {
        hasher.append(reinterpret_cast<gp::UIntPtr>(initializer.rasterizerState));
    }

    pipeline::DepthStencilStateDesc dsDesc{};
    if (initializer.depthStencilState != nullptr && initializer.depthStencilState->getInitializer(dsDesc))
    {
        dsDesc.hashAppend(hasher);
    }
    else
    {
        hasher.append(reinterpret_cast<gp::UIntPtr>(initializer.depthStencilState));
    }

    hasher.append(static_cast<gp::UInt8>(initializer.primitiveType));
    hasher.append(static_cast<gp::UInt8>(initializer.conservativeRasterization));
    hasher.append(initializer.depthBounds);
    hasher.append(initializer.allowVariableRateShading);
    hasher.append(static_cast<gp::UInt8>(initializer.shadingRate));
    hasher.append(initializer.multiViewCount);
    hasher.append(initializer.hasFragmentDensityAttachment);
    return hasher.finalize();
}

gp::UInt64 DynamicRHI::computePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept
{
    const gp::UInt64 stateHash = (initializer.statePrecachePSOHash != 0u) ? initializer.statePrecachePSOHash
                                                                          : computeStatePrecachePSOHash(initializer);

    gp::hashing::Fnv1a64 hasher{};
    hasher.append(stateHash);
    hasher.append(initializer.renderTargetsEnabled);
    const gp::UInt32 rtCount = (initializer.renderTargetsEnabled <= kMaxSimultaneousRenderTargets)
                                   ? initializer.renderTargetsEnabled
                                   : kMaxSimultaneousRenderTargets;
    for (gp::UInt32 i = 0u; i < rtCount; ++i)
    {
        hasher.append(static_cast<gp::UInt8>(initializer.renderTargetFormats[i]));
        hasher.append(
            static_cast<gp::UInt64>(
                initializer.renderTargetFlags[i] &
                pipeline::GraphicsPipelineStateInitializer::kRelevantRenderTargetFlagMask
            )
        );
    }
    hasher.append(static_cast<gp::UInt8>(initializer.depthStencilTargetFormat));
    hasher.append(
        static_cast<gp::UInt64>(
            initializer.depthStencilTargetFlag &
            pipeline::GraphicsPipelineStateInitializer::kRelevantDepthStencilFlagMask
        )
    );
    hasher.append(static_cast<gp::UInt8>(initializer.depthTargetLoadAction));
    hasher.append(static_cast<gp::UInt8>(initializer.depthTargetStoreAction));
    hasher.append(static_cast<gp::UInt8>(initializer.stencilTargetLoadAction));
    hasher.append(static_cast<gp::UInt8>(initializer.stencilTargetStoreAction));
    initializer.depthStencilAccess.hashAppend(hasher);
    hasher.append(initializer.numSamples);
    hasher.append(static_cast<gp::UInt8>(initializer.subpassHint));
    hasher.append(initializer.subpassIndex);
    return hasher.finalize();
}

bool DynamicRHI::matchPrecachePSOInitializers(
    const pipeline::GraphicsPipelineStateInitializer& lhs, const pipeline::GraphicsPipelineStateInitializer& rhs
) noexcept
{
    const pipeline::BoundShaderStateInput& lhsBss = lhs.boundShaderState;
    const pipeline::BoundShaderStateInput& rhsBss = rhs.boundShaderState;

    if (!matchStateContent<pipeline::VertexDeclaration, pipeline::VertexDeclarationElementList>(
            lhsBss.getVertexDeclaration(), rhsBss.getVertexDeclaration()
        ) ||
        !matchShaderStage(lhsBss.getVertexShader(), rhsBss.getVertexShader()) ||
        !matchShaderStage(lhsBss.getPixelShader(), rhsBss.getPixelShader()) ||
        !matchShaderStage(lhsBss.getGeometryShader(), rhsBss.getGeometryShader()) ||
        !matchShaderStage(lhsBss.getMeshShader(), rhsBss.getMeshShader()) ||
        !matchShaderStage(lhsBss.getAmplificationShader(), rhsBss.getAmplificationShader()) ||
        lhsBss.vertexShaderSpecializationConstants != rhsBss.vertexShaderSpecializationConstants ||
        lhsBss.pixelShaderSpecializationConstants != rhsBss.pixelShaderSpecializationConstants ||
        lhsBss.meshShaderSpecializationConstants != rhsBss.meshShaderSpecializationConstants ||
        lhsBss.amplificationShaderSpecializationConstants != rhsBss.amplificationShaderSpecializationConstants ||
        lhsBss.geometryShaderSpecializationConstants != rhsBss.geometryShaderSpecializationConstants)
    {
        return false;
    }

    if (!matchStateContent<pipeline::BlendState, pipeline::BlendStateDesc>(lhs.blendState, rhs.blendState) ||
        !matchStateContent<pipeline::RasterizerState, pipeline::RasterizerStateDesc>(
            lhs.rasterizerState, rhs.rasterizerState
        ) ||
        !matchStateContent<pipeline::DepthStencilState, pipeline::DepthStencilStateDesc>(
            lhs.depthStencilState, rhs.depthStencilState
        ))
    {
        return false;
    }

    if (lhs.primitiveType != rhs.primitiveType || lhs.renderTargetsEnabled != rhs.renderTargetsEnabled ||
        lhs.depthStencilTargetFormat != rhs.depthStencilTargetFormat ||
        ((lhs.depthStencilTargetFlag & pipeline::GraphicsPipelineStateInitializer::kRelevantDepthStencilFlagMask) !=
         (rhs.depthStencilTargetFlag & pipeline::GraphicsPipelineStateInitializer::kRelevantDepthStencilFlagMask)) ||
        lhs.depthTargetLoadAction != rhs.depthTargetLoadAction ||
        lhs.depthTargetStoreAction != rhs.depthTargetStoreAction ||
        lhs.stencilTargetLoadAction != rhs.stencilTargetLoadAction ||
        lhs.stencilTargetStoreAction != rhs.stencilTargetStoreAction ||
        lhs.depthStencilAccess != rhs.depthStencilAccess || lhs.numSamples != rhs.numSamples ||
        lhs.subpassHint != rhs.subpassHint || lhs.subpassIndex != rhs.subpassIndex ||
        lhs.conservativeRasterization != rhs.conservativeRasterization || lhs.multiViewCount != rhs.multiViewCount ||
        lhs.depthBounds != rhs.depthBounds || lhs.hasFragmentDensityAttachment != rhs.hasFragmentDensityAttachment ||
        lhs.allowVariableRateShading != rhs.allowVariableRateShading || lhs.shadingRate != rhs.shadingRate)
    {
        return false;
    }

    const gp::UInt32 count = (lhs.renderTargetsEnabled <= kMaxSimultaneousRenderTargets)
                                 ? lhs.renderTargetsEnabled
                                 : kMaxSimultaneousRenderTargets;
    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        if (lhs.renderTargetFormats[i] != rhs.renderTargetFormats[i] ||
            ((lhs.renderTargetFlags[i] & pipeline::GraphicsPipelineStateInitializer::kRelevantRenderTargetFlagMask) !=
             (rhs.renderTargetFlags[i] & pipeline::GraphicsPipelineStateInitializer::kRelevantRenderTargetFlagMask)))
        {
            return false;
        }
    }

    return true;
}

}   // namespace gp::rhi
