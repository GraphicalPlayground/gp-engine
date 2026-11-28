// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanGraphicsPipelineDesc.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "hashing/XxHash.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/VulkanPipelineShaders.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "shaders/VulkanShader.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "states/VulkanBlendState.hpp"
#include "states/VulkanDepthStencilState.hpp"
#include "states/VulkanRasterizerState.hpp"
#include "states/VulkanVertexDeclaration.hpp"
#include "states/VulkanVertexInputStateInfo.hpp"
#include "translation/VulkanPipelineEnums.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Per-stage multipliers of the shared shader key; distinct odd primes so every stage weighs differently.
constexpr gp::Array<gp::UInt64, kNumGraphicsPipelineStages> kShaderKeyPrimes{
    6843488303525203279ull, 3095754086865563867ull, 8242695776924673527ull,
    7556751872809527943ull, 8278265491465149053ull,
};

/// @brief Appends a length-prefixed array of descriptions to a key.
/// @tparam InElementType Description type with `writeKey`.
/// @param[in,out] writer Key writer.
/// @param[in] elements Descriptions.
template <typename InElementType>
void writeDescArray(VulkanPipelineKeyWriter& writer, const gp::Vector<InElementType>& elements) noexcept
{
    writer.writeCount(elements.size());
    for (const InElementType& element: elements)
    {
        element.writeKey(writer);
    }
}

/// @brief Returns how many color attachments the pipeline writes, after the subpass overrides.
/// @details In a deferred shading subpass (index 2 and up) the G-buffer is read as input attachments and only scene
///          color is written; after the first depth-read subpass only scene color is written; a custom resolve
///          pipeline always writes one target.
/// @param[in] initializer RHI initializer.
/// @return Number of color attachments.
[[nodiscard]] gp::UInt32
    computeColorAttachmentCount(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept
{
    gp::UInt32 count = initializer.computeNumValidRenderTargets();
    if (initializer.subpassHint == pipeline::SubpassHint::DeferredShadingSubpass && initializer.subpassIndex >= 2u)
    {
        count = 1u;
    }
    if (initializer.subpassHint == pipeline::SubpassHint::DepthReadSubpass && initializer.subpassIndex >= 1u)
    {
        count = 1u;
    }
    if (initializer.subpassHint == pipeline::SubpassHint::CustomResolveSubpass)
    {
        count = 1u;
    }
    return count;
}

}   // namespace

VulkanPipelineKey VulkanGraphicsPipelineDesc::createKey() const noexcept
{
    VulkanPipelineKey key{};
    {
        VulkanPipelineKeyWriter writer(key);
        writer.write(rasterizationSamples);
        writer.write(topology);
        writeDescArray(writer, colorAttachmentStates);

        writer.writeCount(descriptorSetLayoutBindings.size());
        for (const gp::Vector<VulkanDescriptorSetLayoutBindingDesc>& setBindings: descriptorSetLayoutBindings)
        {
            writeDescArray(writer, setBindings);
        }

        writeDescArray(writer, vertexBindings);
        writeDescArray(writer, vertexAttributes);
        writer.write(vertexStreamMask);
        rasterizer.writeKey(writer);
        depthStencil.writeKey(writer);
        for (const gp::UInt64 shaderKey: shaderKeys)
        {
            writer.write(shaderKey);
        }
        writer.write(specializationKey);
        renderTargets.writeKey(writer);
        writer.write(shadingRate);
        writer.write(combiner);
        writer.write(useAlphaToCoverage);
        writer.write(subpassIndex);
    }
    return key;
}

