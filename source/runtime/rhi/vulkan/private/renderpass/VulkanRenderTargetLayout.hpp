// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"              // IWYU pragma: keep
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"             // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native attachment layout of a render pass, derived from the RHI render target bindings.
/// @details One layout describes every attachment of a pass (colors, their resolves, depth-stencil, its resolve and
///          the shading rate image) in `VkAttachmentDescription` order, plus the references a subpass needs. Three
///          sources produce one:
///          - a graphics pipeline initializer (formats only: the pipeline is compiled against a *compatible* pass);
///          - a `SetRenderTargetsInfo` (legacy bindings, layouts derived from the depth-stencil access);
///          - a `RenderPassInfo` and the layouts the depth-stencil image is currently in.
///
///          Two CRC32 hashes are computed at construction:
///          - the *compatible* hash covers what Vulkan render pass compatibility depends on (formats, sample count,
///            resolve set, subpass hint, multiview count). Pipelines and framebuffers key on it;
///          - the *full* hash chains the load/store ops and initial layouts on top of it. Render passes key on it.
///
///          Attachment order inside `getAttachmentDescriptions()`:
///          `color0, [resolve0], color1, [resolve1], ..., depth, [depthResolve], [shadingRate]`.
/// @note A layout is a value type with no pointers to itself; it is copied freely into render passes and caches.
class GP_RHI_VULKAN_API VulkanRenderTargetLayout final
{
public:
    /// @brief Largest number of attachment descriptions: every color plus its resolve, depth, its resolve, and VRS.
    static constexpr gp::UInt32 kMaxAttachmentDescriptions = (kMaxSimultaneousRenderTargets * 2u) + 3u;

public:
    /// @brief Builds an empty layout (no attachment, zero hashes).
    VulkanRenderTargetLayout() noexcept;

    /// @brief Builds the layout a graphics pipeline is compiled against.
    /// @details Load and store ops of colors are `DONT_CARE`: only the compatible hash matters for a pipeline. Every
    ///          multisampled color gets a resolve attachment so the pipeline stays compatible with resolving passes.
    /// @param[in] caps Device capabilities.
    /// @param[in] initializer Pipeline state initializer.
    VulkanRenderTargetLayout(
        const VulkanRenderTargetCaps& caps, const pipeline::GraphicsPipelineStateInitializer& initializer
    ) noexcept;

    /// @brief Builds the layout of a legacy render target binding.
    /// @param[in] caps Device capabilities.
    /// @param[in] rtInfo Render target binding; every texture must be a `VulkanTexture`.
    VulkanRenderTargetLayout(const VulkanRenderTargetCaps& caps, const command::SetRenderTargetsInfo& rtInfo) noexcept;

    /// @brief Builds the layout of a render pass.
    /// @details Color attachments that are cleared start `UNDEFINED`, which lets the driver skip the load. The
    ///          depth-stencil attachment starts and ends in the given layouts; an `UNDEFINED` aspect ends in
    ///          `ATTACHMENT_OPTIMAL`. A pass with no attachment at all takes its area from `resolveRect`.
    /// @param[in] caps Device capabilities.
    /// @param[in] rpInfo Render pass description; every texture must be a `VulkanTexture`.
    /// @param[in] currentDepthLayout Layout the depth aspect is in when the pass begins.
    /// @param[in] currentStencilLayout Layout the stencil aspect is in when the pass begins.
    VulkanRenderTargetLayout(
        const VulkanRenderTargetCaps& caps,
        const command::RenderPassInfo& rpInfo,
        VkImageLayout currentDepthLayout,
        VkImageLayout currentStencilLayout
    ) noexcept;

    /// @brief Returns the hash render pass compatibility depends on.
    /// @return CRC32 of formats, samples, resolves, subpass hint and multiview count.
    [[nodiscard]] gp::UInt32 getRenderPassCompatibleHash() const noexcept
    {
        return m_compatibleHash;
    }

    /// @brief Returns the hash that also covers load/store ops and initial layouts.
    /// @return CRC32 chained on the compatible hash.
    [[nodiscard]] gp::UInt32 getRenderPassFullHash() const noexcept
    {
        return m_fullHash;
    }

    /// @brief Returns the 2D render area offset.
    /// @return Offset of the render area.
    [[nodiscard]] VkOffset2D getOffset2D() const noexcept
    {
        return VkOffset2D{ m_offset.x, m_offset.y };
    }

    /// @brief Returns the 3D render area offset.
    /// @return Offset of the render area; `z` is always 0.
    [[nodiscard]] const VkOffset3D& getOffset3D() const noexcept
    {
        return m_offset;
    }

    /// @brief Returns the 2D render area extent.
    /// @return Width and height of the render area.
    [[nodiscard]] VkExtent2D getExtent2D() const noexcept
    {
        return VkExtent2D{ m_extent.width, m_extent.height };
    }

    /// @brief Returns the 3D render area extent.
    /// @return Width, height and layer count (or 3D depth) of the render area.
    [[nodiscard]] const VkExtent3D& getExtent3D() const noexcept
    {
        return m_extent;
    }

