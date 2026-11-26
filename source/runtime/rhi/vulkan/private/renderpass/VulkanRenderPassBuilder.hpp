// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "RHIDefinitions.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "translation/VulkanResourceEnums.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native types of the original render pass API (`vkCreateRenderPass`).
struct VulkanRenderPass1Traits final
{
    /// @brief Attachment reference type.
    using AttachmentReference = VkAttachmentReference;

    /// @brief Subpass description type.
    using SubpassDescription = VkSubpassDescription;

    /// @brief Subpass dependency type.
    using SubpassDependency = VkSubpassDependency;

    /// @brief Attachment description type.
    using AttachmentDescription = VkAttachmentDescription;

    /// @brief Render pass create info type.
    using CreateInfo = VkRenderPassCreateInfo;

    /// @brief False: no `sType`, no `pNext` chains on subpasses, multiview goes through a separate struct.
    static constexpr bool kIsRenderPass2 = false;
};

/// @brief Native types of `VK_KHR_create_renderpass2` (`vkCreateRenderPass2`).
struct VulkanRenderPass2Traits final
{
    /// @brief Attachment reference type.
    using AttachmentReference = VkAttachmentReference2;

    /// @brief Subpass description type.
    using SubpassDescription = VkSubpassDescription2;

    /// @brief Subpass dependency type.
    using SubpassDependency = VkSubpassDependency2;

    /// @brief Attachment description type.
    using AttachmentDescription = VkAttachmentDescription2;

    /// @brief Render pass create info type.
    using CreateInfo = VkRenderPassCreateInfo2;

    /// @brief True: aspect masks, per-subpass view masks and `pNext` chains (resolve, shading rate) are available.
    static constexpr bool kIsRenderPass2 = true;
};

/// @brief Translates a `VulkanRenderTargetLayout` into a complete render pass create info.
/// @details One template serves both APIs: `InTraits` names the native types and `if constexpr` switches on the
///          fields only the second API has. Every array the create info points into is a member, so the builder
///          allocates nothing and the create info stays valid for as long as the builder lives.
///
///          Subpass layouts, selected by the layout's subpass hint:
///          - `None`: one subpass writing every color and the depth-stencil;
///          - `DepthReadSubpass`: subpass 1 writes color 0 and reads depth as input attachment 0;
///          - `DeferredShadingSubpass`: subpass 1 writes every color reading depth, subpass 2 writes color 0 reading
///            depth and colors 1..N as input attachments 2..N+1;
///          - `CustomResolveSubpass`: like `DepthReadSubpass`, then subpass 2 writes the last (single-sampled) color
///            reading color 0 as input attachment 1.
/// @note Not copyable: the create info points into the builder itself.
/// @tparam InTraits `VulkanRenderPass1Traits` or `VulkanRenderPass2Traits`.
template <typename InTraits>
class VulkanRenderPassBuilder final
{
public:
    /// @brief Attachment reference type.
    using AttachmentReference = typename InTraits::AttachmentReference;

    /// @brief Subpass description type.
    using SubpassDescription = typename InTraits::SubpassDescription;

    /// @brief Subpass dependency type.
    using SubpassDependency = typename InTraits::SubpassDependency;

    /// @brief Attachment description type.
    using AttachmentDescription = typename InTraits::AttachmentDescription;

    /// @brief Render pass create info type.
    using CreateInfo = typename InTraits::CreateInfo;

    /// @brief Largest number of subpasses a hint produces.
    static constexpr gp::UInt32 kMaxSubpasses = 4u;

