// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanGraphicsPipelineDescTypes.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "pipeline/VulkanPipelineKey.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Appends an array of descriptions as a count followed by every element.
/// @tparam InElementType Element type providing `writeKey`.
/// @param[in,out] writer Key writer.
/// @param[in] elements Elements to append.
template <typename InElementType>
void writeKeyArray(VulkanPipelineKeyWriter& writer, const gp::Vector<InElementType>& elements) noexcept
{
    writer.writeCount(static_cast<gp::USize>(elements.size()));
    for (const InElementType& element: elements)
    {
        element.writeKey(writer);
    }
}

}   // namespace

void VulkanBlendAttachmentDesc::readFrom(const VkPipelineColorBlendAttachmentState& state) noexcept
{
    blend = state.blendEnable != VK_FALSE;
    colorBlendOp = static_cast<gp::UInt8>(state.colorBlendOp);
    srcColorBlendFactor = static_cast<gp::UInt8>(state.srcColorBlendFactor);
    dstColorBlendFactor = static_cast<gp::UInt8>(state.dstColorBlendFactor);
    alphaBlendOp = static_cast<gp::UInt8>(state.alphaBlendOp);
    srcAlphaBlendFactor = static_cast<gp::UInt8>(state.srcAlphaBlendFactor);
    dstAlphaBlendFactor = static_cast<gp::UInt8>(state.dstAlphaBlendFactor);
    colorWriteMask = static_cast<gp::UInt8>(state.colorWriteMask);
}

void VulkanBlendAttachmentDesc::writeInto(VkPipelineColorBlendAttachmentState& outState) const noexcept
{
    outState.blendEnable = blend ? VK_TRUE : VK_FALSE;
    outState.colorBlendOp = static_cast<VkBlendOp>(colorBlendOp);
    outState.srcColorBlendFactor = static_cast<VkBlendFactor>(srcColorBlendFactor);
    outState.dstColorBlendFactor = static_cast<VkBlendFactor>(dstColorBlendFactor);
    outState.alphaBlendOp = static_cast<VkBlendOp>(alphaBlendOp);
    outState.srcAlphaBlendFactor = static_cast<VkBlendFactor>(srcAlphaBlendFactor);
    outState.dstAlphaBlendFactor = static_cast<VkBlendFactor>(dstAlphaBlendFactor);
    outState.colorWriteMask = static_cast<VkColorComponentFlags>(colorWriteMask);
}

void VulkanBlendAttachmentDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(blend);
    writer.write(colorBlendOp);
    writer.write(srcColorBlendFactor);
    writer.write(dstColorBlendFactor);
    writer.write(alphaBlendOp);
    writer.write(srcAlphaBlendFactor);
    writer.write(dstAlphaBlendFactor);
    writer.write(colorWriteMask);
}

void VulkanDescriptorSetLayoutBindingDesc::readFrom(const VkDescriptorSetLayoutBinding& nativeBinding) noexcept
{
    GP_CHECKF(nativeBinding.descriptorCount == 1u, "Pipeline descriptor bindings must hold exactly one descriptor.");
    binding = nativeBinding.binding;
    descriptorType = static_cast<gp::UInt8>(nativeBinding.descriptorType);
    stageFlags = nativeBinding.stageFlags;
}

void VulkanDescriptorSetLayoutBindingDesc::writeInto(VkDescriptorSetLayoutBinding& outBinding) const noexcept
{
    outBinding.binding = binding;
    outBinding.descriptorType = static_cast<VkDescriptorType>(descriptorType);
    outBinding.descriptorCount = 1u;
    outBinding.stageFlags = stageFlags;
    outBinding.pImmutableSamplers = nullptr;
}

void VulkanDescriptorSetLayoutBindingDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(binding);
    writer.write(descriptorType);
    writer.write(stageFlags);
}

void VulkanVertexBindingDesc::readFrom(const VkVertexInputBindingDescription& nativeBinding) noexcept
{
    binding = static_cast<gp::UInt16>(nativeBinding.binding);
    inputRate = static_cast<gp::UInt16>(nativeBinding.inputRate);
    stride = nativeBinding.stride;
}

void VulkanVertexBindingDesc::writeInto(VkVertexInputBindingDescription& outBinding) const noexcept
{
    outBinding.binding = binding;
    outBinding.inputRate = static_cast<VkVertexInputRate>(inputRate);
    outBinding.stride = stride;
}

void VulkanVertexBindingDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(stride);
    writer.write(binding);
    writer.write(inputRate);
}

void VulkanVertexAttributeDesc::readFrom(const VkVertexInputAttributeDescription& attribute) noexcept
{
    binding = attribute.binding;
    format = static_cast<gp::UInt32>(attribute.format);
    location = attribute.location;
    offset = attribute.offset;
}

void VulkanVertexAttributeDesc::writeInto(VkVertexInputAttributeDescription& outAttribute) const noexcept
{
    outAttribute.binding = binding;
    outAttribute.format = static_cast<VkFormat>(format);
    outAttribute.location = location;
    outAttribute.offset = offset;
}

void VulkanVertexAttributeDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(location);
    writer.write(binding);
    writer.write(format);
    writer.write(offset);
}

void VulkanRasterizerDesc::readFrom(const VkPipelineRasterizationStateCreateInfo& state) noexcept
{
    polygonMode = static_cast<gp::UInt8>(state.polygonMode);
    cullMode = static_cast<gp::UInt8>(state.cullMode);
    depthBiasSlopeScale = state.depthBiasSlopeFactor;
    depthBiasConstantFactor = state.depthBiasConstantFactor;
}

void VulkanRasterizerDesc::writeInto(VkPipelineRasterizationStateCreateInfo& outState) const noexcept
{
    outState.polygonMode = static_cast<VkPolygonMode>(polygonMode);
    outState.cullMode = static_cast<VkCullModeFlags>(cullMode);
    outState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    outState.depthClampEnable = VK_FALSE;
    outState.depthBiasEnable = (depthBiasConstantFactor != 0.0f) ? VK_TRUE : VK_FALSE;
    outState.rasterizerDiscardEnable = VK_FALSE;
    outState.depthBiasSlopeFactor = depthBiasSlopeScale;
    outState.depthBiasConstantFactor = depthBiasConstantFactor;
}

void VulkanRasterizerDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(polygonMode);
    writer.write(cullMode);
    writer.write(depthBiasSlopeScale);
    writer.write(depthBiasConstantFactor);
}

void VulkanDepthStencilDesc::readFrom(const VkPipelineDepthStencilStateCreateInfo& state) noexcept
{
    depthCompareOp = static_cast<gp::UInt8>(state.depthCompareOp);
    depthTestEnable = state.depthTestEnable != VK_FALSE;
    depthWriteEnable = state.depthWriteEnable != VK_FALSE;
    depthBoundsTestEnable = state.depthBoundsTestEnable != VK_FALSE;
    stencilTestEnable = state.stencilTestEnable != VK_FALSE;
    frontFailOp = static_cast<gp::UInt8>(state.front.failOp);
    frontPassOp = static_cast<gp::UInt8>(state.front.passOp);
    frontDepthFailOp = static_cast<gp::UInt8>(state.front.depthFailOp);
    frontCompareOp = static_cast<gp::UInt8>(state.front.compareOp);
    frontCompareMask = state.front.compareMask;
    frontWriteMask = state.front.writeMask;
    frontReference = state.front.reference;
    backFailOp = static_cast<gp::UInt8>(state.back.failOp);
    backPassOp = static_cast<gp::UInt8>(state.back.passOp);
    backDepthFailOp = static_cast<gp::UInt8>(state.back.depthFailOp);
    backCompareOp = static_cast<gp::UInt8>(state.back.compareOp);
    backCompareMask = state.back.compareMask;
    backWriteMask = state.back.writeMask;
    backReference = state.back.reference;
}

