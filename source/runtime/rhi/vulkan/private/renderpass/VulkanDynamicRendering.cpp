// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanDynamicRendering.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/CommandTypes.hpp"
#include "commands/RenderPassInfo.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/color/LinearColor.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "renderpass/VulkanRenderTargetViews.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "RHIPixelFormat.hpp"
#include "translation/VulkanPipelineEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reports whether a texture lives only in tile memory.
/// @param[in] texture Render target.
/// @return True for memoryless textures.
[[nodiscard]] bool isMemoryless(const resource::Texture& texture) noexcept
{
    return hasAnyTextureFlags(texture.getFlags(), resource::TextureCreateFlags::Memoryless);
}

/// @brief Returns the size of a mip, never below one texel.
/// @param[in] size Base size.
/// @param[in] mipIndex Mip level.
/// @return Mip size.
[[nodiscard]] gp::UInt32 getMipSize(const gp::Int32 size, const gp::UInt32 mipIndex) noexcept
{
    return static_cast<gp::UInt32>(gp::math::max(1, size >> mipIndex));
}

/// @brief Fills the layout and ops of a depth or stencil aspect.
/// @param[in] currentLayout Layout of the aspect, `UNDEFINED` when unused.
/// @param[in] actions Load and store actions of the aspect.
/// @param[in] memoryless Texture lives in tile memory only.
/// @param[out] outAttachment Attachment.
/// @return True when the aspect is stored.
bool fillDepthStencilAspect(
    const VkImageLayout currentLayout,
    const command::RenderTargetActions actions,
    const bool memoryless,
    VkRenderingAttachmentInfo& outAttachment
) noexcept
{
    if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        // An aspect that was never initialized is left untouched; the final layout still cannot be UNDEFINED.
        outAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        outAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_NONE_EXT;
        outAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
        return false;
    }

    outAttachment.imageLayout = currentLayout;
    outAttachment.loadOp = renderTargetLoadActionToVkLoadOp(command::getLoadAction(actions));
    outAttachment.storeOp = renderTargetStoreActionToVkStoreOp(command::getStoreAction(actions), memoryless);
    return outAttachment.storeOp == VK_ATTACHMENT_STORE_OP_STORE;
}

}   // namespace

void extractVulkanDepthStencilLayouts(
    const VulkanRenderTargetCaps& caps,
    const command::RenderPassInfo& rpInfo,
    VkImageLayout& outDepthLayout,
    VkImageLayout& outStencilLayout
) noexcept
{
    outDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    outStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    const resource::Texture* const target = rpInfo.depthStencilRenderTarget.depthStencilTarget;
    if (target == nullptr)
    {
        return;
    }

    const VulkanTexture& texture = *static_cast<const VulkanTexture*>(target);
    const VkImageAspectFlags aspects = texture.getFullAspectMask();
    const resource::ExclusiveDepthStencil access = rpInfo.depthStencilRenderTarget.exclusiveDepthStencil;

    // Without separate depth and stencil layouts, both aspects transition together: the unused aspect keeps the
    // layout the texture is tracked in.
    const bool needsAllPlanes = !caps.usesSeparateDepthStencilLayouts();
    if ((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0u)
    {
        if (access.isDepthWrite())
        {
            outDepthLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }
        else if (access.isDepthRead())
        {
            outDepthLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        }
        else if (needsAllPlanes)
        {
            outDepthLayout = texture.getTrackedLayout();
        }
    }
    if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0u)
    {
        if (access.isStencilWrite())
        {
            outStencilLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }
        else if (access.isStencilRead())
        {
            outStencilLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        }
        else if (needsAllPlanes)
        {
            outStencilLayout = texture.getTrackedLayout();
        }
    }
}

