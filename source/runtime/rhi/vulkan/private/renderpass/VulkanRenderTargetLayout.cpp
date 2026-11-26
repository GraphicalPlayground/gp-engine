// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/CommandTypes.hpp"
#include "commands/RenderPassInfo.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "hashing/Crc32.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "translation/VulkanPipelineEnums.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "translation/VulkanResourceEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Slot of the depth aspect in the hash keys.
constexpr gp::UInt32 kDepthSlot = kMaxSimultaneousRenderTargets;

/// @brief Slot of the stencil aspect in the full hash key.
constexpr gp::UInt32 kStencilSlot = kMaxSimultaneousRenderTargets + 1u;

/// @brief Slot of the shading rate image in the full hash key.
constexpr gp::UInt32 kShadingRateSlot = kMaxSimultaneousRenderTargets + 2u;

/// @brief Slot of the shading rate format in the compatible hash key.
constexpr gp::UInt32 kShadingRateFormatSlot = kMaxSimultaneousRenderTargets + 1u;

/// @brief Number of slots of the hash keys.
constexpr gp::UInt32 kHashSlotCount = kMaxSimultaneousRenderTargets + 3u;

/// @brief Returns the mip-level extent of a texture dimension.
/// @param[in] size Base size.
/// @param[in] mipIndex Mip level.
/// @return `max(1, size >> mipIndex)`.
[[nodiscard]] gp::UInt32 getMipSize(const gp::Int32 size, const gp::UInt32 mipIndex) noexcept
{
    return static_cast<gp::UInt32>(gp::math::max<gp::Int32>(1, size >> mipIndex));
}

/// @brief Reports whether a texture lives only in tile memory.
/// @param[in] texture RHI texture.
/// @return True for memoryless textures.
[[nodiscard]] bool isMemorylessTexture(const resource::Texture& texture) noexcept
{
    return hasAnyTextureFlags(texture.getFlags(), resource::TextureCreateFlags::Memoryless);
}

/// @brief Translates the format of a color attachment.
/// @param[in] format Engine format.
/// @param[in] flags Texture flags; `SRGB` selects the sRGB variant.
/// @return Native format.
[[nodiscard]] VkFormat getAttachmentFormat(const PixelFormat format, const resource::TextureCreateFlags flags) noexcept
{
    return pixelFormatToVkFormat(format, hasAnyTextureFlags(flags, resource::TextureCreateFlags::SRGB));
}

/// @brief Layout of a depth or stencil aspect, from the requested access.
/// @param[in] isWrite True when the aspect is written.
/// @return `ATTACHMENT_OPTIMAL` when written, `READ_ONLY_OPTIMAL` otherwise.
[[nodiscard]] constexpr VkImageLayout getAspectLayout(const bool isWrite) noexcept
{
    return isWrite ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
}

}   // namespace

struct VulkanRenderTargetLayout::CompatibleHashKey final
{
    /// @brief Zeroes every byte, padding included, so the hash is deterministic.
    CompatibleHashKey() noexcept
    {
        gp::memory::zeroMemory(this, sizeof(CompatibleHashKey));
    }

    /// @brief Number of color attachments.
    gp::UInt8 numAttachments;

    /// @brief Multiview count.
    gp::UInt8 multiViewCount;

    /// @brief Sample count.
    gp::UInt8 numSamples;

    /// @brief Subpass hint.
    gp::UInt8 subpassHint;

    /// @brief Formats: colors, then depth, then shading rate.
    VkFormat formats[kHashSlotCount];

    /// @brief One bit per resolving color; the bit after the last color is the depth resolve.
    gp::UInt16 attachmentsToResolve;
};

struct VulkanRenderTargetLayout::FullHashKey final
{
    /// @brief Zeroes every byte, padding included, so the hash is deterministic.
    FullHashKey() noexcept
    {
        gp::memory::zeroMemory(this, sizeof(FullHashKey));
    }

    /// @brief Load ops: colors, depth, stencil, shading rate. Kept at full width, `LOAD_OP_NONE` does not fit a byte.
    VkAttachmentLoadOp loadOps[kHashSlotCount];

