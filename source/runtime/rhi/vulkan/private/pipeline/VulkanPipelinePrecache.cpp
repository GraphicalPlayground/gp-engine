// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelinePrecache.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/XxHash.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineStateUtils.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/AmplificationShader.hpp"   // IWYU pragma: keep
#include "shaders/GeometryShader.hpp"        // IWYU pragma: keep
#include "shaders/MeshShader.hpp"            // IWYU pragma: keep
#include "shaders/PixelShader.hpp"           // IWYU pragma: keep
#include "shaders/Shader.hpp"
#include "shaders/VertexShader.hpp"          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Feeds one plain value into a hasher.
/// @details Fields are hashed one by one rather than as a packed struct, so padding bytes never reach the hash.
/// @tparam InValueType Trivially copyable value type.
/// @param[in,out] hasher Running hash.
/// @param[in] value Value.
template <typename InValueType>
void hashValue(gp::hashing::XxHash64& hasher, const InValueType& value) noexcept
{
    hasher.update(&value, sizeof(InValueType));
}

/// @brief Folds a 64-bit hash to 32 bits.
/// @param[in] hash Hash.
/// @return Low and high halves combined.
[[nodiscard]] gp::UInt32 foldHash(const gp::UInt64 hash) noexcept
{
    return static_cast<gp::UInt32>(hash ^ (hash >> 32u));
}

/// @brief Folds the 128-bit bytecode hash of a shader to 32 bits.
/// @param[in] shader Shader, or null.
/// @return Folded hash, or 0 for a null shader.
[[nodiscard]] gp::UInt32 hashShader(const shader::Shader* const shader) noexcept
{
    if (shader == nullptr)
    {
        return 0u;
    }
    const shader::ShaderHash& hash = shader->getHash();
    return foldHash(hash.low ^ hash.high);
}

/// @brief Hashes the content of a state object through its initializer.
/// @tparam InStateType RHI state class with `getInitializer`.
/// @tparam InDescType Initializer type with `getHash`.
/// @param[in] state State, or null.
/// @return Folded content hash, or 0 when the state is null or cannot report its initializer.
template <typename InStateType, typename InDescType>
[[nodiscard]] gp::UInt32 hashStateContent(const InStateType* const state) noexcept
{
    InDescType desc{};
    if ((state == nullptr) || !state->getInitializer(desc))
    {
        return 0u;
    }
    return foldHash(desc.getHash());
}

}   // namespace

gp::UInt64 computeVulkanStatePrecachePSOHash(
    const pipeline::GraphicsPipelineStateInitializer& initializer, const gp::rhi::GpuVendorId vendorId
) noexcept
{
    const pipeline::BoundShaderStateInput& bound = initializer.boundShaderState;

    // Arm drivers compile vertex fetch into the shader prologue at bind time, so the declaration does not split PSOs.
    gp::UInt32 vertexDeclarationHash = 0u;
    if ((vendorId != gp::rhi::GpuVendorId::Arm) && (bound.getVertexDeclaration() != nullptr))
    {
        vertexDeclarationHash = bound.getVertexDeclaration()->getPrecachePSOHash();
    }

    const gp::UInt32 blendHash =
        hashStateContent<pipeline::BlendState, pipeline::BlendStateDesc>(initializer.blendState);
    const gp::UInt32 rasterizerHash =
        hashStateContent<pipeline::RasterizerState, pipeline::RasterizerStateDesc>(initializer.rasterizerState);
    const gp::UInt32 depthStencilHash =
        hashStateContent<pipeline::DepthStencilState, pipeline::DepthStencilStateDesc>(initializer.depthStencilState);

    gp::hashing::XxHash64 hasher(0u);
    hashValue(hasher, vertexDeclarationHash);
    hashValue(hasher, hashShader(bound.getVertexShader()));
    hashValue(hasher, hashShader(bound.getPixelShader()));
    hashValue(hasher, hashShader(bound.getGeometryShader()));
    hashValue(hasher, hashShader(bound.getMeshShader()));
    hashValue(hasher, hashShader(bound.getAmplificationShader()));
    hashValue(hasher, blendHash);
    hashValue(hasher, rasterizerHash);
    hashValue(hasher, depthStencilHash);
    hashValue(hasher, static_cast<gp::UInt8>(initializer.shadingRate));
    hashValue(hasher, static_cast<gp::UInt8>(initializer.primitiveType));
    hashValue(hasher, static_cast<gp::UInt8>(initializer.depthBounds ? 1u : 0u));
    hashValue(hasher, static_cast<gp::UInt8>(initializer.allowVariableRateShading ? 1u : 0u));
    return hasher.finalize();
}