void VulkanDepthStencilDesc::writeInto(VkPipelineDepthStencilStateCreateInfo& outState) const noexcept
{
    outState.depthCompareOp = static_cast<VkCompareOp>(depthCompareOp);
    outState.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
    outState.depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE;
    outState.depthBoundsTestEnable = depthBoundsTestEnable ? VK_TRUE : VK_FALSE;
    outState.stencilTestEnable = stencilTestEnable ? VK_TRUE : VK_FALSE;
    outState.front.failOp = static_cast<VkStencilOp>(frontFailOp);
    outState.front.passOp = static_cast<VkStencilOp>(frontPassOp);
    outState.front.depthFailOp = static_cast<VkStencilOp>(frontDepthFailOp);
    outState.front.compareOp = static_cast<VkCompareOp>(frontCompareOp);
    outState.front.compareMask = frontCompareMask;
    outState.front.writeMask = frontWriteMask;
    outState.front.reference = frontReference;
    outState.back.failOp = static_cast<VkStencilOp>(backFailOp);
    outState.back.passOp = static_cast<VkStencilOp>(backPassOp);
    outState.back.depthFailOp = static_cast<VkStencilOp>(backDepthFailOp);
    outState.back.compareOp = static_cast<VkCompareOp>(backCompareOp);
    outState.back.compareMask = backCompareMask;
    outState.back.writeMask = backWriteMask;
    outState.back.reference = backReference;
}

void VulkanDepthStencilDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(depthCompareOp);
    writer.write(depthTestEnable);
    writer.write(depthWriteEnable);
    writer.write(stencilTestEnable);
    writer.write(depthBoundsTestEnable);
    writer.write(frontFailOp);
    writer.write(frontPassOp);
    writer.write(frontDepthFailOp);
    writer.write(frontCompareOp);
    writer.write(frontCompareMask);
    writer.write(frontWriteMask);
    writer.write(frontReference);
    writer.write(backFailOp);
    writer.write(backPassOp);
    writer.write(backDepthFailOp);
    writer.write(backCompareOp);
    writer.write(backCompareMask);
    writer.write(backWriteMask);
    writer.write(backReference);
}

void VulkanAttachmentRefDesc::readFrom(const VkAttachmentReference& reference) noexcept
{
    attachment = reference.attachment;
    layout = static_cast<gp::UInt64>(reference.layout);
}

void VulkanAttachmentRefDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(attachment);
    writer.write(layout);
}

void VulkanStencilAttachmentRefDesc::readFrom(const VkAttachmentReferenceStencilLayout& reference) noexcept
{
    layout = static_cast<gp::UInt64>(reference.stencilLayout);
}

void VulkanStencilAttachmentRefDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(layout);
}

void VulkanAttachmentDescriptionDesc::readFrom(const VkAttachmentDescription& description) noexcept
{
    format = static_cast<gp::UInt32>(description.format);
    flags = static_cast<gp::UInt8>(description.flags);
    samples = static_cast<gp::UInt8>(description.samples);
    loadOp = static_cast<gp::UInt8>(description.loadOp);
    storeOp = static_cast<gp::UInt8>(description.storeOp);
    stencilLoadOp = static_cast<gp::UInt8>(description.stencilLoadOp);
    stencilStoreOp = static_cast<gp::UInt8>(description.stencilStoreOp);
    initialLayout = static_cast<gp::UInt64>(description.initialLayout);
    finalLayout = static_cast<gp::UInt64>(description.finalLayout);
}

void VulkanAttachmentDescriptionDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(format);
    writer.write(flags);
    writer.write(samples);
    writer.write(loadOp);
    writer.write(storeOp);
    writer.write(stencilLoadOp);
    writer.write(stencilStoreOp);
    writer.write(initialLayout);
    writer.write(finalLayout);
}

void VulkanStencilAttachmentDescriptionDesc::readFrom(const VkAttachmentDescriptionStencilLayout& description) noexcept
{
    initialLayout = static_cast<gp::UInt64>(description.stencilInitialLayout);
    finalLayout = static_cast<gp::UInt64>(description.stencilFinalLayout);
}

void VulkanStencilAttachmentDescriptionDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(initialLayout);
    writer.write(finalLayout);
}