    /// @brief Returns the attachment descriptions, in attachment index order.
    /// @return Pointer to `getNumAttachmentDescriptions()` descriptions.
    [[nodiscard]] const VkAttachmentDescription* getAttachmentDescriptions() const noexcept
    {
        return m_descriptions.data();
    }

    /// @brief Returns the number of attachment descriptions.
    /// @return Attachment count, resolves and shading rate image included.
    [[nodiscard]] gp::UInt32 getNumAttachmentDescriptions() const noexcept
    {
        return m_numAttachmentDescriptions;
    }

    /// @brief Returns the number of color attachments.
    /// @return Color attachment count, resolves excluded.
    [[nodiscard]] gp::UInt32 getNumColorAttachments() const noexcept
    {
        return m_numColorAttachments;
    }

    /// @brief Reports whether the pass has a depth-stencil attachment.
    /// @return True when a depth-stencil attachment is present.
    [[nodiscard]] bool getHasDepthStencil() const noexcept
    {
        return m_hasDepthStencil;
    }

    /// @brief Reports whether at least one color attachment resolves.
    /// @return True when color resolve attachments are present.
    [[nodiscard]] bool getHasResolveAttachments() const noexcept
    {
        return m_hasResolveAttachments;
    }

    /// @brief Reports whether the depth-stencil attachment resolves inside the pass.
    /// @return True when a depth-stencil resolve attachment is present.
    [[nodiscard]] bool getHasDepthStencilResolve() const noexcept
    {
        return m_hasDepthStencilResolve;
    }

    /// @brief Reports whether the pass reads a shading rate (or fragment density) image.
    /// @return True when the shading rate attachment is present.
    [[nodiscard]] bool getHasFragmentDensityAttachment() const noexcept
    {
        return m_hasFragmentDensityAttachment;
    }

    /// @brief Returns the sample count shared by the multisampled attachments.
    /// @return Sample count, or 0 for a pass without attachments.
    [[nodiscard]] gp::UInt32 getNumSamples() const noexcept
    {
        return m_numSamples;
    }

    /// @brief Returns how many clear values `vkCmdBeginRenderPass` must receive.
    /// @return Attachment count when any attachment clears, otherwise 0.
    [[nodiscard]] gp::UInt32 getNumUsedClearValues() const noexcept
    {
        return m_numUsedClearValues;
    }

    /// @brief Reports whether the pass broadcasts to several views.
    /// @return True when the multiview count is not zero.
    [[nodiscard]] bool getIsMultiView() const noexcept
    {
        return m_multiViewCount != 0u;
    }

    /// @brief Returns the multiview count.
    /// @return Number of views, or 0 without multiview.
    [[nodiscard]] gp::UInt32 getMultiViewCount() const noexcept
    {
        return m_multiViewCount;
    }

    /// @brief Returns the subpass layout hint.
    /// @return Subpass hint.
    [[nodiscard]] pipeline::SubpassHint getSubpassHint() const noexcept
    {
        return m_subpassHint;
    }

    /// @brief Returns the color attachment references.
    /// @return `getNumColorAttachments()` references, or `nullptr` without color attachments.
    [[nodiscard]] const VkAttachmentReference* getColorAttachmentReferences() const noexcept
    {
        return (m_numColorAttachments > 0u) ? m_colorReferences.data() : nullptr;
    }

    /// @brief Returns the color resolve references, one per color attachment.
    /// @return References (`VK_ATTACHMENT_UNUSED` for colors that do not resolve), or `nullptr` without resolves.
    [[nodiscard]] const VkAttachmentReference* getResolveAttachmentReferences() const noexcept
    {
        return m_hasResolveAttachments ? m_resolveReferences.data() : nullptr;
    }

    /// @brief Returns the depth attachment reference.
    /// @return Reference, or `nullptr` without depth-stencil.
    [[nodiscard]] const VkAttachmentReference* getDepthAttachmentReference() const noexcept
    {
        return m_hasDepthStencil ? &m_depthReference : nullptr;
    }

    /// @brief Returns the separate stencil layout of the depth reference.
    /// @return Stencil layout, or `nullptr` without depth-stencil.
    [[nodiscard]] const VkAttachmentReferenceStencilLayout* getStencilAttachmentReference() const noexcept
    {
        return m_hasDepthStencil ? &m_stencilReference : nullptr;
    }

    /// @brief Returns the depth-stencil resolve reference.
    /// @return Reference, or `nullptr` without depth-stencil resolve.
    [[nodiscard]] const VkAttachmentReference* getDepthStencilResolveAttachmentReference() const noexcept
    {
        return m_hasDepthStencilResolve ? &m_depthStencilResolveReference : nullptr;
    }

    /// @brief Returns the shading rate attachment reference.
    /// @return Reference, or `nullptr` without shading rate image.
    [[nodiscard]] const VkAttachmentReference* getFragmentDensityAttachmentReference() const noexcept
    {
        return m_hasFragmentDensityAttachment ? &m_fragmentDensityReference : nullptr;
    }