gp::UInt64 computeVulkanPrecachePSOHash(
    const pipeline::GraphicsPipelineStateInitializer& initializer, const gp::rhi::GpuVendorId vendorId
) noexcept
{
    // The renderer normally provides the state hash; compute it only when it is missing.
    gp::UInt64 statePrecachePSOHash = initializer.statePrecachePSOHash;
    if (statePrecachePSOHash == 0u)
    {
        statePrecachePSOHash = computeVulkanStatePrecachePSOHash(initializer, vendorId);
    }

    gp::hashing::XxHash64 hasher(0u);
    hashValue(hasher, statePrecachePSOHash);
    hashValue(hasher, initializer.renderTargetsEnabled);
    for (gp::UInt32 index = 0u; index < kMaxSimultaneousRenderTargets; ++index)
    {
        hashValue(hasher, initializer.renderTargetFormats[index]);
    }
    for (gp::UInt32 index = 0u; index < kMaxSimultaneousRenderTargets; ++index)
    {
        const resource::TextureCreateFlags relevantFlags =
            initializer.renderTargetFlags[index] &
            pipeline::GraphicsPipelineStateInitializer::kRelevantRenderTargetFlagMask;
        hashValue(hasher, relevantFlags);
    }
    hashValue(hasher, initializer.numSamples);
    hashValue(hasher, static_cast<gp::UInt8>(initializer.subpassHint));
    hashValue(hasher, initializer.subpassIndex);
    hashValue(hasher, initializer.multiViewCount);
    hashValue(hasher, static_cast<gp::UInt8>(initializer.hasFragmentDensityAttachment ? 1u : 0u));
    hashValue(hasher, static_cast<gp::UInt8>(initializer.conservativeRasterization));
    return hasher.finalize();
}

bool matchVulkanPrecachePSOInitializers(
    const pipeline::GraphicsPipelineStateInitializer& lhs, const pipeline::GraphicsPipelineStateInitializer& rhs
) noexcept
{
    using Initializer = pipeline::GraphicsPipelineStateInitializer;

    // 1. Plain fields first: cheapest, and they reject most collisions.
    if ((lhs.primitiveType != rhs.primitiveType) || (lhs.depthBounds != rhs.depthBounds) ||
        (lhs.multiViewCount != rhs.multiViewCount) || (lhs.shadingRate != rhs.shadingRate) ||
        (lhs.hasFragmentDensityAttachment != rhs.hasFragmentDensityAttachment) ||
        (lhs.allowVariableRateShading != rhs.allowVariableRateShading) ||
        (lhs.renderTargetsEnabled != rhs.renderTargetsEnabled) ||
        (lhs.depthStencilTargetFormat != rhs.depthStencilTargetFormat) ||
        ((lhs.depthStencilTargetFlag & Initializer::kRelevantDepthStencilFlagMask) !=
         (rhs.depthStencilTargetFlag & Initializer::kRelevantDepthStencilFlagMask)) ||
        (lhs.numSamples != rhs.numSamples) || (lhs.subpassHint != rhs.subpassHint) ||
        (lhs.subpassIndex != rhs.subpassIndex) || (lhs.statePrecachePSOHash != rhs.statePrecachePSOHash) ||
        (lhs.conservativeRasterization != rhs.conservativeRasterization))
    {
        return false;
    }
    for (gp::UInt32 index = 0u; index < kMaxSimultaneousRenderTargets; ++index)
    {
        if ((lhs.renderTargetFormats[index] != rhs.renderTargetFormats[index]) ||
            ((lhs.renderTargetFlags[index] & Initializer::kRelevantRenderTargetFlagMask) !=
             (rhs.renderTargetFlags[index] & Initializer::kRelevantRenderTargetFlagMask)))
        {
            return false;
        }
    }

    // 2. Shaders: pointer identity is enough, shaders are deduplicated by the shader factory.
    const pipeline::BoundShaderStateInput& lhsBound = lhs.boundShaderState;
    const pipeline::BoundShaderStateInput& rhsBound = rhs.boundShaderState;
    if ((lhsBound.getVertexShader() != rhsBound.getVertexShader()) ||
        (lhsBound.getPixelShader() != rhsBound.getPixelShader()) ||
        (lhsBound.getMeshShader() != rhsBound.getMeshShader()) ||
        (lhsBound.getAmplificationShader() != rhsBound.getAmplificationShader()) ||
        (lhsBound.getGeometryShader() != rhsBound.getGeometryShader()))
    {
        return false;
    }

    // 3. State content: each initializer may carry its own state object with equal content.
    if (!pipeline::matchPipelineState<pipeline::VertexDeclaration, pipeline::VertexDeclarationElementList>(
            lhsBound.getVertexDeclaration(), rhsBound.getVertexDeclaration()
        ))
    {
        return false;
    }
    return pipeline::matchPipelineState<pipeline::BlendState, pipeline::BlendStateDesc>(
               lhs.blendState, rhs.blendState
           ) &&
           pipeline::matchPipelineState<pipeline::RasterizerState, pipeline::RasterizerStateDesc>(
               lhs.rasterizerState, rhs.rasterizerState
           ) &&
           pipeline::matchPipelineState<pipeline::DepthStencilState, pipeline::DepthStencilStateDesc>(
               lhs.depthStencilState, rhs.depthStencilState
           );
}

}   // namespace gp::rhi::vulkan