    /// @brief Largest number of input attachments of a subpass: depth, a hole, and every color but one.
    static constexpr gp::UInt32 kMaxInputAttachments = kMaxSimultaneousRenderTargets + 1u;

public:
    /// @brief Binds the builder to the device capabilities.
    /// @param[in] caps Device capabilities.
    /// @param[in] inputAttachmentShaderRead Adds `SHADER_READ` to the deferred shading dependency (driver quirk).
    explicit VulkanRenderPassBuilder(
        const VulkanRenderTargetCaps& caps, const bool inputAttachmentShaderRead = false
    ) noexcept
        : m_caps(caps)
        , m_inputAttachmentShaderRead(inputAttachmentShaderRead)
    {
        zeroVulkanStruct(
            m_createInfo,
            kIsRenderPass2 ? VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2 : VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO
        );
        zeroVulkanStruct(m_multiviewInfo, VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
        zeroVulkanStruct(m_fragmentDensityInfo, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);
        zeroVulkanStruct(m_depthStencilResolve, VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE);
        zeroVulkanStruct(m_depthStencilResolveReference, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
        zeroVulkanStruct(m_shadingRateReference, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
        zeroVulkanStruct(m_stencilReference, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);
        zeroVulkanStruct(m_stencilDescription, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);
        for (VkFragmentShadingRateAttachmentInfoKHR& info: m_shadingRateInfos)
        {
            zeroVulkanStruct(info, VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);
        }
        for (SubpassDescription& subpass: m_subpasses)
        {
            subpass = makeSubpass();
        }
        for (SubpassDependency& dependency: m_dependencies)
        {
            dependency = makeDependency();
        }
    }

    VulkanRenderPassBuilder(const VulkanRenderPassBuilder&) = delete;
    VulkanRenderPassBuilder& operator=(const VulkanRenderPassBuilder&) = delete;
    VulkanRenderPassBuilder(VulkanRenderPassBuilder&&) = delete;
    VulkanRenderPassBuilder& operator=(VulkanRenderPassBuilder&&) = delete;

    /// @brief Fills the create info from a layout.
    /// @note Call once per builder.
    /// @param[in] layout Render target layout.
    void build(const VulkanRenderTargetLayout& layout) noexcept
    {
        const pipeline::SubpassHint hint = layout.getSubpassHint();
        const bool isDeferredShading = hint == pipeline::SubpassHint::DeferredShadingSubpass;
        const bool isCustomResolve = hint == pipeline::SubpassHint::CustomResolveSubpass;
        const bool isDepthRead = isCustomResolve || (hint == pipeline::SubpassHint::DepthReadSubpass);
        const bool hasDepthStencil = layout.getDepthAttachmentReference() != nullptr;

        // Bit mask of the views rendering is broadcast to: 0b11 for two views, 0b1111 for four.
        const gp::UInt32 viewMask = (1u << layout.getMultiViewCount()) - 1u;

        // Shading rate images and in-pass depth resolve are only reachable through the second API.
        const bool applyShadingRate = kIsRenderPass2 && m_caps.supportsAttachmentVariableRateShading &&
                                      m_caps.supportsFragmentShadingRateAttachment &&
                                      (layout.getFragmentDensityAttachmentReference() != nullptr);
        const bool resolveDepth =
            kIsRenderPass2 && m_caps.supportsDepthStencilResolve && layout.getHasDepthStencilResolve();

        if (applyShadingRate)
        {
            const VkAttachmentReference& source = *layout.getFragmentDensityAttachmentReference();
            m_shadingRateReference.attachment = source.attachment;
            m_shadingRateReference.layout = source.layout;
            m_shadingRateReference.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (resolveDepth)
        {
            const VkAttachmentReference& source = *layout.getDepthStencilResolveAttachmentReference();
            m_depthStencilResolveReference.attachment = source.attachment;
            m_depthStencilResolveReference.layout = source.layout;

            // SAMPLE_ZERO is the only resolve mode every implementation of the extension must support.
            m_depthStencilResolve.depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
            m_depthStencilResolve.stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
            m_depthStencilResolve.pDepthStencilResolveAttachment = &m_depthStencilResolveReference;
        }

        // 1. Attachment references.
        m_numColorReferences = layout.getNumColorAttachments();
        m_numResolveReferences = 0u;
        for (gp::UInt32 index = 0u; index < m_numColorReferences; ++index)
        {
            const VkAttachmentReference& color = layout.getColorAttachmentReferences()[index];
            m_colorReferences[index] = makeReference(color.attachment, color.layout, 0u);
            if (layout.getResolveAttachmentReferences() != nullptr)
            {
                const VkAttachmentReference& resolve = layout.getResolveAttachmentReferences()[index];
                m_resolveReferences[m_numResolveReferences] = makeReference(resolve.attachment, resolve.layout, 0u);
                ++m_numResolveReferences;
            }
        }

        // The custom resolve target is written by the last subpass only.
        gp::UInt32 numMainColors = m_numColorReferences;
        if (isCustomResolve && (numMainColors > 1u))
        {
            --numMainColors;
        }

        gp::UInt32 depthInputAttachment = VK_ATTACHMENT_UNUSED;
        VkImageLayout depthInputLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags depthInputAspect = 0u;
        m_depthStencilReference = makeReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, 0u);
        m_depthReadReference = makeReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, 0u);
        if (hasDepthStencil)
        {
            setDepthStencilReference(*layout.getDepthAttachmentReference(), layout.getStencilAttachmentReference());
            if (isDepthRead || isDeferredShading)
            {
                // Later subpasses read depth as an input attachment while lights may still write stencil.
                depthInputAttachment = layout.getDepthAttachmentReference()->attachment;
                depthInputLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
                depthInputAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
                m_depthReadReference = makeReference(depthInputAttachment, depthInputLayout, depthInputAspect);
            }
        }

        // 2. Main subpass.
        m_numSubpasses = 0u;
        m_numDependencies = 0u;
        {
            SubpassDescription& subpass = m_subpasses[m_numSubpasses++];
            setColorAttachments(subpass, m_colorReferences.data(), numMainColors);
            if (hasDepthStencil)
            {
                subpass.pDepthStencilAttachment = &m_depthStencilReference;
            }
            if (!isDepthRead && resolveDepth)
            {
                chainDepthStencilResolve(subpass);
            }
            finishSubpass(subpass, applyShadingRate, viewMask);
        }

        // 3. Color write, depth read subpass.
        if (isDepthRead)
        {
            GP_CHECKF(hasDepthStencil, "A depth read subpass needs a depth-stencil attachment.");
            SubpassDescription& subpass = m_subpasses[m_numSubpasses++];
            setColorAttachments(subpass, m_colorReferences.data(), 1u);

            m_inputAttachments1[0] = makeReference(depthInputAttachment, depthInputLayout, depthInputAspect);
            setInputAttachments(subpass, m_inputAttachments1.data(), 1u);
            subpass.pDepthStencilAttachment = &m_depthReadReference;
            if (resolveDepth && !isCustomResolve)
            {
                chainDepthStencilResolve(subpass);
            }
            finishSubpass(subpass, applyShadingRate, viewMask);

            SubpassDependency& dependency = m_dependencies[m_numDependencies++];
            dependency.srcSubpass = 0u;
            dependency.dstSubpass = 1u;
            dependency.srcStageMask =
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        }

        // 4. Deferred shading: G-buffer write reading depth, then lighting reading the G-buffer.
        if (isDeferredShading)
        {
            {
                SubpassDescription& subpass = m_subpasses[m_numSubpasses++];
                setColorAttachments(subpass, m_colorReferences.data(), m_numColorReferences);
                if (hasDepthStencil)
                {
                    subpass.pDepthStencilAttachment = &m_depthReadReference;
                }
                m_inputAttachments1[0] = makeReference(depthInputAttachment, depthInputLayout, depthInputAspect);
                setInputAttachments(subpass, m_inputAttachments1.data(), 1u);
                finishSubpass(subpass, applyShadingRate, viewMask);

                SubpassDependency& dependency = m_dependencies[m_numDependencies++];
                dependency.srcSubpass = 0u;
                dependency.dstSubpass = 1u;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dependency.srcAccessMask =
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
                dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            }
            {
                SubpassDescription& subpass = m_subpasses[m_numSubpasses++];
                setColorAttachments(subpass, m_colorReferences.data(), 1u);
                if (hasDepthStencil)
                {
                    subpass.pDepthStencilAttachment = &m_depthReadReference;
                }

                // Input 0 is depth, input 1 is a hole (scene color is only written), inputs 2.. are colors 1..
                m_inputAttachments2[0] = makeReference(depthInputAttachment, depthInputLayout, depthInputAspect);
                m_inputAttachments2[1] = makeReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, 0u);
                const gp::UInt32 numColorInputs = (m_numColorReferences > 0u) ? (m_numColorReferences - 1u) : 0u;
                for (gp::UInt32 index = 0u; index < numColorInputs; ++index)
                {
                    m_inputAttachments2[index + 2u] = makeReference(
                        m_colorReferences[index + 1u].attachment,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_ASPECT_COLOR_BIT
                    );
                }
                setInputAttachments(subpass, m_inputAttachments2.data(), numColorInputs + 2u);
                finishSubpass(subpass, applyShadingRate, viewMask);

                SubpassDependency& dependency = m_dependencies[m_numDependencies++];
                dependency.srcSubpass = 1u;
                dependency.dstSubpass = 2u;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
                if (m_inputAttachmentShaderRead)
                {
                    // Not required by the specification; some drivers flicker without it.
                    dependency.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }
                dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            }
        }

        // 5. Custom resolve: write the single-sampled last color, reading color 0.
        if (isCustomResolve)
        {
            GP_CHECKF(m_numColorReferences > 1u, "A custom resolve subpass needs a source and a target color.");
            SubpassDescription& subpass = m_subpasses[m_numSubpasses++];

            m_customResolveColor = makeReference(
                m_colorReferences[m_numColorReferences - 1u].attachment,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT
            );
            // Subpass fetch expects depth in input 0, which this subpass does not read.
            m_inputAttachments3[0] = makeReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, 0u);
            m_inputAttachments3[1] = makeReference(
                m_colorReferences[0].attachment, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
            );

            setColorAttachments(subpass, &m_customResolveColor, 1u);
            setInputAttachments(subpass, m_inputAttachments3.data(), 2u);
            subpass.pDepthStencilAttachment = &m_depthReadReference;
            if (resolveDepth)
            {
                chainDepthStencilResolve(subpass);
            }
            finishSubpass(subpass, false, viewMask);

            SubpassDependency& dependency = m_dependencies[m_numDependencies++];
            dependency.srcSubpass = 1u;
            dependency.dstSubpass = 2u;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        }

        // 6. Resolves. The custom resolve subpass resolves by hand and takes none.
        if (!isCustomResolve && (m_numResolveReferences > 0u))
        {
            if (isDepthRead && (m_numResolveReferences > 1u))
            {
                // The depth read subpass has one color (more would break dual source blending): colors 1.. resolve
                // at the end of the main subpass, color 0 at the end of the depth read subpass.
                m_resolveReferences[m_numResolveReferences] = m_resolveReferences[0];
                m_resolveReferences[0] = makeReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, 0u);
                setResolveAttachments(m_subpasses[0], m_resolveReferences.data(), m_numResolveReferences);
                setResolveAttachments(
                    m_subpasses[m_numSubpasses - 1u], &m_resolveReferences[m_numResolveReferences], 1u
                );
            }
            else
            {
                setResolveAttachments(
                    m_subpasses[m_numSubpasses - 1u], m_resolveReferences.data(), m_numResolveReferences
                );
            }
        }

