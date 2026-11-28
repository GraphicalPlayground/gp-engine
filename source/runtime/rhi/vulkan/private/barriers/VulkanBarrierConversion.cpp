// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierConversion.hpp"
#include "barriers/IVulkanBarrierResourceResolver.hpp"
#include "barriers/VulkanAccessTranslation.hpp"
#include "barriers/VulkanBarrierContext.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanTransitionData.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "sync/ResourceState.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "translation/VulkanResourceEnums.hpp"
#include "VulkanCommon.hpp"
#include <type_traits>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Shared implementation of both `convertTransitionToBarriers()` overloads.
/// @tparam InBarrierArrayType `VulkanLegacyBarrierArrays` or `VulkanSync2BarrierArrays`.
/// @param[in] context Barrier context.
/// @param[in] data Transition payload.
/// @param[in,out] outBarriers Arrays receiving the barriers.
template <typename InBarrierArrayType>
void convertTransitionToBarriersImpl(
    const VulkanBarrierContext& context, const VulkanTransitionData& data, InBarrierArrayType& outBarriers
) noexcept
{
    using MemoryBarrierType = std::remove_cvref_t<decltype(outBarriers.memoryBarriers[0])>;
    using BufferBarrierType = std::remove_cvref_t<decltype(outBarriers.bufferBarriers[0])>;
    using ImageBarrierType = std::remove_cvref_t<decltype(outBarriers.imageBarriers[0])>;

    // Legacy barriers share one stage mask pair across the whole batch.
    constexpr bool kIsLegacyBarrier = std::is_same_v<VulkanLegacyBarrierArrays, InBarrierArrayType>;

    const IVulkanBarrierResourceResolver& resolver = *context.resolver;

    // Count images and buffers to presize the arrays.
    gp::USize numTextures = 0u;
    gp::USize numBuffers = 0u;
    for (const sync::TransitionInfo& info: data.transitionInfos)
    {
        if (info.resource == nullptr)
        {
            continue;
        }

        if ((info.accessAfter & sync::ResourceAccess::Discard) != sync::ResourceAccess::None)
        {
            // Discard as a destination is a no-op.
            continue;
        }

        if (info.type == sync::TransitionResourceType::Texture)
        {
            // CPU readback "textures" are buffers; only count real images.
            if (resolver.resolve(info).type != VulkanBarrierResourceType::ReadbackTexture)
            {
                ++numTextures;
            }
            continue;
        }

        if (info.type == sync::TransitionResourceType::UAV)
        {
            if (resolver.resolve(info).type == VulkanBarrierResourceType::Texture)
            {
                ++numTextures;
                continue;
            }
        }

        if (data.srcPipelines != data.dstPipelines)
        {
            ++numBuffers;
        }
    }

    // Presize every array.
    if (!context.settings.useMemoryBarrierOpt)
    {
        outBarriers.bufferBarriers.reserve(outBarriers.bufferBarriers.size() + numBuffers);
    }
    outBarriers.imageBarriers.reserve(outBarriers.imageBarriers.size() + numTextures);

    for (const sync::TransitionInfo& info: data.transitionInfos)
    {
        if (info.resource == nullptr)
        {
            continue;
        }

        if (info.accessAfter == sync::ResourceAccess::Discard)
        {
            // Discard as a destination is a no-op.
            continue;
        }

        const sync::ResourceState resourceState(*context.trackingContext, data.srcPipelines, data.dstPipelines, info);

        VulkanBarrierResource resolved{};
        const VulkanBarrierResource* buffer = nullptr;
        const VulkanBarrierResource* texture = nullptr;
        const VulkanBarrierResource* readbackTexture = nullptr;
        sync::TransitionResourceType underlyingType = info.type;
        VkBufferUsageFlags usageFlags = 0u;

        switch (info.type)
        {
        case sync::TransitionResourceType::Texture:
        {
            resolved = resolver.resolve(info);
            if (resolved.type == VulkanBarrierResourceType::ReadbackTexture)
            {
                readbackTexture = &resolved;
            }
            else
            {
                texture = &resolved;
            }
            break;
        }

        case sync::TransitionResourceType::Buffer:
        {
            resolved = resolver.resolve(info);
            buffer = &resolved;
            usageFlags = resolved.bufferUsageFlags;
            break;
        }

        case sync::TransitionResourceType::UAV:
        {
            resolved = resolver.resolve(info);
            if (resolved.type == VulkanBarrierResourceType::Texture)
            {
                texture = &resolved;
                underlyingType = sync::TransitionResourceType::Texture;
            }
            else
            {
                buffer = &resolved;
                underlyingType = sync::TransitionResourceType::Buffer;
                usageFlags = resolved.bufferUsageFlags;
            }
            break;
        }

        case sync::TransitionResourceType::BVH:
        {
            // Acceleration structures are synchronized with a memory barrier.
            break;
        }

        default:
            GP_CHECKF(false, "convertTransitionToBarriers() received an unknown transition resource type.");
            continue;
        }

        VkPipelineStageFlags srcStageMask = 0u;
        VkPipelineStageFlags dstStageMask = 0u;
        VkAccessFlags srcAccessFlags = 0u;
        VkAccessFlags dstAccessFlags = 0u;
        VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout dstLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        const bool isDepthStencil = (texture != nullptr) && texture->isDepthOrStencilAspect;

        if (isDepthStencil)
        {
            // Planes are transitioned separately, so the per-plane tables apply as they are.
            srcLayout = getDepthOrStencilLayout(resourceState.accessBefore);
            dstLayout = getDepthOrStencilLayout(resourceState.accessAfter);

            const VulkanStageAccessFlags srcFlags = getDepthOrStencilStageAndAccessFlags(resourceState.accessBefore);
            const VulkanStageAccessFlags dstFlags = getDepthOrStencilStageAndAccessFlags(resourceState.accessAfter);
            srcStageMask = srcFlags.stageFlags;
            srcAccessFlags = srcFlags.accessFlags;
            dstStageMask = dstFlags.stageFlags;
            dstAccessFlags = dstFlags.accessFlags;
        }
        else
        {
            const bool supportsReadOnlyOptimal = (texture == nullptr) || texture->supportsSampling;
            const bool supportsLocalRead = (texture != nullptr) && texture->supportsRenderingLocalRead;

            const VulkanStageAccessLayout srcFlags = getVkStageAndAccessFlags(
                resourceState.accessBefore,
                underlyingType,
                usageFlags,
                isDepthStencil,
                supportsReadOnlyOptimal,
                supportsLocalRead,
                true,
                context.translation
            );
            const VulkanStageAccessLayout dstFlags = getVkStageAndAccessFlags(
                resourceState.accessAfter,
                underlyingType,
                usageFlags,
                isDepthStencil,
                supportsReadOnlyOptimal,
                supportsLocalRead,
                false,
                context.translation
            );
            srcStageMask = srcFlags.stageFlags;
            srcAccessFlags = srcFlags.accessFlags;
            srcLayout = srcFlags.layout;
            dstStageMask = dstFlags.stageFlags;
            dstAccessFlags = dstFlags.accessFlags;
            dstLayout = dstFlags.layout;

            // Kept until non-pixel SRV access is always tracked separately.
            if (!context.translation.supportsDedicatedSRVGraphicsNonPixelAccess)
            {
                // Only compute or transfer can update data read by the vertex stages, so drop those stages unless
                // the source was one of them. SRVGraphicsNonPixel on its own is the exception: it keeps the vertex
                // stages.
                const bool keepVertex = (texture != nullptr) &&
                                        ((resourceState.accessAfter & sync::ResourceAccess::SRVGraphicsNonPixel) !=
                                         sync::ResourceAccess::None) &&
                                        ((resourceState.accessAfter & sync::ResourceAccess::SRVGraphicsPixel) ==
                                         sync::ResourceAccess::None);
                if (!keepVertex &&
                    ((srcStageMask & (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT)) == 0u))
                {
                    dstStageMask &= ~static_cast<VkPipelineStageFlags>(
                        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                    );
                }
            }
        }

        // Legacy barriers merge every stage mask; synchronization2 sets them per barrier.
        if constexpr (kIsLegacyBarrier)
        {
            outBarriers.srcStageMask |= srcStageMask;
            outBarriers.dstStageMask |= dstStageMask;
        }

        // On one pipeline with no layout change, a global memory barrier expresses the dependency.
        if ((data.srcPipelines == data.dstPipelines) && (texture == nullptr || (srcLayout == dstLayout)) &&
            ((readbackTexture != nullptr) || context.settings.useMemoryBarrierOpt))
        {
            if (outBarriers.memoryBarriers.isEmpty())
            {
                MemoryBarrierType& newBarrier = outBarriers.memoryBarriers.emplaceBack();
                zeroVulkanStruct(
                    newBarrier, kIsLegacyBarrier ? VK_STRUCTURE_TYPE_MEMORY_BARRIER : VK_STRUCTURE_TYPE_MEMORY_BARRIER_2
                );
            }

            // Every dependency goes into the single barrier.
            MemoryBarrierType& memoryBarrier = outBarriers.memoryBarriers[0];

            // Only a write needs a memory dependency; after a read, an execution dependency is enough.
            const bool srcAccessIsRead = ((srcAccessFlags & (~kVulkanReadAccessMask)) == 0u);
            if (!srcAccessIsRead)
            {
                memoryBarrier.srcAccessMask |= srcAccessFlags;
                memoryBarrier.dstAccessMask |= dstAccessFlags;
            }

            if constexpr (!kIsLegacyBarrier)
            {
                memoryBarrier.srcStageMask |= srcStageMask;
                memoryBarrier.dstStageMask |= dstStageMask;
            }
        }
        else if (buffer != nullptr)
        {
            BufferBarrierType& bufferBarrier = outBarriers.bufferBarriers.emplaceBack();
            bufferBarrier = BufferBarrierType{};
            bufferBarrier.sType =
                kIsLegacyBarrier ? VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER : VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            bufferBarrier.srcAccessMask = srcAccessFlags;
            bufferBarrier.dstAccessMask = dstAccessFlags;
            bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            bufferBarrier.buffer = buffer->buffer;
            bufferBarrier.offset = buffer->bufferOffset;
            bufferBarrier.size = buffer->bufferSize;

            if constexpr (!kIsLegacyBarrier)
            {
                bufferBarrier.srcStageMask = srcStageMask;
                bufferBarrier.dstStageMask = dstStageMask;
            }
        }
        else if (texture != nullptr)
        {
            const VkImageAspectFlags aspectFlags =
                isDepthStencil ? getDepthStencilAspectMask(info.planeSlice) : texture->fullAspectMask;

            ImageBarrierType& imageBarrier = outBarriers.imageBarriers.emplaceBack();
            imageBarrier = ImageBarrierType{};
            imageBarrier.sType =
                kIsLegacyBarrier ? VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER : VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imageBarrier.srcAccessMask = srcAccessFlags;
            imageBarrier.dstAccessMask = dstAccessFlags;
            imageBarrier.oldLayout = srcLayout;
            imageBarrier.newLayout = dstLayout;
            imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.image = texture->image;

            // With VK_KHR_maintenance9, a layerCount of 1 on a 3D texture created 2D-array compatible addresses a
            // single slice instead of the rest of the texture. Keep the same behavior with and without the extension
            // (drivers without it are left untouched).
            const bool useAllSlices =
                texture->is3DTexture && context.device.hasKHRMaintenance9 && !texture->isReservedResource;

            imageBarrier.subresourceRange.aspectMask = aspectFlags;
            imageBarrier.subresourceRange.baseMipLevel = info.isAllMips() ? 0u : info.mipIndex;
            imageBarrier.subresourceRange.levelCount = info.isAllMips() ? VK_REMAINING_MIP_LEVELS : 1u;
            imageBarrier.subresourceRange.baseArrayLayer = info.isAllArraySlices() ? 0u : info.arraySlice;
            imageBarrier.subresourceRange.layerCount =
                (info.isAllArraySlices() || useAllSlices) ? VK_REMAINING_ARRAY_LAYERS : 1u;

            if constexpr (kIsLegacyBarrier)
            {
                VulkanImageBarrierExtraData& extraData = outBarriers.imageExtraData.emplaceBack();
                extraData = VulkanImageBarrierExtraData{};
                extraData.baseTexture = *texture;
                extraData.isAliasingBarrier = (resourceState.accessBefore == sync::ResourceAccess::Discard);

                if (texture->isDepthOrStencilAspect)
                {
                    extraData.planeAccess = resourceState.accessAfter;
                    extraData.planeSlice = static_cast<gp::UInt8>(info.planeSlice);
                }
            }
            else
            {
                imageBarrier.srcStageMask = srcStageMask;
                imageBarrier.dstStageMask = dstStageMask;
            }
        }
        else
        {
            GP_CHECKF(false, "Transition with no resource!");
        }
    }
}

}   // namespace

