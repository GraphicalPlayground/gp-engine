// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanFramebuffer.hpp"
#include "commands/RenderPassInfo.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "renderpass/VulkanRenderPass.hpp"
#include "renderpass/VulkanRenderTargetViews.hpp"
#include "resources/VulkanTexture.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Returns the image of an optional render target texture.
/// @param[in] texture Render target, or `nullptr`.
/// @return Image, or `VK_NULL_HANDLE`.
[[nodiscard]] VkImage getTextureImage(const resource::Texture* const texture) noexcept
{
    return (texture != nullptr) ? static_cast<const VulkanTexture*>(texture)->getImage() : VK_NULL_HANDLE;
}

}   // namespace

VulkanFramebuffer::VulkanFramebuffer(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const command::SetRenderTargetsInfo& rtInfo,
    const VulkanRenderTargetLayout& layout,
    const VulkanRenderPass& renderPass
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_numColorRenderTargets(
          gp::math::clamp<gp::UInt32>(
              static_cast<gp::UInt32>(gp::math::max(0, rtInfo.numColorRenderTargets)), 0u, kMaxSimultaneousRenderTargets
          )
      )
{
    const VkExtent3D& extent = layout.getExtent3D();
    // Some tilers fault on zero-sized render targets.
    GP_CHECKF((extent.width != 0u) && (extent.height != 0u), "Framebuffers cannot be empty.");

    gp::UInt32 numLayers = extent.depth;
    gp::UInt32 mipIndex = 0u;
    const gp::UInt32 multiViewCount = layout.getMultiViewCount();
    const auto addView = [this](const VkImageView view) -> void
    {
        if (view != VK_NULL_HANDLE)
        {
            m_attachmentViews[m_numAttachmentViews] = view;
            ++m_numAttachmentViews;
        }
    };

    for (gp::UInt32 index = 0u; index < m_numColorRenderTargets; ++index)
    {
        const command::RenderTargetView& colorView = rtInfo.colorRenderTarget[index];
        if (colorView.texture == nullptr)
        {
            continue;
        }

        VulkanTexture& texture = *static_cast<VulkanTexture*>(colorView.texture);
        m_colorImages[index] = texture.getImage();
        mipIndex = colorView.mipIndex;
        const gp::Int32 sliceIndex = toRenderTargetSliceIndex(colorView.arraySliceIndex);
        addView(getVulkanColorRenderTargetView(texture, mipIndex, sliceIndex, multiViewCount, numLayers));
        ++m_numColorAttachments;

        // The layout decides whether the resolve attachment exists (it may drop it for single-sampled targets).
        const command::RenderTargetView& resolveView = rtInfo.colorResolveRenderTarget[index];
        const bool layoutResolves = rtInfo.hasResolveAttachments && layout.getHasResolveAttachments() &&
                                    (layout.getResolveAttachmentReferences()[index].attachment != VK_ATTACHMENT_UNUSED);
        if (layoutResolves && (resolveView.texture != nullptr))
        {
            VulkanTexture& resolveTexture = *static_cast<VulkanTexture*>(resolveView.texture);
            m_colorResolveImages[index] = resolveTexture.getImage();
            addView(getVulkanColorResolveTargetView(resolveTexture, mipIndex, sliceIndex));
        }
    }

    if (layout.getHasDepthStencil() && (rtInfo.depthStencilRenderTarget.texture != nullptr))
    {
        VulkanTexture& texture = *static_cast<VulkanTexture*>(rtInfo.depthStencilRenderTarget.texture);
        m_depthStencilImage = texture.getImage();
        m_partialDepthView = texture.getPartialView();
        addView(getVulkanDepthStencilTargetView(texture, m_numColorRenderTargets, mipIndex, multiViewCount, numLayers));

        const VkAttachmentReference* const resolveReference = layout.getDepthStencilResolveAttachmentReference();
        if (layout.getHasDepthStencilResolve() && (resolveReference != nullptr) &&
            (resolveReference->layout != VK_IMAGE_LAYOUT_UNDEFINED) &&
            (rtInfo.depthStencilResolveRenderTarget.texture != nullptr))
        {
            VulkanTexture& resolveTexture =
                *static_cast<VulkanTexture*>(rtInfo.depthStencilResolveRenderTarget.texture);
            m_depthStencilResolveImage = resolveTexture.getImage();
            addView(getVulkanDepthStencilResolveTargetView(resolveTexture, mipIndex));
        }
    }

    if (layout.getHasFragmentDensityAttachment() && (rtInfo.shadingRateTexture != nullptr))
    {
        VulkanTexture& texture = *static_cast<VulkanTexture*>(rtInfo.shadingRateTexture);
        m_fragmentDensityImage = texture.getImage();
        addView(getVulkanFragmentDensityAttachmentView(texture, mipIndex));
    }

    m_numLayers = (numLayers > 0u) ? numLayers : 1u;
    m_renderArea.offset = layout.getOffset2D();
    m_renderArea.extent = layout.getExtent2D();

    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreateFramebuffer == nullptr) ||
        (renderPass.getHandle() == VK_NULL_HANDLE))
    {
        return;
    }
    if (m_numAttachmentViews != layout.getNumAttachmentDescriptions())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Framebuffer views do not match the render pass attachments.");
        return;
    }

    VkFramebufferCreateInfo createInfo;
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    createInfo.renderPass = renderPass.getHandle();
    createInfo.attachmentCount = m_numAttachmentViews;
    createInfo.pAttachments = (m_numAttachmentViews > 0u) ? m_attachmentViews.data() : nullptr;
    createInfo.width = extent.width;
    createInfo.height = extent.height;
    createInfo.layers = m_numLayers;

    const VkResult result =
        vkCreateFramebuffer(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_framebuffer);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateFramebuffer failed with {}.", vulkanResultToString(result));
        m_framebuffer = VK_NULL_HANDLE;
    }
}