        // 7. Attachment descriptions.
        m_numAttachmentDescriptions = layout.getNumAttachmentDescriptions();
        for (gp::UInt32 index = 0u; index < m_numAttachmentDescriptions; ++index)
        {
            const VkAttachmentDescription& source = layout.getAttachmentDescriptions()[index];
            const bool isDepthAttachment =
                hasDepthStencil && (index == layout.getDepthAttachmentReference()->attachment);
            m_attachmentDescriptions[index] = isDepthAttachment
                                                  ? makeDepthStencilDescription(source, layout.getStencilDescription())
                                                  : makeDescription(source);
        }

        m_createInfo.attachmentCount = m_numAttachmentDescriptions;
        m_createInfo.pAttachments = m_attachmentDescriptions.data();
        m_createInfo.subpassCount = m_numSubpasses;
        m_createInfo.pSubpasses = m_subpasses.data();
        m_createInfo.dependencyCount = m_numDependencies;
        m_createInfo.pDependencies = (m_numDependencies > 0u) ? m_dependencies.data() : nullptr;

        // 8. Multiview. Views are also declared correlated, which lets the implementation render them concurrently.
        m_correlationMask = viewMask;
        if (layout.getIsMultiView())
        {
            if constexpr (kIsRenderPass2)
            {
                m_createInfo.correlatedViewMaskCount = 1u;
                m_createInfo.pCorrelatedViewMasks = &m_correlationMask;
            }
            else
            {
                GP_CHECKF(m_caps.hasKHRMultiview, "Multiview layout without VK_KHR_multiview.");
                for (gp::UInt32 index = 0u; index < m_numSubpasses; ++index)
                {
                    m_viewMasks[index] = viewMask;
                }
                m_multiviewInfo.subpassCount = m_numSubpasses;
                m_multiviewInfo.pViewMasks = m_viewMasks.data();
                m_multiviewInfo.dependencyCount = 0u;
                m_multiviewInfo.pViewOffsets = nullptr;
                m_multiviewInfo.correlationMaskCount = 1u;
                m_multiviewInfo.pCorrelationMasks = &m_correlationMask;
                m_multiviewInfo.pNext = m_createInfo.pNext;
                m_createInfo.pNext = &m_multiviewInfo;
            }
        }