void convertTransitionToBarriers(
    const VulkanBarrierContext& context, const VulkanTransitionData& data, VulkanLegacyBarrierArrays& outBarriers
) noexcept
{
    convertTransitionToBarriersImpl(context, data, outBarriers);
}

void convertTransitionToBarriers(
    const VulkanBarrierContext& context, const VulkanTransitionData& data, VulkanSync2BarrierArrays& outBarriers
) noexcept
{
    convertTransitionToBarriersImpl(context, data, outBarriers);
}

void mergePlanes(
    VkImageMemoryBarrier* const imageBarriers,
    const VulkanImageBarrierExtraData* const remainingExtras,
    const gp::Int32 remainingCount
) noexcept
{
    VkImageMemoryBarrier& imageBarrier = imageBarriers[0];
    const VulkanBarrierResource& texture = remainingExtras->baseTexture;
    GP_CHECK(texture.image == imageBarrier.image);
    GP_CHECK(imageBarrier.newLayout != VK_IMAGE_LAYOUT_UNDEFINED);
    GP_CHECK((imageBarrier.oldLayout != VK_IMAGE_LAYOUT_UNDEFINED) || remainingExtras->isAliasingBarrier);

    // When only one aspect of a depth/stencil format is transitioned, look ahead for the barrier on the other aspect.
    if (texture.isDepthOrStencilAspect)
    {
        if (texture.fullAspectMask != imageBarrier.subresourceRange.aspectMask)
        {
            GP_CHECK(
                (imageBarrier.subresourceRange.aspectMask &
                 (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0u
            );

            const VkImageAspectFlags otherAspectMask =
                texture.fullAspectMask ^ imageBarrier.subresourceRange.aspectMask;
            VkImageMemoryBarrier* otherAspectImageBarrier = nullptr;
            for (gp::Int32 otherBarrierIndex = 0; otherBarrierIndex < remainingCount; ++otherBarrierIndex)
            {
                VkImageMemoryBarrier& otherImageBarrier = imageBarriers[otherBarrierIndex];
                const VulkanBarrierResource& otherTexture = remainingExtras[otherBarrierIndex].baseTexture;
                if ((otherTexture.image == imageBarrier.image) &&
                    (otherImageBarrier.subresourceRange.aspectMask == otherAspectMask))
                {
                    GP_CHECK(
                        imageBarrier.subresourceRange.baseArrayLayer ==
                        otherImageBarrier.subresourceRange.baseArrayLayer
                    );
                    GP_CHECK(
                        imageBarrier.subresourceRange.baseMipLevel == otherImageBarrier.subresourceRange.baseMipLevel
                    );

                    otherAspectImageBarrier = &otherImageBarrier;

                    break;
                }
            }

            VkImageLayout otherAspectOldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout otherAspectNewLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (otherAspectImageBarrier != nullptr)
            {
                otherAspectOldLayout = otherAspectImageBarrier->oldLayout;
                otherAspectNewLayout = otherAspectImageBarrier->newLayout;

                // Clear the aspect so the barrier is removed when it is reached.
                otherAspectImageBarrier->subresourceRange.aspectMask = 0u;
            }
            else
            {
                const gp::USize otherPlaneIndex =
                    (imageBarrier.subresourceRange.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT) ? 0u : 1u;
                const sync::ResourceAccess otherPlaneAccess = (texture.allPlanesTrackedAccess != nullptr)
                                                                  ? texture.allPlanesTrackedAccess[otherPlaneIndex]
                                                                  : sync::ResourceAccess::Unknown;

                otherAspectOldLayout = getDepthOrStencilLayout(otherPlaneAccess);
                otherAspectNewLayout = otherAspectOldLayout;
            }

            // Merge the layout with its other half and store it in the barrier.
            if (otherAspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
            {
                imageBarrier.oldLayout = getMergedDepthStencilLayout(imageBarrier.oldLayout, otherAspectOldLayout);
                imageBarrier.newLayout = getMergedDepthStencilLayout(imageBarrier.newLayout, otherAspectNewLayout);
            }
            else
            {
                imageBarrier.oldLayout = getMergedDepthStencilLayout(otherAspectOldLayout, imageBarrier.oldLayout);
                imageBarrier.newLayout = getMergedDepthStencilLayout(otherAspectNewLayout, imageBarrier.newLayout);
            }

            imageBarrier.subresourceRange.aspectMask |= otherAspectMask;
        }
        else
        {
            // Every aspect of the depth(-stencil) texture is transitioned.
            imageBarrier.oldLayout = getMergedDepthStencilLayout(imageBarrier.oldLayout, imageBarrier.oldLayout);
            imageBarrier.newLayout = getMergedDepthStencilLayout(imageBarrier.newLayout, imageBarrier.newLayout);
        }
    }

    // No per-plane synchronization2 layout may be left once the barrier is merged.
    GP_CHECK(
        (imageBarrier.oldLayout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) &&
        (imageBarrier.oldLayout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
    );
    GP_CHECK(
        (imageBarrier.newLayout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) &&
        (imageBarrier.newLayout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
    );
}

}   // namespace gp::rhi::vulkan