bool VulkanGraphicsPipelineDesc::operator==(const VulkanGraphicsPipelineDesc& rhs) const noexcept
{
    // Cheap scalar filters first: the shared shader key and the counts reject almost every mismatch.
    if (shaderKeyShared != rhs.shaderKeyShared || rasterizationSamples != rhs.rasterizationSamples ||
        topology != rhs.topology || vertexStreamMask != rhs.vertexStreamMask ||
        specializationKey != rhs.specializationKey || subpassIndex != rhs.subpassIndex ||
        useAlphaToCoverage != rhs.useAlphaToCoverage || shadingRate != rhs.shadingRate || combiner != rhs.combiner)
    {
        return false;
    }

    for (gp::USize index = 0u; index < kNumGraphicsPipelineStages; ++index)
    {
        if (shaderKeys[index] != rhs.shaderKeys[index])
        {
            return false;
        }
    }

    if (!(rasterizer == rhs.rasterizer) || !(depthStencil == rhs.depthStencil))
    {
        return false;
    }

    if (!areVulkanDescArraysEqual(colorAttachmentStates, rhs.colorAttachmentStates) ||
        !areVulkanDescArraysEqual(vertexBindings, rhs.vertexBindings) ||
        !areVulkanDescArraysEqual(vertexAttributes, rhs.vertexAttributes))
    {
        return false;
    }

    if (descriptorSetLayoutBindings.size() != rhs.descriptorSetLayoutBindings.size())
    {
        return false;
    }
    for (gp::USize index = 0u; index < descriptorSetLayoutBindings.size(); ++index)
    {
        if (!areVulkanDescArraysEqual(descriptorSetLayoutBindings[index], rhs.descriptorSetLayoutBindings[index]))
        {
            return false;
        }
    }

    return renderTargets == rhs.renderTargets;
}

gp::UInt64 combineVulkanShaderKeys(const gp::Array<gp::UInt64, kNumGraphicsPipelineStages>& shaderKeys) noexcept
{
    gp::UInt64 sharedKey = 0u;
    for (gp::USize index = 0u; index < kNumGraphicsPipelineStages; ++index)
    {
        // Unsigned arithmetic wraps by definition; the overflow is the mixing.
        sharedKey += shaderKeys[index] * kShaderKeyPrimes[index];
    }
    return sharedKey;
}