        // 9. Fragment density map (fractional shading rate images).
        if (m_caps.hasEXTFragmentDensityMap && layout.getHasFragmentDensityAttachment() &&
            (m_caps.vrsImageDataType == pipeline::VRSImageDataType::Fractional))
        {
            m_fragmentDensityInfo.fragmentDensityMapAttachment = *layout.getFragmentDensityAttachmentReference();
            m_fragmentDensityInfo.pNext = m_createInfo.pNext;
            m_createInfo.pNext = &m_fragmentDensityInfo;
        }
    }

    /// @brief Returns the create info.
    /// @return Create info pointing into this builder.
    [[nodiscard]] const CreateInfo& getCreateInfo() const noexcept
    {
        return m_createInfo;
    }

    /// @brief Returns the subpass descriptions.
    /// @return `getCreateInfo().subpassCount` subpasses.
    [[nodiscard]] const SubpassDescription* getSubpasses() const noexcept
    {
        return m_subpasses.data();
    }

private:
    /// @brief Shortcut to the traits flag.
    static constexpr bool kIsRenderPass2 = InTraits::kIsRenderPass2;

private:
    /// @brief Builds an attachment reference.
    /// @param[in] attachment Attachment index or `VK_ATTACHMENT_UNUSED`.
    /// @param[in] layout Layout during the subpass.
    /// @param[in] aspectMask Aspects read through an input attachment (second API only).
    /// @return Reference.
    [[nodiscard]] static AttachmentReference makeReference(
        const gp::UInt32 attachment, const VkImageLayout layout, const VkImageAspectFlags aspectMask
    ) noexcept
    {
        AttachmentReference reference{};
        if constexpr (kIsRenderPass2)
        {
            reference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            reference.pNext = nullptr;
            reference.aspectMask = aspectMask;
        }
        else
        {
            static_cast<void>(aspectMask);
        }
        reference.attachment = attachment;
        reference.layout = layout;
        return reference;
    }

    /// @brief Builds an empty graphics subpass.
    /// @return Zeroed subpass bound to the graphics pipeline.
    [[nodiscard]] static SubpassDescription makeSubpass() noexcept
    {
        SubpassDescription subpass{};
        if constexpr (kIsRenderPass2)
        {
            subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        }
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        return subpass;
    }

    /// @brief Builds an empty dependency.
    /// @return Zeroed dependency; `viewOffset` stays 0 since view-local dependencies are never used.
    [[nodiscard]] static SubpassDependency makeDependency() noexcept
    {
        SubpassDependency dependency{};
        if constexpr (kIsRenderPass2)
        {
            dependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        }
        return dependency;
    }

    /// @brief Converts a color (or resolve, or shading rate) attachment description.
    /// @param[in] source Layout description.
    /// @return Native description.
    [[nodiscard]] static AttachmentDescription makeDescription(const VkAttachmentDescription& source) noexcept
    {
        AttachmentDescription description{};
        if constexpr (kIsRenderPass2)
        {
            description.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        }
        description.flags = source.flags;
        description.format = source.format;
        description.samples = source.samples;
        description.loadOp = source.loadOp;
        description.storeOp = source.storeOp;
        description.stencilLoadOp = source.stencilLoadOp;
        description.stencilStoreOp = source.stencilStoreOp;
        description.initialLayout = source.initialLayout;
        description.finalLayout = source.finalLayout;
        return description;
    }

    /// @brief Converts the depth-stencil description, chaining or merging the stencil layouts.
    /// @param[in] source Layout description.
    /// @param[in] stencil Separate stencil layouts, or `nullptr`.
    /// @return Native description.
    [[nodiscard]] AttachmentDescription makeDepthStencilDescription(
        const VkAttachmentDescription& source, const VkAttachmentDescriptionStencilLayout* const stencil
    ) noexcept
    {
        AttachmentDescription description = makeDescription(source);
        const VkImageLayout stencilInitial =
            (stencil != nullptr) ? stencil->stencilInitialLayout : VK_IMAGE_LAYOUT_UNDEFINED;
        const VkImageLayout stencilFinal =
            (stencil != nullptr) ? stencil->stencilFinalLayout : VK_IMAGE_LAYOUT_UNDEFINED;

        if constexpr (kIsRenderPass2)
        {
            if (m_caps.usesSeparateDepthStencilLayouts())
            {
                // Keep both layouts apart; the stencil ones ride in the pNext chain.
                if (vkFormatHasStencil(source.format) && (stencil != nullptr) &&
                    (stencilFinal != VK_IMAGE_LAYOUT_UNDEFINED))
                {
                    m_stencilDescription = *stencil;
                    m_stencilDescription.pNext = nullptr;
                    description.pNext = &m_stencilDescription;
                }
                return description;
            }
        }

        // Fold the stencil layouts into combined depth-stencil layouts.
        const bool hasStencil = vkFormatHasStencil(source.format);
        description.initialLayout =
            getMergedDepthStencilLayout(source.initialLayout, hasStencil ? stencilInitial : VK_IMAGE_LAYOUT_UNDEFINED);
        description.finalLayout =
            getMergedDepthStencilLayout(source.finalLayout, hasStencil ? stencilFinal : VK_IMAGE_LAYOUT_UNDEFINED);
        return description;
    }

    /// @brief Fills the main depth-stencil reference, chaining or merging the stencil layout.
    /// @param[in] depth Layout depth reference.
    /// @param[in] stencil Separate stencil layout, or `nullptr`.
    void setDepthStencilReference(
        const VkAttachmentReference& depth, const VkAttachmentReferenceStencilLayout* const stencil
    ) noexcept
    {
        const VkImageLayout stencilLayout = (stencil != nullptr) ? stencil->stencilLayout : VK_IMAGE_LAYOUT_UNDEFINED;
        m_depthStencilReference = makeReference(depth.attachment, depth.layout, 0u);
        if constexpr (kIsRenderPass2)
        {
            if (m_caps.usesSeparateDepthStencilLayouts())
            {
                if (stencilLayout != VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    m_stencilReference.stencilLayout = stencilLayout;
                    m_depthStencilReference.pNext = &m_stencilReference;
                }
                return;
            }
        }
        m_depthStencilReference.layout = getMergedDepthStencilLayout(depth.layout, stencilLayout);
    }

    /// @brief Sets the color attachments of a subpass.
    /// @param[in,out] subpass Subpass.
    /// @param[in] references Color references.
    /// @param[in] count Number of colors.
    static void setColorAttachments(
        SubpassDescription& subpass, const AttachmentReference* const references, const gp::UInt32 count
    ) noexcept
    {
        subpass.colorAttachmentCount = count;
        subpass.pColorAttachments = (count > 0u) ? references : nullptr;
    }

    /// @brief Sets the input attachments of a subpass.
    /// @param[in,out] subpass Subpass.
    /// @param[in] references Input references.
    /// @param[in] count Number of inputs.
    static void setInputAttachments(
        SubpassDescription& subpass, const AttachmentReference* const references, const gp::UInt32 count
    ) noexcept
    {
        subpass.inputAttachmentCount = count;
        subpass.pInputAttachments = (count > 0u) ? references : nullptr;
    }

    /// @brief Sets the resolve attachments of a subpass.
    /// @param[in,out] subpass Subpass; must have no more colors than `count`.
    /// @param[in] references Resolve references, one per color.
    /// @param[in] count Number of references available.
    static void setResolveAttachments(
        SubpassDescription& subpass, const AttachmentReference* const references, const gp::UInt32 count
    ) noexcept
    {
        GP_CHECKF(subpass.colorAttachmentCount <= count, "Every color of a resolving subpass needs a resolve slot.");
        static_cast<void>(count);
        subpass.pResolveAttachments = references;
    }

    /// @brief Chains the depth-stencil resolve onto a subpass (second API only).
    /// @param[in,out] subpass Subpass.
    void chainDepthStencilResolve(SubpassDescription& subpass) noexcept
    {
        if constexpr (kIsRenderPass2)
        {
            m_depthStencilResolve.pNext = subpass.pNext;
            subpass.pNext = &m_depthStencilResolve;
        }
        else
        {
            static_cast<void>(subpass);
        }
    }

    /// @brief Applies the per-subpass shading rate attachment and view mask.
    /// @details Each subpass gets its own shading rate struct: sharing one would let the last subpass rewrite the
    ///          `pNext` chain of the earlier ones.
    /// @param[in,out] subpass Subpass, the last one added.
    /// @param[in] applyShadingRate True to chain the shading rate attachment.
    /// @param[in] viewMask Multiview mask.
    void finishSubpass(SubpassDescription& subpass, const bool applyShadingRate, const gp::UInt32 viewMask) noexcept
    {
        if constexpr (kIsRenderPass2)
        {
            subpass.viewMask = viewMask;
            if (applyShadingRate)
            {
                VkFragmentShadingRateAttachmentInfoKHR& info = m_shadingRateInfos[m_numSubpasses - 1u];
                info.pFragmentShadingRateAttachment = &m_shadingRateReference;
                info.shadingRateAttachmentTexelSize = VkExtent2D{ m_caps.vrsTileWidth, m_caps.vrsTileHeight };
                info.pNext = subpass.pNext;
                subpass.pNext = &info;
            }
        }
        else
        {
            static_cast<void>(subpass);
            static_cast<void>(applyShadingRate);
            static_cast<void>(viewMask);
        }
    }

