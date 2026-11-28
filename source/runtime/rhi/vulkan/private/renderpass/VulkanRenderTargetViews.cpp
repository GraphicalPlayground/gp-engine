// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderTargetViews.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/VulkanTexture.hpp"
#include "views/VulkanView.hpp"
#include "views/VulkanViewEnums.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reports whether a view type is 2D or 2D array, the only types resolve and shading rate attachments support.
/// @param[in] viewType Native view type.
/// @return True for 2D and 2D array.
[[nodiscard]] constexpr bool is2DViewType(const VkImageViewType viewType) noexcept
{
    return (viewType == VK_IMAGE_VIEW_TYPE_2D) || (viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY);
}

/// @brief Finds or creates an internal view and returns its native handle.
/// @param[in,out] texture Owner of the view.
/// @param[in] viewDesc View description.
/// @return Image view, or `VK_NULL_HANDLE` when the view could not be created.
[[nodiscard]] VkImageView findOrAddView(VulkanTexture& texture, const VulkanTextureViewDesc& viewDesc) noexcept
{
    const VulkanView* const view = texture.findOrAddInternalView(viewDesc);
    if ((view == nullptr) || (view->getViewKind() != VulkanViewKind::Texture))
    {
        return VK_NULL_HANDLE;
    }
    return view->getTextureView().view;
}

}   // namespace

VulkanTextureViewDesc makeVulkanRenderTargetViewDesc(
    const VulkanTexture& texture, const gp::UInt32 mipIndex, const gp::Int32 sliceIndex
) noexcept
{
    VulkanTextureViewDesc viewDesc{};
    viewDesc.viewType = texture.getViewType();
    viewDesc.aspectFlags = texture.getFullAspectMask();
    viewDesc.pixelFormat = texture.getDesc().format;
    viewDesc.format = texture.getViewFormat();
    viewDesc.firstMip = mipIndex;
    viewDesc.numMips = 1u;
    viewDesc.arraySliceIndex = (sliceIndex > 0) ? static_cast<gp::UInt32>(sliceIndex) : 0u;
    viewDesc.numArraySlices = texture.getNumberOfArrayLevels();
    viewDesc.useIdentitySwizzle = true;
    return viewDesc;
}

VkImageView getVulkanColorRenderTargetView(
    VulkanTexture& texture,
    const gp::UInt32 mipIndex,
    const gp::Int32 sliceIndex,
    const gp::UInt32 multiViewCount,
    gp::UInt32& inOutNumLayers
) noexcept
{
    if (texture.getImage() == VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Color render target has no image.");
        return VK_NULL_HANDLE;
    }

    VulkanTextureViewDesc viewDesc = makeVulkanRenderTargetViewDesc(texture, mipIndex, sliceIndex);
    const VkImageViewType viewType = texture.getViewType();
    const bool isAllSlices = sliceIndex < 0;
    if (is2DViewType(viewType))
    {
        if (!isAllSlices)
        {
            GP_CHECK(static_cast<gp::UInt32>(sliceIndex) < texture.getNumberOfArrayLevels());
            viewDesc.numArraySlices = 1u;
        }

        // A multiview framebuffer must have exactly one layer: views are selected by the view mask instead.
        inOutNumLayers = ((multiViewCount == 0u) && isAllSlices) ? texture.getNumberOfArrayLevels() : 1u;
    }
    else if ((viewType == VK_IMAGE_VIEW_TYPE_CUBE) || (viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY))
    {
        // One face at a time, through a plain 2D view.
        viewDesc.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewDesc.numArraySlices = 1u;
    }
    else if (viewType == VK_IMAGE_VIEW_TYPE_3D)
    {
        // Every depth slice of a volume is a layer of a 2D array view.
        viewDesc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewDesc.arraySliceIndex = 0u;
        viewDesc.numArraySlices = texture.getDesc().depth;
    }
    else
    {
        GP_LOG_ERROR(LogVulkanRHI, "Unsupported color render target view type.");
        return VK_NULL_HANDLE;
    }

    viewDesc.imageUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (texture.getImageUsageFlags() & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    return findOrAddView(texture, viewDesc);
}

VkImageView getVulkanColorResolveTargetView(
    VulkanTexture& texture, const gp::UInt32 mipIndex, const gp::Int32 sliceIndex
) noexcept
{
    if (!is2DViewType(texture.getViewType()))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Color resolve targets must be 2D or 2D array textures.");
        return VK_NULL_HANDLE;
    }
    return findOrAddView(texture, makeVulkanRenderTargetViewDesc(texture, mipIndex, sliceIndex));
}

VkImageView getVulkanDepthStencilTargetView(
    VulkanTexture& texture,
    const gp::UInt32 numColorAttachments,
    const gp::UInt32 mipIndex,
    const gp::UInt32 multiViewCount,
    gp::UInt32& inOutNumLayers
) noexcept
{
    const VkImageViewType viewType = texture.getViewType();
    VulkanTextureViewDesc viewDesc = makeVulkanRenderTargetViewDesc(texture, mipIndex, 0);
    if ((numColorAttachments == 0u) && (viewType == VK_IMAGE_VIEW_TYPE_CUBE))
    {
        // Depth-only cube passes (shadow maps) render all six faces as layers.
        inOutNumLayers = 6u;
        viewDesc.numArraySlices = 6u;
        return findOrAddView(texture, viewDesc);
    }
    if (is2DViewType(viewType))
    {
        inOutNumLayers = (multiViewCount == 0u) ? texture.getNumberOfArrayLevels() : 1u;
        return findOrAddView(texture, viewDesc);
    }
    if (viewType == VK_IMAGE_VIEW_TYPE_CUBE)
    {
        return texture.getDefaultView();
    }

    GP_LOG_ERROR(LogVulkanRHI, "Depth-stencil targets must be 2D, 2D array or cube textures.");
    return VK_NULL_HANDLE;
}

VkImageView getVulkanDepthStencilResolveTargetView(VulkanTexture& texture, const gp::UInt32 mipIndex) noexcept
{
    if (!is2DViewType(texture.getViewType()))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Depth-stencil resolve targets must be 2D or 2D array textures.");
        return VK_NULL_HANDLE;
    }
    return findOrAddView(texture, makeVulkanRenderTargetViewDesc(texture, mipIndex, 0));
}

VkImageView getVulkanFragmentDensityAttachmentView(VulkanTexture& texture, const gp::UInt32 mipIndex) noexcept
{
    if (!is2DViewType(texture.getViewType()))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Shading rate images must be 2D or 2D array textures.");
        return VK_NULL_HANDLE;
    }
    return findOrAddView(texture, makeVulkanRenderTargetViewDesc(texture, mipIndex, 0));
}

}   // namespace gp::rhi::vulkan