void fillVulkanDynamicRenderingInfo(
    const VulkanRenderTargetCaps& caps, const command::RenderPassInfo& rpInfo, VulkanDynamicRenderingInfo& outInfo
) noexcept
{
    VkRenderingInfo& renderingInfo = outInfo.renderingInfo;
    zeroVulkanStruct(renderingInfo, VK_STRUCTURE_TYPE_RENDERING_INFO);
    for (VkImageView& view: outInfo.colorViews)
    {
        view = VK_NULL_HANDLE;
    }
    outInfo.partialDepthView = VK_NULL_HANDLE;

    gp::UInt32 numSamples = 0u;
    gp::UInt32 mipIndex = 0u;
    gp::UInt32 numLayers = 0u;
    const bool hasCustomResolve = rpInfo.subpassHint == pipeline::SubpassHint::CustomResolveSubpass;

    // 1. Colors.
    const gp::UInt32 numColors = rpInfo.getNumColorRenderTargets();
    renderingInfo.colorAttachmentCount = numColors;
    renderingInfo.pColorAttachments = (numColors > 0u) ? outInfo.colorAttachments.data() : nullptr;
    for (gp::UInt32 index = 0u; index < numColors; ++index)
    {
        const command::RenderPassInfo::ColorEntry& entry = rpInfo.colorRenderTargets[index];
        VkRenderingAttachmentInfo& attachment = outInfo.colorAttachments[index];
        zeroVulkanStruct(attachment, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);

        VulkanTexture& texture = *static_cast<VulkanTexture*>(entry.renderTarget);
        const resource::TextureDesc& desc = texture.getDesc();
        const bool memoryless = isMemoryless(texture);

        VkExtent2D& extent = renderingInfo.renderArea.extent;
        const gp::UInt32 width = getMipSize(desc.extent.x, entry.mipIndex);
        const gp::UInt32 height = getMipSize(desc.extent.y, entry.mipIndex);
        GP_CHECKF((extent.width == 0u) || (extent.width == width), "Color targets must share one extent.");
        GP_CHECKF((extent.height == 0u) || (extent.height == height), "Color targets must share one extent.");
        extent.width = width;
        extent.height = height;
        numLayers = desc.depth;

        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = renderTargetLoadActionToVkLoadOp(command::getLoadAction(entry.action));
        attachment.storeOp = renderTargetStoreActionToVkStoreOp(command::getStoreAction(entry.action), memoryless);
        if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        {
            const gp::math::LinearColor color =
                texture.hasClearValue() ? texture.getClearColor() : gp::math::LinearColor(0.0f, 0.0f, 0.0f, 1.0f);
            attachment.clearValue.color.float32[0] = color.r;
            attachment.clearValue.color.float32[1] = color.g;
            attachment.clearValue.color.float32[2] = color.b;
            attachment.clearValue.color.float32[3] = color.a;
        }

        mipIndex = entry.mipIndex;
        attachment.imageView =
            getVulkanColorRenderTargetView(texture, mipIndex, entry.arraySlice, rpInfo.multiViewCount, numLayers);
        outInfo.colorViews[index] = attachment.imageView;

        GP_CHECKF(
            (numSamples == 0u) || (numSamples == texture.getNumSamples()) || hasCustomResolve,
            "Every color target of a pass must have the same sample count."
        );
        numSamples = texture.getNumSamples();

        // The custom resolve target is single-sampled and resolved by a shader, never by the hardware.
        const bool isCustomResolveTarget = hasCustomResolve && (index == (numColors - 1u));
        if (!isCustomResolveTarget && (numSamples > 1u) && (entry.resolveTarget != nullptr))
        {
            VulkanTexture& resolveTexture = *static_cast<VulkanTexture*>(entry.resolveTarget);
            const VkImageView resolveView = getVulkanColorResolveTargetView(resolveTexture, mipIndex, entry.arraySlice);
            if (resolveView != VK_NULL_HANDLE)
            {
                // Averaging integer samples is undefined: integer formats keep sample 0.
                attachment.resolveMode =
                    isIntegerFormat(desc.format) ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT : VK_RESOLVE_MODE_AVERAGE_BIT;
                attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachment.resolveImageView = resolveView;
            }
        }
    }

    // 2. Depth and stencil.
    const command::RenderPassInfo::DepthStencilEntry& depthEntry = rpInfo.depthStencilRenderTarget;
    if (depthEntry.depthStencilTarget != nullptr)
    {
        VkImageLayout depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout stencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        extractVulkanDepthStencilLayouts(caps, rpInfo, depthLayout, stencilLayout);

        VulkanTexture& texture = *static_cast<VulkanTexture*>(depthEntry.depthStencilTarget);
        const resource::TextureDesc& desc = texture.getDesc();
        const bool memoryless = isMemoryless(texture);
        outInfo.partialDepthView = texture.getPartialView();

        VkExtent2D& extent = renderingInfo.renderArea.extent;
        const gp::UInt32 width = static_cast<gp::UInt32>(desc.extent.x);
        const gp::UInt32 height = static_cast<gp::UInt32>(desc.extent.y);
        extent.width = (extent.width == 0u) ? width : gp::math::min(extent.width, width);
        extent.height = (extent.height == 0u) ? height : gp::math::min(extent.height, height);
        numLayers = (numLayers == 0u) ? texture.getNumberOfArrayLevels() : numLayers;

        gp::Float32 clearDepth = 0.0f;
        gp::UInt32 clearStencil = 0u;
        if (texture.hasClearValue())
        {
            texture.getDepthStencilClearValue(clearDepth, clearStencil);
        }

        VkImageView depthView = VK_NULL_HANDLE;
        bool isDepthStore = false;
        bool isStencilStore = false;
        const resource::ExclusiveDepthStencil access = depthEntry.exclusiveDepthStencil;
        if (access.isUsingDepth())
        {
            zeroVulkanStruct(outInfo.depthAttachment, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
            renderingInfo.pDepthAttachment = &outInfo.depthAttachment;
            outInfo.depthAttachment.clearValue.depthStencil.depth = clearDepth;
            outInfo.depthAttachment.clearValue.depthStencil.stencil = clearStencil;
            isDepthStore = fillDepthStencilAspect(
                depthLayout, command::getDepthActions(depthEntry.action), memoryless, outInfo.depthAttachment
            );
            depthView = getVulkanDepthStencilTargetView(texture, numColors, mipIndex, rpInfo.multiViewCount, numLayers);
            outInfo.depthAttachment.imageView = depthView;
        }
        if (access.isUsingStencil() && isStencilFormat(desc.format))
        {
            zeroVulkanStruct(outInfo.stencilAttachment, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
            renderingInfo.pStencilAttachment = &outInfo.stencilAttachment;
            outInfo.stencilAttachment.clearValue.depthStencil.depth = clearDepth;
            outInfo.stencilAttachment.clearValue.depthStencil.stencil = clearStencil;
            isStencilStore = fillDepthStencilAspect(
                stencilLayout, command::getStencilActions(depthEntry.action), memoryless, outInfo.stencilAttachment
            );
            if (depthView == VK_NULL_HANDLE)
            {
                depthView =
                    getVulkanDepthStencilTargetView(texture, numColors, mipIndex, rpInfo.multiViewCount, numLayers);
            }
            outInfo.stencilAttachment.imageView = depthView;
        }

        // Some drivers corrupt the aspect that is not stored: store both as soon as one is.
        if (caps.requiresDepthStencilFullWrite &&
            (texture.getFullAspectMask() == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) &&
            (isDepthStore || isStencilStore))
        {
            outInfo.depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            outInfo.stencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        }

        GP_CHECKF(
            (numSamples == 0u) || (numSamples == texture.getNumSamples()) || hasCustomResolve,
            "The depth target must have the sample count of the color targets."
        );
        numSamples = texture.getNumSamples();

        if (caps.supportsDepthStencilResolve && (numSamples > 1u) && (depthEntry.resolveTarget != nullptr))
        {
            VulkanTexture& resolveTexture = *static_cast<VulkanTexture*>(depthEntry.resolveTarget);
            const VkImageView resolveView = getVulkanDepthStencilResolveTargetView(resolveTexture, mipIndex);
            if (resolveView != VK_NULL_HANDLE)
            {
                // The resolve view goes into `resolveImageView`; `imageView` stays the multisampled source.
                outInfo.depthAttachment.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
                outInfo.depthAttachment.resolveImageLayout = depthLayout;
                outInfo.depthAttachment.resolveImageView = resolveView;
                outInfo.stencilAttachment.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
                outInfo.stencilAttachment.resolveImageLayout = stencilLayout;
                outInfo.stencilAttachment.resolveImageView = resolveView;
            }
        }
    }

    // 3. Raster-only pass: the render area comes from the resolve rectangle.
    if ((numColors == 0u) && (renderingInfo.pDepthAttachment == nullptr) &&
        (renderingInfo.pStencilAttachment == nullptr))
    {
        const command::ResolveRect& rect = rpInfo.resolveRect;
        renderingInfo.renderArea.offset.x = rect.x1;
        renderingInfo.renderArea.offset.y = rect.y1;
        renderingInfo.renderArea.extent.width = static_cast<gp::UInt32>(gp::math::max(0, rect.x2 - rect.x1));
        renderingInfo.renderArea.extent.height = static_cast<gp::UInt32>(gp::math::max(0, rect.y2 - rect.y1));
        numLayers = 1u;
    }

    // 4. Shading rate image, with the struct matching its encoding.
    if (caps.supportsAttachmentVariableRateShading && rpInfo.shadingRateTexture.isValid())
    {
        VulkanTexture& texture = *static_cast<VulkanTexture*>(rpInfo.shadingRateTexture.get());
        GP_CHECKF(texture.getFormat() == caps.vrsImageFormat, "Shading rate image has an unexpected format.");
        const VkImageView view = getVulkanFragmentDensityAttachmentView(texture, mipIndex);
        if (caps.vrsImageDataType == pipeline::VRSImageDataType::Palette)
        {
            VkRenderingFragmentShadingRateAttachmentInfoKHR& info = outInfo.shadingRateAttachment;
            zeroVulkanStruct(info, VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);
            info.imageView = view;
            info.imageLayout = caps.getShadingRateImageLayout();
            info.shadingRateAttachmentTexelSize = VkExtent2D{ caps.vrsTileWidth, caps.vrsTileHeight };
            info.pNext = renderingInfo.pNext;
            renderingInfo.pNext = &info;
        }
        else if (caps.vrsImageDataType == pipeline::VRSImageDataType::Fractional)
        {
            VkRenderingFragmentDensityMapAttachmentInfoEXT& info = outInfo.fragmentDensityAttachment;
            zeroVulkanStruct(info, VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT);
            info.imageView = view;
            info.imageLayout = caps.getShadingRateImageLayout();
            info.pNext = renderingInfo.pNext;
            renderingInfo.pNext = &info;
        }
    }

    renderingInfo.viewMask = (1u << rpInfo.multiViewCount) - 1u;
    // Multiview passes take their views from the mask; the layer count must then be ignored (and kept at least 1).
    renderingInfo.layerCount = (numLayers > 0u) ? numLayers : 1u;
}

}   // namespace gp::rhi::vulkan