private:
    /// @brief Device capabilities.
    VulkanRenderTargetCaps m_caps;

    /// @brief Adds `SHADER_READ` to the deferred shading dependency.
    bool m_inputAttachmentShaderRead{ false };

    /// @brief Subpasses.
    gp::Array<SubpassDescription, kMaxSubpasses> m_subpasses{};

    /// @brief Dependencies between subpasses.
    gp::Array<SubpassDependency, kMaxSubpasses> m_dependencies{};

    /// @brief Color references.
    gp::Array<AttachmentReference, kMaxSimultaneousRenderTargets> m_colorReferences{};

    /// @brief Resolve references, plus one spare slot for the depth read subpass swap.
    gp::Array<AttachmentReference, kMaxSimultaneousRenderTargets + 1u> m_resolveReferences{};

    /// @brief Input attachments of the depth read and G-buffer subpasses.
    gp::Array<AttachmentReference, 1u> m_inputAttachments1{};

    /// @brief Input attachments of the deferred lighting subpass.
    gp::Array<AttachmentReference, kMaxInputAttachments> m_inputAttachments2{};

    /// @brief Input attachments of the custom resolve subpass.
    gp::Array<AttachmentReference, 2u> m_inputAttachments3{};

    /// @brief Color written by the custom resolve subpass.
    AttachmentReference m_customResolveColor{};

    /// @brief Depth-stencil reference of the main subpass.
    AttachmentReference m_depthStencilReference{};

    /// @brief Read-only depth reference of the later subpasses.
    AttachmentReference m_depthReadReference{};

    /// @brief Attachment descriptions.
    gp::Array<AttachmentDescription, VulkanRenderTargetLayout::kMaxAttachmentDescriptions> m_attachmentDescriptions{};

    /// @brief Separate stencil layout chained on the main depth reference.
    VkAttachmentReferenceStencilLayout m_stencilReference{};

    /// @brief Separate stencil layouts chained on the depth-stencil description.
    VkAttachmentDescriptionStencilLayout m_stencilDescription{};

    /// @brief Shading rate attachment reference.
    VkAttachmentReference2 m_shadingRateReference{};

    /// @brief Shading rate attachment info, one per subpass.
    gp::Array<VkFragmentShadingRateAttachmentInfoKHR, kMaxSubpasses> m_shadingRateInfos{};

    /// @brief Depth-stencil resolve reference.
    VkAttachmentReference2 m_depthStencilResolveReference{};

    /// @brief Depth-stencil resolve description.
    VkSubpassDescriptionDepthStencilResolve m_depthStencilResolve{};

    /// @brief Fragment density map info.
    VkRenderPassFragmentDensityMapCreateInfoEXT m_fragmentDensityInfo{};

    /// @brief Multiview info (first API only).
    VkRenderPassMultiviewCreateInfo m_multiviewInfo{};

    /// @brief Per-subpass view masks of the multiview info.
    gp::Array<gp::UInt32, kMaxSubpasses> m_viewMasks{};

    /// @brief Correlated view mask.
    gp::UInt32 m_correlationMask{ 0u };

    /// @brief Number of color references.
    gp::UInt32 m_numColorReferences{ 0u };

    /// @brief Number of resolve references.
    gp::UInt32 m_numResolveReferences{ 0u };

    /// @brief Number of subpasses.
    gp::UInt32 m_numSubpasses{ 0u };

    /// @brief Number of dependencies.
    gp::UInt32 m_numDependencies{ 0u };

    /// @brief Number of attachment descriptions.
    gp::UInt32 m_numAttachmentDescriptions{ 0u };

    /// @brief Create info.
    CreateInfo m_createInfo{};
};

}   // namespace gp::rhi::vulkan