void VulkanRenderTargetsDesc::readFrom(const VulkanRenderTargetLayout& layout) noexcept
{
    numAttachments = static_cast<gp::UInt8>(layout.getNumAttachmentDescriptions());
    numColorAttachments = static_cast<gp::UInt8>(layout.getNumColorAttachments());
    hasDepthStencil = layout.getHasDepthStencil();
    hasResolveAttachments = layout.getHasResolveAttachments();
    hasDepthStencilResolve = layout.getHasDepthStencilResolve();
    hasFragmentDensityAttachment = layout.getHasFragmentDensityAttachment();
    numUsedClearValues = static_cast<gp::UInt8>(layout.getNumUsedClearValues());
    renderPassCompatibleHash = layout.getRenderPassCompatibleHash();
    extent[0] = layout.getExtent3D().width;
    extent[1] = layout.getExtent3D().height;
    extent[2] = layout.getExtent3D().depth;

    // Only the used slots are stored: unused slots are always zero, and the counts are part of the key.
    colorAttachments.clear();
    resolveAttachments.clear();
    const VkAttachmentReference* const colorReferences = layout.getColorAttachmentReferences();
    const VkAttachmentReference* const resolveReferences = layout.getResolveAttachmentReferences();
    for (gp::UInt32 index = 0u; index < numColorAttachments; ++index)
    {
        colorAttachments.emplaceBack().readFrom(colorReferences[index]);
        if (resolveReferences != nullptr)
        {
            resolveAttachments.emplaceBack().readFrom(resolveReferences[index]);
        }
    }

    depth = VulkanAttachmentRefDesc{};
    stencil = VulkanStencilAttachmentRefDesc{};
    stencilDescription = VulkanStencilAttachmentDescriptionDesc{};
    fragmentDensity = VulkanAttachmentRefDesc{};
    if (const VkAttachmentReference* const depthReference = layout.getDepthAttachmentReference())
    {
        depth.readFrom(*depthReference);
    }
    if (const VkAttachmentReferenceStencilLayout* const stencilReference = layout.getStencilAttachmentReference())
    {
        stencil.readFrom(*stencilReference);
    }
    if (const VkAttachmentDescriptionStencilLayout* const stencilLayouts = layout.getStencilDescription())
    {
        stencilDescription.readFrom(*stencilLayouts);
    }
    if (const VkAttachmentReference* const densityReference = layout.getFragmentDensityAttachmentReference())
    {
        fragmentDensity.readFrom(*densityReference);
    }

    descriptions.clear();
    const VkAttachmentDescription* const nativeDescriptions = layout.getAttachmentDescriptions();
    for (gp::UInt32 index = 0u; index < numAttachments; ++index)
    {
        descriptions.emplaceBack().readFrom(nativeDescriptions[index]);
    }
}

void VulkanRenderTargetsDesc::writeKey(VulkanPipelineKeyWriter& writer) const noexcept
{
    writer.write(numAttachments);
    writer.write(numColorAttachments);
    writer.write(numUsedClearValues);
    writeKeyArray(writer, colorAttachments);
    writeKeyArray(writer, resolveAttachments);
    depth.writeKey(writer);
    stencil.writeKey(writer);
    fragmentDensity.writeKey(writer);
    writeKeyArray(writer, descriptions);
    stencilDescription.writeKey(writer);
    writer.write(hasDepthStencil);
    writer.write(hasResolveAttachments);
    writer.write(hasDepthStencilResolve);
    writer.write(hasFragmentDensityAttachment);
    writer.write(renderPassCompatibleHash);
    writer.write(extent[0]);
    writer.write(extent[1]);
    writer.write(extent[2]);
}

bool VulkanRenderTargetsDesc::operator==(const VulkanRenderTargetsDesc& rhs) const noexcept
{
    return areVulkanDescArraysEqual(colorAttachments, rhs.colorAttachments) &&
           areVulkanDescArraysEqual(resolveAttachments, rhs.resolveAttachments) && (depth == rhs.depth) &&
           (stencil == rhs.stencil) && (fragmentDensity == rhs.fragmentDensity) &&
           areVulkanDescArraysEqual(descriptions, rhs.descriptions) && (stencilDescription == rhs.stencilDescription) &&
           (numAttachments == rhs.numAttachments) && (numColorAttachments == rhs.numColorAttachments) &&
           (hasDepthStencil == rhs.hasDepthStencil) && (hasResolveAttachments == rhs.hasResolveAttachments) &&
           (hasDepthStencilResolve == rhs.hasDepthStencilResolve) &&
           (hasFragmentDensityAttachment == rhs.hasFragmentDensityAttachment) &&
           (numUsedClearValues == rhs.numUsedClearValues) &&
           (renderPassCompatibleHash == rhs.renderPassCompatibleHash) && (extent[0] == rhs.extent[0]) &&
           (extent[1] == rhs.extent[1]) && (extent[2] == rhs.extent[2]);
}

}   // namespace gp::rhi::vulkan