    /// @brief Returns the separate stencil layouts of the depth-stencil description.
    /// @return Stencil layouts, or `nullptr` without depth-stencil.
    [[nodiscard]] const VkAttachmentDescriptionStencilLayout* getStencilDescription() const noexcept
    {
        return m_hasDepthStencil ? &m_stencilDescription : nullptr;
    }

private:
    /// @brief Hashed bytes of render pass compatibility; defined in the source file.
    struct CompatibleHashKey;

    /// @brief Hashed bytes of load/store ops and initial layouts; defined in the source file.
    struct FullHashKey;

private:
    /// @brief Zeroes every attachment and reference.
    void resetAttachments() noexcept;

    /// @brief Appends a single-sampled resolve description for the color attachment that was just written.
    /// @param[in,out] compatibleKey Receives the resolve bit of the color attachment.
    void addColorResolve(CompatibleHashKey& compatibleKey) noexcept;

    /// @brief Appends a single-sampled resolve description for the depth-stencil attachment just written.
    /// @param[in] resolveLayout Layout of the resolve reference.
    /// @param[in,out] compatibleKey Receives the depth resolve bit.
    void addDepthStencilResolve(VkImageLayout resolveLayout, CompatibleHashKey& compatibleKey) noexcept;

    /// @brief Appends the shading rate attachment.
    /// @param[in] caps Device capabilities; provide the layout.
    /// @param[in] format Native format of the shading rate image.
    /// @param[in] samples Sample count of the shading rate image.
    /// @param[in,out] compatibleKey Receives the shading rate format.
    /// @param[in,out] fullKey Receives the shading rate ops and layout.
    void addShadingRateAttachment(
        const VulkanRenderTargetCaps& caps,
        VkFormat format,
        VkSampleCountFlagBits samples,
        CompatibleHashKey& compatibleKey,
        FullHashKey& fullKey
    ) noexcept;

    /// @brief Computes both hashes and the clear value count.
    /// @param[in,out] compatibleKey Compatibility key; receives the samples, hint and multiview count.
    /// @param[in] fullKey Load/store key.
    /// @param[in] foundClearOp True when any attachment clears.
    void finalize(CompatibleHashKey& compatibleKey, const FullHashKey& fullKey, bool foundClearOp) noexcept;

private:
    /// @brief Color references, one per color attachment.
    gp::Array<VkAttachmentReference, kMaxSimultaneousRenderTargets> m_colorReferences{};

    /// @brief Color resolve references, one per color attachment.
    gp::Array<VkAttachmentReference, kMaxSimultaneousRenderTargets> m_resolveReferences{};

    /// @brief Attachment descriptions in attachment index order.
    gp::Array<VkAttachmentDescription, kMaxAttachmentDescriptions> m_descriptions{};

    /// @brief Depth reference.
    VkAttachmentReference m_depthReference{};

    /// @brief Separate stencil layout of the depth reference.
    VkAttachmentReferenceStencilLayout m_stencilReference{};

    /// @brief Shading rate reference.
    VkAttachmentReference m_fragmentDensityReference{};

    /// @brief Depth-stencil resolve reference.
    VkAttachmentReference m_depthStencilResolveReference{};

    /// @brief Separate stencil layouts of the depth-stencil description.
    VkAttachmentDescriptionStencilLayout m_stencilDescription{};

    /// @brief Render area offset.
    VkOffset3D m_offset{};

    /// @brief Render area extent; `depth` is the layer count or the 3D depth.
    VkExtent3D m_extent{};

    /// @brief Hash render pass compatibility depends on.
    gp::UInt32 m_compatibleHash{ 0u };

    /// @brief Hash that also covers load/store ops and initial layouts.
    gp::UInt32 m_fullHash{ 0u };

    /// @brief Number of attachment descriptions.
    gp::UInt8 m_numAttachmentDescriptions{ 0u };

    /// @brief Number of color attachments.
    gp::UInt8 m_numColorAttachments{ 0u };

    /// @brief Sample count of the multisampled attachments.
    gp::UInt8 m_numSamples{ 0u };

    /// @brief Number of clear values the pass consumes.
    gp::UInt8 m_numUsedClearValues{ 0u };

    /// @brief Multiview count.
    gp::UInt8 m_multiViewCount{ 0u };

    /// @brief Subpass layout hint.
    pipeline::SubpassHint m_subpassHint{ pipeline::SubpassHint::None };

    /// @brief A depth-stencil attachment is present.
    bool m_hasDepthStencil{ false };

    /// @brief Color resolve attachments are present.
    bool m_hasResolveAttachments{ false };

    /// @brief A depth-stencil resolve attachment is present.
    bool m_hasDepthStencilResolve{ false };

    /// @brief A shading rate attachment is present.
    bool m_hasFragmentDensityAttachment{ false };
};

}   // namespace gp::rhi::vulkan