VulkanFramebuffer::~VulkanFramebuffer() noexcept
{
    destroy(false);
}

void VulkanFramebuffer::destroy(const bool immediate) noexcept
{
    if (m_framebuffer == VK_NULL_HANDLE)
    {
        return;
    }

    if (!immediate && (m_deferredDeletionQueue != nullptr))
    {
        m_deferredDeletionQueue->enqueueHandle(VulkanDeferredDeletionQueue::EntryType::Framebuffer, m_framebuffer);
    }
    else if (dynamic_api::vkDestroyFramebuffer != nullptr)
    {
        vkDestroyFramebuffer(m_device.getHandle(), m_framebuffer, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_framebuffer = VK_NULL_HANDLE;
}

bool VulkanFramebuffer::matches(const command::SetRenderTargetsInfo& rtInfo) const noexcept
{
    if (m_numColorRenderTargets != static_cast<gp::UInt32>(rtInfo.numColorRenderTargets))
    {
        return false;
    }

    // Absent targets in the query are wildcards, exactly like the cache key ignores them.
    if ((rtInfo.depthStencilRenderTarget.texture != nullptr) &&
        (getTextureImage(rtInfo.depthStencilRenderTarget.texture) != m_depthStencilImage))
    {
        return false;
    }
    if ((rtInfo.depthStencilResolveRenderTarget.texture != nullptr) &&
        (getTextureImage(rtInfo.depthStencilResolveRenderTarget.texture) != m_depthStencilResolveImage))
    {
        return false;
    }
    if ((rtInfo.shadingRateTexture != nullptr) &&
        (getTextureImage(rtInfo.shadingRateTexture) != m_fragmentDensityImage))
    {
        return false;
    }

    for (gp::UInt32 index = 0u; index < m_numColorRenderTargets; ++index)
    {
        const resource::Texture* const resolve = rtInfo.colorResolveRenderTarget[index].texture;
        if (rtInfo.hasResolveAttachments && (resolve != nullptr) &&
            (getTextureImage(resolve) != m_colorResolveImages[index]))
        {
            return false;
        }
        const resource::Texture* const color = rtInfo.colorRenderTarget[index].texture;
        if ((color != nullptr) && (getTextureImage(color) != m_colorImages[index]))
        {
            return false;
        }
    }
    return true;
}

bool VulkanFramebuffer::containsImage(const VkImage image) const noexcept
{
    if (image == VK_NULL_HANDLE)
    {
        return false;
    }
    if ((image == m_depthStencilImage) || (image == m_depthStencilResolveImage) || (image == m_fragmentDensityImage))
    {
        return true;
    }
    for (gp::UInt32 index = 0u; index < m_numColorRenderTargets; ++index)
    {
        if ((m_colorImages[index] == image) || (m_colorResolveImages[index] == image))
        {
            return true;
        }
    }
    return false;
}

}   // namespace gp::rhi::vulkan