    /// @brief Store ops, same slots as `loadOps`.
    VkAttachmentStoreOp storeOps[kHashSlotCount];

    /// @brief Initial layouts, same slots as `loadOps`.
    VkImageLayout initialLayouts[kHashSlotCount];
};

VulkanRenderTargetLayout::VulkanRenderTargetLayout() noexcept
{
    resetAttachments();
}

VulkanRenderTargetLayout::VulkanRenderTargetLayout(
    const VulkanRenderTargetCaps& caps, const pipeline::GraphicsPipelineStateInitializer& initializer
) noexcept
{
    resetAttachments();

    CompatibleHashKey compatibleKey;
    FullHashKey fullKey;
    bool foundClearOp = false;

    m_multiViewCount = initializer.multiViewCount;
    m_numSamples = static_cast<gp::UInt8>(initializer.numSamples);
    const gp::UInt32 numRenderTargets = gp::math::min(initializer.renderTargetsEnabled, kMaxSimultaneousRenderTargets);

    for (gp::UInt32 index = 0u; index < numRenderTargets; ++index)
    {
        const PixelFormat format = initializer.renderTargetFormats[index];
        if (format == PixelFormat::Unknown)
        {
            continue;
        }

        // The custom resolve subpass writes its last color at one sample per pixel.
        const bool isCustomResolveAttachment = (index == (numRenderTargets - 1u)) &&
                                               (initializer.subpassHint == pipeline::SubpassHint::CustomResolveSubpass);

        VkAttachmentDescription& description = m_descriptions[m_numAttachmentDescriptions];
        description.samples =
            isCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : sampleCountToVkSampleCountFlagBits(m_numSamples);
        description.format = getAttachmentFormat(format, initializer.renderTargetFlags[index]);
        description.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        description.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        m_colorReferences[m_numColorAttachments].attachment = m_numAttachmentDescriptions;
        m_colorReferences[m_numColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        compatibleKey.formats[m_numColorAttachments] = description.format;
        fullKey.loadOps[m_numColorAttachments] = description.loadOp;
        fullKey.storeOps[m_numColorAttachments] = description.storeOp;
        fullKey.initialLayouts[m_numColorAttachments] = description.initialLayout;

        // A pipeline does not know whether the pass resolves: assume it does, so it stays compatible with both.
        if (description.samples > VK_SAMPLE_COUNT_1_BIT)
        {
            addColorResolve(compatibleKey);
        }

        ++compatibleKey.numAttachments;
        ++m_numAttachmentDescriptions;
        ++m_numColorAttachments;
    }

    if (initializer.depthStencilTargetFormat != PixelFormat::Unknown)
    {
        const bool isMemoryless =
            hasAnyTextureFlags(initializer.depthStencilTargetFlag, resource::TextureCreateFlags::Memoryless);

        VkAttachmentDescription& description = m_descriptions[m_numAttachmentDescriptions];
        description.samples = sampleCountToVkSampleCountFlagBits(m_numSamples);
        description.format = pixelFormatToVkFormat(initializer.depthStencilTargetFormat);
        description.loadOp = renderTargetLoadActionToVkLoadOp(initializer.depthTargetLoadAction);
        description.stencilLoadOp = renderTargetLoadActionToVkLoadOp(initializer.stencilTargetLoadAction);
        description.storeOp = renderTargetStoreActionToVkStoreOp(initializer.depthTargetStoreAction, isMemoryless);
        description.stencilStoreOp =
            renderTargetStoreActionToVkStoreOp(initializer.stencilTargetStoreAction, isMemoryless);
        foundClearOp = (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) ||
                       (description.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);

        const VkImageLayout depthLayout = getAspectLayout(initializer.depthStencilAccess.isDepthWrite());
        const VkImageLayout stencilLayout = getAspectLayout(initializer.depthStencilAccess.isStencilWrite());
        description.initialLayout = depthLayout;
        description.finalLayout = depthLayout;
        m_stencilDescription.stencilInitialLayout = stencilLayout;
        m_stencilDescription.stencilFinalLayout = stencilLayout;

        m_depthReference.attachment = m_numAttachmentDescriptions;
        m_depthReference.layout = depthLayout;
        m_stencilReference.stencilLayout = stencilLayout;

        const bool wantsResolve =
            (initializer.depthTargetStoreAction == command::RenderTargetStoreAction::MultisampleResolve) ||
            (initializer.stencilTargetStoreAction == command::RenderTargetStoreAction::MultisampleResolve);
        if (wantsResolve && caps.supportsDepthStencilResolve && (description.samples > VK_SAMPLE_COUNT_1_BIT))
        {
            addDepthStencilResolve(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, compatibleKey);
        }

        fullKey.loadOps[kDepthSlot] = description.loadOp;
        fullKey.loadOps[kStencilSlot] = description.stencilLoadOp;
        fullKey.storeOps[kDepthSlot] = description.storeOp;
        fullKey.storeOps[kStencilSlot] = description.stencilStoreOp;
        fullKey.initialLayouts[kDepthSlot] = depthLayout;
        fullKey.initialLayouts[kStencilSlot] = stencilLayout;
        compatibleKey.formats[kDepthSlot] = description.format;

        ++m_numAttachmentDescriptions;
        m_hasDepthStencil = true;
    }

    if (initializer.hasFragmentDensityAttachment)
    {
        GP_CHECKF(
            caps.vrsImageFormat != PixelFormat::Unknown, "Shading rate attachment without a shading rate format."
        );
        addShadingRateAttachment(
            caps, pixelFormatToVkFormat(caps.vrsImageFormat), VK_SAMPLE_COUNT_1_BIT, compatibleKey, fullKey
        );
    }

    m_subpassHint = initializer.subpassHint;
    finalize(compatibleKey, fullKey, foundClearOp);
}

VulkanRenderTargetLayout::VulkanRenderTargetLayout(
    const VulkanRenderTargetCaps& caps, const command::SetRenderTargetsInfo& rtInfo
) noexcept
{
    resetAttachments();

    CompatibleHashKey compatibleKey;
    FullHashKey fullKey;
    bool hasExtent = false;
    bool foundClearOp = false;

    const gp::UInt32 numRenderTargets = static_cast<gp::UInt32>(gp::math::clamp<gp::Int32>(
        rtInfo.numColorRenderTargets, 0, static_cast<gp::Int32>(kMaxSimultaneousRenderTargets)
    ));
    for (gp::UInt32 index = 0u; index < numRenderTargets; ++index)
    {
        const command::RenderTargetView& view = rtInfo.colorRenderTarget[index];
        if (view.texture == nullptr)
        {
            continue;
        }

        const resource::TextureDesc& textureDesc = view.texture->getDesc();
        if (!hasExtent)
        {
            hasExtent = true;
            m_extent.width = getMipSize(textureDesc.extent.x, view.mipIndex);
            m_extent.height = getMipSize(textureDesc.extent.y, view.mipIndex);
            m_extent.depth = textureDesc.depth;
        }
        GP_CHECKF(
            (m_numSamples == 0u) || (m_numSamples == view.texture->getNumSamples()),
            "Every color target of a pass must have the same sample count."
        );
        m_numSamples = static_cast<gp::UInt8>(view.texture->getNumSamples());

        VkAttachmentDescription& description = m_descriptions[m_numAttachmentDescriptions];
        description.samples = sampleCountToVkSampleCountFlagBits(m_numSamples);
        description.format = getAttachmentFormat(view.texture->getFormat(), textureDesc.flags);
        description.loadOp = renderTargetLoadActionToVkLoadOp(view.loadAction);
        description.storeOp = renderTargetStoreActionToVkStoreOp(view.storeAction, isMemorylessTexture(*view.texture));
        description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        description.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        foundClearOp = foundClearOp || (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);

        m_colorReferences[m_numColorAttachments].attachment = m_numAttachmentDescriptions;
        m_colorReferences[m_numColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        compatibleKey.formats[m_numColorAttachments] = description.format;
        fullKey.loadOps[m_numColorAttachments] = description.loadOp;
        fullKey.storeOps[m_numColorAttachments] = description.storeOp;
        fullKey.initialLayouts[m_numColorAttachments] = description.initialLayout;

        const bool hasResolveTarget =
            rtInfo.hasResolveAttachments && (rtInfo.colorResolveRenderTarget[index].texture != nullptr);
        if ((description.samples > VK_SAMPLE_COUNT_1_BIT) && hasResolveTarget)
        {
            addColorResolve(compatibleKey);
        }

        ++compatibleKey.numAttachments;
        ++m_numAttachmentDescriptions;
        ++m_numColorAttachments;
    }

    const command::DepthRenderTargetView& depthView = rtInfo.depthStencilRenderTarget;
    if (depthView.texture != nullptr)
    {
        const resource::TextureDesc& textureDesc = depthView.texture->getDesc();
        const bool isMemoryless = isMemorylessTexture(*depthView.texture);
        GP_CHECKF(
            (m_numSamples == 0u) || (m_numSamples == depthView.texture->getNumSamples()),
            "The depth target must have the sample count of the color targets."
        );
        m_numSamples = textureDesc.numSamples;

        VkAttachmentDescription& description = m_descriptions[m_numAttachmentDescriptions];
        description.samples = sampleCountToVkSampleCountFlagBits(m_numSamples);
        description.format = pixelFormatToVkFormat(depthView.texture->getFormat());
        description.loadOp = renderTargetLoadActionToVkLoadOp(depthView.depthLoadAction);
        description.stencilLoadOp = renderTargetLoadActionToVkLoadOp(depthView.stencilLoadAction);
        description.storeOp = renderTargetStoreActionToVkStoreOp(depthView.depthStoreAction, isMemoryless);
        description.stencilStoreOp =
            renderTargetStoreActionToVkStoreOp(depthView.getStencilStoreAction(), isMemoryless);
        foundClearOp = foundClearOp || (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) ||
                       (description.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);

        const resource::ExclusiveDepthStencil access = depthView.getDepthStencilAccess();
        const VkImageLayout depthLayout = getAspectLayout(access.isDepthWrite());
        const VkImageLayout stencilLayout = getAspectLayout(access.isStencilWrite());
        description.initialLayout = depthLayout;
        description.finalLayout = depthLayout;
        m_stencilDescription.stencilInitialLayout = stencilLayout;
        m_stencilDescription.stencilFinalLayout = stencilLayout;

        m_depthReference.attachment = m_numAttachmentDescriptions;
        m_depthReference.layout = depthLayout;
        m_stencilReference.stencilLayout = stencilLayout;

        const bool wantsResolve =
            (depthView.depthStoreAction == command::RenderTargetStoreAction::MultisampleResolve) ||
            (depthView.getStencilStoreAction() == command::RenderTargetStoreAction::MultisampleResolve);
        if (caps.supportsDepthStencilResolve && wantsResolve && (description.samples > VK_SAMPLE_COUNT_1_BIT) &&
            (rtInfo.depthStencilResolveRenderTarget.texture != nullptr))
        {
            addDepthStencilResolve(depthLayout, compatibleKey);
        }

        fullKey.loadOps[kDepthSlot] = description.loadOp;
        fullKey.loadOps[kStencilSlot] = description.stencilLoadOp;
        fullKey.storeOps[kDepthSlot] = description.storeOp;
        fullKey.storeOps[kStencilSlot] = description.stencilStoreOp;
        fullKey.initialLayouts[kDepthSlot] = depthLayout;
        fullKey.initialLayouts[kStencilSlot] = stencilLayout;
        compatibleKey.formats[kDepthSlot] = description.format;

        ++m_numAttachmentDescriptions;
        m_hasDepthStencil = true;

        const gp::UInt32 depthWidth = static_cast<gp::UInt32>(textureDesc.extent.x);
        const gp::UInt32 depthHeight = static_cast<gp::UInt32>(textureDesc.extent.y);
        if (hasExtent)
        {
            m_extent.width = gp::math::min(m_extent.width, depthWidth);
            m_extent.height = gp::math::min(m_extent.height, depthHeight);
        }
        else
        {
            hasExtent = true;
            m_extent.width = depthWidth;
            m_extent.height = depthHeight;
            m_extent.depth = static_cast<VulkanTexture*>(depthView.texture)->getNumberOfArrayLevels();
        }
    }

    if (caps.supportsAttachmentVariableRateShading && (rtInfo.shadingRateTexture != nullptr))
    {
        const resource::Texture& texture = *rtInfo.shadingRateTexture;
        GP_CHECKF(texture.getFormat() == caps.vrsImageFormat, "Shading rate image has an unexpected format.");
        addShadingRateAttachment(
            caps,
            pixelFormatToVkFormat(texture.getFormat()),
            sampleCountToVkSampleCountFlagBits(texture.getNumSamples()),
            compatibleKey,
            fullKey
        );
    }

    m_multiViewCount = rtInfo.multiViewCount;
    m_subpassHint = pipeline::SubpassHint::None;
    finalize(compatibleKey, fullKey, foundClearOp);
}

VulkanRenderTargetLayout::VulkanRenderTargetLayout(
    const VulkanRenderTargetCaps& caps,
    const command::RenderPassInfo& rpInfo,
    const VkImageLayout currentDepthLayout,
    const VkImageLayout currentStencilLayout
) noexcept
{
    resetAttachments();

    CompatibleHashKey compatibleKey;
    FullHashKey fullKey;
    bool hasExtent = false;
    bool foundClearOp = false;
    bool hasMultiviewRenderTargets = false;

    m_multiViewCount = rpInfo.multiViewCount;
    const gp::UInt32 numRenderTargets = rpInfo.getNumColorRenderTargets();
    for (gp::UInt32 index = 0u; index < numRenderTargets; ++index)
    {
        const command::RenderPassInfo::ColorEntry& entry = rpInfo.colorRenderTargets[index];
        const VulkanTexture& texture = *static_cast<const VulkanTexture*>(entry.renderTarget);
        const resource::TextureDesc& textureDesc = texture.getDesc();

        if (!hasExtent)
        {
            hasExtent = true;
            m_extent.width = getMipSize(textureDesc.extent.x, entry.mipIndex);
            m_extent.height = getMipSize(textureDesc.extent.y, entry.mipIndex);
            m_extent.depth = textureDesc.depth;
        }
        GP_CHECKF(
            (m_numSamples == 0u) || (m_numSamples == texture.getNumSamples()) ||
                (rpInfo.subpassHint == pipeline::SubpassHint::CustomResolveSubpass),
            "Every color target of a pass must have the same sample count."
        );
        m_numSamples = static_cast<gp::UInt8>(texture.getNumSamples());
        hasMultiviewRenderTargets = texture.getNumberOfArrayLevels() > 1u;

        const bool isCustomResolveAttachment =
            (index == (numRenderTargets - 1u)) && (rpInfo.subpassHint == pipeline::SubpassHint::CustomResolveSubpass);

        VkAttachmentDescription& description = m_descriptions[m_numAttachmentDescriptions];
        description.samples =
            isCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : sampleCountToVkSampleCountFlagBits(m_numSamples);
        description.format = getAttachmentFormat(texture.getFormat(), textureDesc.flags);
        description.loadOp = renderTargetLoadActionToVkLoadOp(command::getLoadAction(entry.action));
        description.storeOp =
            renderTargetStoreActionToVkStoreOp(command::getStoreAction(entry.action), isMemorylessTexture(texture));
        description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        foundClearOp = foundClearOp || (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);

        // A cleared attachment does not care about its previous contents: starting UNDEFINED skips the load.
        description.initialLayout = (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
                                        ? VK_IMAGE_LAYOUT_UNDEFINED
                                        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        m_colorReferences[m_numColorAttachments].attachment = m_numAttachmentDescriptions;
        m_colorReferences[m_numColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        compatibleKey.formats[m_numColorAttachments] = description.format;
        fullKey.loadOps[m_numColorAttachments] = description.loadOp;
        fullKey.storeOps[m_numColorAttachments] = description.storeOp;
        fullKey.initialLayouts[m_numColorAttachments] = description.initialLayout;

        if ((description.samples > VK_SAMPLE_COUNT_1_BIT) && (entry.resolveTarget != nullptr))
        {
            addColorResolve(compatibleKey);
        }

        ++compatibleKey.numAttachments;
        ++m_numAttachmentDescriptions;
        ++m_numColorAttachments;
    }

    bool hasMultiviewDepthStencil = false;
    const command::RenderPassInfo::DepthStencilEntry& depthEntry = rpInfo.depthStencilRenderTarget;
    if (depthEntry.depthStencilTarget != nullptr)
    {
        const VulkanTexture& texture = *static_cast<const VulkanTexture*>(depthEntry.depthStencilTarget);
        const resource::TextureDesc& textureDesc = texture.getDesc();
        const bool isMemoryless = isMemorylessTexture(texture);
        hasMultiviewDepthStencil = (texture.getNumberOfArrayLevels() > 1u) && !textureDesc.isTextureCube();

        VkAttachmentDescription& description = m_descriptions[m_numAttachmentDescriptions];
        description.samples = sampleCountToVkSampleCountFlagBits(texture.getNumSamples());
        GP_CHECKF(
            (m_numSamples == 0u) || (m_numSamples == texture.getNumSamples()) ||
                (rpInfo.subpassHint == pipeline::SubpassHint::CustomResolveSubpass),
            "The depth target must have the sample count of the color targets."
        );
        m_numSamples = static_cast<gp::UInt8>(texture.getNumSamples());

        const command::RenderTargetActions depthActions = command::getDepthActions(depthEntry.action);
        const command::RenderTargetActions stencilActions = command::getStencilActions(depthEntry.action);
        description.format = pixelFormatToVkFormat(texture.getFormat());
        description.loadOp = renderTargetLoadActionToVkLoadOp(command::getLoadAction(depthActions));
        description.stencilLoadOp = renderTargetLoadActionToVkLoadOp(command::getLoadAction(stencilActions));
        description.storeOp = renderTargetStoreActionToVkStoreOp(command::getStoreAction(depthActions), isMemoryless);
        description.stencilStoreOp =
            renderTargetStoreActionToVkStoreOp(command::getStoreAction(stencilActions), isMemoryless);
        foundClearOp = foundClearOp || (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) ||
                       (description.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);

        // Some drivers corrupt the aspect that is not stored: store both as soon as one is.
        if (caps.requiresDepthStencilFullWrite &&
            (texture.getFullAspectMask() == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) &&
            ((description.storeOp == VK_ATTACHMENT_STORE_OP_STORE) ||
             (description.stencilStoreOp == VK_ATTACHMENT_STORE_OP_STORE)))
        {
            description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        }

        description.initialLayout = currentDepthLayout;
        description.finalLayout = currentDepthLayout;
        m_stencilDescription.stencilInitialLayout = currentStencilLayout;
        m_stencilDescription.stencilFinalLayout = currentStencilLayout;

        // An aspect that was never initialized ends usable; an unused one can skip its load entirely.
        if (currentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            if (!depthEntry.exclusiveDepthStencil.isUsingDepth() && caps.hasEXTLoadStoreOpNone &&
                (description.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD))
            {
                description.loadOp = VK_ATTACHMENT_LOAD_OP_NONE_EXT;
            }
            description.finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }
        if (currentStencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            if (!depthEntry.exclusiveDepthStencil.isUsingStencil() && caps.hasEXTLoadStoreOpNone &&
                (description.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD))
            {
                description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_NONE_EXT;
            }
            m_stencilDescription.stencilFinalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }

        m_depthReference.attachment = m_numAttachmentDescriptions;
        m_depthReference.layout = currentDepthLayout;
        m_stencilReference.stencilLayout = currentStencilLayout;

        if (caps.supportsDepthStencilResolve && (description.samples > VK_SAMPLE_COUNT_1_BIT) &&
            (depthEntry.resolveTarget != nullptr))
        {
            addDepthStencilResolve(currentDepthLayout, compatibleKey);
        }

        fullKey.loadOps[kDepthSlot] = description.loadOp;
        fullKey.loadOps[kStencilSlot] = description.stencilLoadOp;
        fullKey.storeOps[kDepthSlot] = description.storeOp;
        fullKey.storeOps[kStencilSlot] = description.stencilStoreOp;
        fullKey.initialLayouts[kDepthSlot] = currentDepthLayout;
        fullKey.initialLayouts[kStencilSlot] = currentStencilLayout;
        compatibleKey.formats[kDepthSlot] = description.format;

        ++m_numAttachmentDescriptions;
        m_hasDepthStencil = true;

        const gp::UInt32 depthWidth = static_cast<gp::UInt32>(textureDesc.extent.x);
        const gp::UInt32 depthHeight = static_cast<gp::UInt32>(textureDesc.extent.y);
        if (hasExtent)
        {
            m_extent.width = gp::math::min(m_extent.width, depthWidth);
            m_extent.height = gp::math::min(m_extent.height, depthHeight);
        }
        else
        {
            hasExtent = true;
            m_extent.width = depthWidth;
            m_extent.height = depthHeight;
            m_extent.depth = textureDesc.depth;
        }
    }
    else if (numRenderTargets == 0u)
    {
        // Raster-only pass (no attachment at all): the render area comes from the resolve rectangle.
        GP_CHECKF(rpInfo.resolveRect.isValid(), "A pass without render targets needs a valid resolveRect.");
        hasExtent = true;
        m_offset.x = rpInfo.resolveRect.x1;
        m_offset.y = rpInfo.resolveRect.y1;
        m_offset.z = 0;
        m_extent.width = static_cast<gp::UInt32>(gp::math::max(0, rpInfo.resolveRect.x2 - rpInfo.resolveRect.x1));
        m_extent.height = static_cast<gp::UInt32>(gp::math::max(0, rpInfo.resolveRect.y2 - rpInfo.resolveRect.y1));
        m_extent.depth = 1u;
    }

    if (caps.supportsAttachmentVariableRateShading && rpInfo.shadingRateTexture.isValid())
    {
        const resource::Texture& texture = *rpInfo.shadingRateTexture.get();
        GP_CHECKF(texture.getFormat() == caps.vrsImageFormat, "Shading rate image has an unexpected format.");
        addShadingRateAttachment(
            caps,
            pixelFormatToVkFormat(texture.getFormat()),
            sampleCountToVkSampleCountFlagBits(texture.getNumSamples()),
            compatibleKey,
            fullKey
        );
    }

    if ((m_multiViewCount > 1u) && !hasMultiviewRenderTargets &&
        !((numRenderTargets == 0u) && hasMultiviewDepthStencil))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Non multiview textures on a multiview layout.");
    }

    m_subpassHint = rpInfo.subpassHint;
    finalize(compatibleKey, fullKey, foundClearOp);
}

void VulkanRenderTargetLayout::resetAttachments() noexcept
{
    gp::memory::zeroMemory(m_colorReferences.data(), sizeof(VkAttachmentReference) * m_colorReferences.size());
    gp::memory::zeroMemory(m_descriptions.data(), sizeof(VkAttachmentDescription) * m_descriptions.size());
    gp::memory::zeroMemory(&m_depthReference, sizeof(m_depthReference));
    gp::memory::zeroMemory(&m_fragmentDensityReference, sizeof(m_fragmentDensityReference));
    gp::memory::zeroMemory(&m_depthStencilResolveReference, sizeof(m_depthStencilResolveReference));
    gp::memory::zeroMemory(&m_offset, sizeof(m_offset));
    gp::memory::zeroMemory(&m_extent, sizeof(m_extent));
    zeroVulkanStruct(m_stencilReference, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);
    zeroVulkanStruct(m_stencilDescription, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);

    // Colors that do not resolve must point at VK_ATTACHMENT_UNUSED, not at attachment 0.
    for (VkAttachmentReference& reference: m_resolveReferences)
    {
        reference.attachment = VK_ATTACHMENT_UNUSED;
        reference.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

void VulkanRenderTargetLayout::addColorResolve(CompatibleHashKey& compatibleKey) noexcept
{
    const gp::UInt32 resolveIndex = m_numAttachmentDescriptions + 1u;
    VkAttachmentDescription& resolve = m_descriptions[resolveIndex];
    resolve = m_descriptions[m_numAttachmentDescriptions];
    resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    m_resolveReferences[m_numColorAttachments].attachment = resolveIndex;
    m_resolveReferences[m_numColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    compatibleKey.attachmentsToResolve =
        static_cast<gp::UInt16>(compatibleKey.attachmentsToResolve | (1u << m_numColorAttachments));

    ++m_numAttachmentDescriptions;
    m_hasResolveAttachments = true;
}

void VulkanRenderTargetLayout::addDepthStencilResolve(
    const VkImageLayout resolveLayout, CompatibleHashKey& compatibleKey
) noexcept
{
    const gp::UInt32 resolveIndex = m_numAttachmentDescriptions + 1u;
    VkAttachmentDescription& resolve = m_descriptions[resolveIndex];
    resolve = m_descriptions[m_numAttachmentDescriptions];
    resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

    m_depthStencilResolveReference.attachment = resolveIndex;
    m_depthStencilResolveReference.layout = resolveLayout;

    // The depth resolve takes the bit right after the last color.
    GP_CHECK(m_numColorAttachments < 16u);
    compatibleKey.attachmentsToResolve =
        static_cast<gp::UInt16>(compatibleKey.attachmentsToResolve | (1u << m_numColorAttachments));

    ++m_numAttachmentDescriptions;
    m_hasDepthStencilResolve = true;
}

void VulkanRenderTargetLayout::addShadingRateAttachment(
    const VulkanRenderTargetCaps& caps,
    const VkFormat format,
    const VkSampleCountFlagBits samples,
    CompatibleHashKey& compatibleKey,
    FullHashKey& fullKey
) noexcept
{
    const VkImageLayout layout = caps.getShadingRateImageLayout();

    VkAttachmentDescription& description = m_descriptions[m_numAttachmentDescriptions];
    description.flags = 0u;
    description.format = format;
    description.samples = samples;
    description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.initialLayout = layout;
    description.finalLayout = layout;

    m_fragmentDensityReference.attachment = m_numAttachmentDescriptions;
    m_fragmentDensityReference.layout = layout;

    fullKey.loadOps[kShadingRateSlot] = description.loadOp;
    fullKey.storeOps[kShadingRateSlot] = description.storeOp;
    fullKey.initialLayouts[kShadingRateSlot] = layout;
    compatibleKey.formats[kShadingRateFormatSlot] = format;

    ++m_numAttachmentDescriptions;
    m_hasFragmentDensityAttachment = true;
}

void VulkanRenderTargetLayout::finalize(
    CompatibleHashKey& compatibleKey, const FullHashKey& fullKey, const bool foundClearOp
) noexcept
{
    compatibleKey.subpassHint = static_cast<gp::UInt8>(m_subpassHint);
    compatibleKey.numSamples = m_numSamples;
    compatibleKey.multiViewCount = m_multiViewCount;

    gp::hashing::Crc32 compatibleHasher;
    compatibleHasher.update(static_cast<const void*>(&compatibleKey), sizeof(CompatibleHashKey));
    m_compatibleHash = compatibleHasher.finalize();

    // The full hash is seeded with the compatible one: equal full hashes imply compatible passes.
    gp::hashing::Crc32 fullHasher(m_compatibleHash);
    fullHasher.update(static_cast<const void*>(&fullKey), sizeof(FullHashKey));
    m_fullHash = fullHasher.finalize();

    m_numUsedClearValues = foundClearOp ? m_numAttachmentDescriptions : static_cast<gp::UInt8>(0u);
}

}   // namespace gp::rhi::vulkan