bool buildVulkanGraphicsPipelineDesc(
    const VkPhysicalDeviceLimits& limits,
    const VulkanRenderTargetCaps& caps,
    const pipeline::GraphicsPipelineStateInitializer& initializer,
    VulkanDescriptorSetsLayoutInfo& outLayoutInfo,
    VulkanGraphicsPipelineDesc& outDesc
) noexcept
{
    if (initializer.blendState == nullptr || initializer.rasterizerState == nullptr ||
        initializer.depthStencilState == nullptr)
    {
        return false;
    }

    const pipeline::BoundShaderStateInput& boundShaderState = initializer.boundShaderState;
    const VulkanGraphicsShaderArray shaders = getVulkanGraphicsShaders(boundShaderState);

    // 1. Gather the header of every active stage and build the descriptor set layouts from them.
    VulkanUniformBufferGatherInfo gatherInfo{};
    gp::UInt32 numActiveShaders = 0u;
    for (gp::USize index = 0u; index < kNumGraphicsPipelineStages; ++index)
    {
        if (shaders[index] != nullptr)
        {
            gatherInfo.codeHeaders[index] = &shaders[index]->getCodeHeader();
            ++numActiveShaders;
        }
    }
    if (numActiveShaders == 0u)
    {
        return false;
    }
    if (!outLayoutInfo.finalizeBindings(VK_PIPELINE_BIND_POINT_GRAPHICS, limits, gatherInfo, false))
    {
        return false;
    }

    // 2. Vertex input: only the attributes the vertex shader actually reads.
    VulkanVertexInputStateInfo vertexInputState{};
    VulkanShader* vertexShader = shaders[static_cast<gp::USize>(shader_stage::Stage::Vertex)];
    if (boundShaderState.getVertexDeclaration() != nullptr && vertexShader != nullptr)
    {
        vertexInputState.generate(
            *static_cast<const VulkanVertexDeclaration*>(boundShaderState.getVertexDeclaration()),
            vertexShader->getCodeHeader().inOutMask
        );
    }

    // 3. Fixed function state.
    const VulkanBlendState* blendState = static_cast<const VulkanBlendState*>(initializer.blendState);
    outDesc.subpassIndex = initializer.subpassIndex;
    outDesc.useAlphaToCoverage =
        (initializer.numSamples > 1u && blendState->getDesc().useAlphaToCoverage) ? gp::UInt8{ 1u } : gp::UInt8{ 0u };
    outDesc.rasterizationSamples = initializer.numSamples;
    outDesc.topology = static_cast<gp::UInt32>(primitiveTypeToVkTopology(initializer.primitiveType));
    if (initializer.subpassHint == pipeline::SubpassHint::CustomResolveSubpass && initializer.subpassIndex >= 2u)
    {
        // The resolve subpass renders to a single-sampled surface.
        outDesc.rasterizationSamples = 1u;
    }

    const gp::UInt32 numColorAttachments = computeColorAttachmentCount(initializer);
    outDesc.colorAttachmentStates.clear();
    outDesc.colorAttachmentStates.reserve(numColorAttachments);
    for (gp::UInt32 index = 0u; index < numColorAttachments; ++index)
    {
        outDesc.colorAttachmentStates.emplaceBack().readFrom(blendState->getAttachmentState(index));
    }

    const VkPipelineVertexInputStateCreateInfo& vertexInfo = vertexInputState.getInfo();
    outDesc.vertexBindings.clear();
    outDesc.vertexBindings.reserve(vertexInfo.vertexBindingDescriptionCount);
    for (gp::UInt32 index = 0u; index < vertexInfo.vertexBindingDescriptionCount; ++index)
    {
        outDesc.vertexBindings.emplaceBack().readFrom(vertexInfo.pVertexBindingDescriptions[index]);
    }
    outDesc.vertexAttributes.clear();
    outDesc.vertexAttributes.reserve(vertexInfo.vertexAttributeDescriptionCount);
    for (gp::UInt32 index = 0u; index < vertexInfo.vertexAttributeDescriptionCount; ++index)
    {
        outDesc.vertexAttributes.emplaceBack().readFrom(vertexInfo.pVertexAttributeDescriptions[index]);
    }
    outDesc.vertexStreamMask = vertexInputState.getBindingsMask();

    const gp::Vector<VulkanDescriptorSetsLayoutInfo::SetLayout>& layouts = outLayoutInfo.getLayouts();
    outDesc.descriptorSetLayoutBindings.clear();
    outDesc.descriptorSetLayoutBindings.reserve(layouts.size());
    for (const VulkanDescriptorSetsLayoutInfo::SetLayout& layout: layouts)
    {
        gp::Vector<VulkanDescriptorSetLayoutBindingDesc>& setBindings =
            outDesc.descriptorSetLayoutBindings.emplaceBack();
        setBindings.reserve(layout.layoutBindings.size());
        for (const VkDescriptorSetLayoutBinding& binding: layout.layoutBindings)
        {
            setBindings.emplaceBack().readFrom(binding);
        }
    }

    outDesc.rasterizer.readFrom(
        static_cast<const VulkanRasterizerState*>(initializer.rasterizerState)->getCreateInfo()
    );
    {
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
        static_cast<const VulkanDepthStencilState*>(initializer.depthStencilState)
            ->setupCreateInfo(initializer.depthBounds, depthStencilInfo);
        outDesc.depthStencil.readFrom(depthStencilInfo);
    }

    // 4. Shader identity.
    for (gp::USize index = 0u; index < kNumGraphicsPipelineStages; ++index)
    {
        outDesc.shaderKeys[index] = (shaders[index] != nullptr) ? shaders[index]->getShaderKey() : 0u;
    }
    outDesc.shaderKeyShared = combineVulkanShaderKeys(outDesc.shaderKeys);

    {
        constexpr gp::Array<shader_stage::Stage, kNumGraphicsPipelineStages> kSpecializationOrder{
            shader_stage::Stage::Vertex, shader_stage::Stage::Pixel, shader_stage::Stage::Geometry,
            shader_stage::Stage::Mesh,   shader_stage::Stage::Task,
        };
        gp::hashing::XxHash64 hasher(0u);
        bool hasConstants = false;
        for (const shader_stage::Stage stage: kSpecializationOrder)
        {
            const shader::SpecializationContainer& constants =
                getVulkanStageSpecializationConstants(boundShaderState, stage);
            if (!constants.isEmpty())
            {
                hasConstants = true;
                hasher.update(constants.data(), constants.size() * sizeof(gp::UInt32));
            }
        }
        outDesc.specializationKey = hasConstants ? hasher.finalize() : 0u;
    }

    // 5. Render targets.
    const VulkanRenderTargetLayout renderTargetLayout(caps, initializer);
    outDesc.renderTargets.readFrom(renderTargetLayout);

    const bool allowVariableRateShading = initializer.allowVariableRateShading;
    outDesc.shadingRate = allowVariableRateShading ? initializer.shadingRate : pipeline::VRSShadingRate::Rate1x1;
    outDesc.combiner =
        allowVariableRateShading ? pipeline::VRSRateCombiner::Max : pipeline::VRSRateCombiner::Passthrough;
    return true;
}

}   // namespace gp::rhi::vulkan
