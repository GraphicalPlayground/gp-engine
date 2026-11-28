// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanAccessTranslation.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Tests whether `access` shares at least one bit with `flags`.
/// @param[in] access Access mask to test.
/// @param[in] flags Bits to look for.
/// @return True when any bit of `flags` is set in `access`.
[[nodiscard]] constexpr bool hasAnyAccess(const sync::ResourceAccess access, const sync::ResourceAccess flags) noexcept
{
    return (access & flags) != sync::ResourceAccess::None;
}

/// @brief Tests whether `access` contains every bit of `flags`.
/// @param[in] access Access mask to test.
/// @param[in] flags Bits that must all be present.
/// @return True when every bit of `flags` is set in `access`.
[[nodiscard]] constexpr bool hasAllAccess(const sync::ResourceAccess access, const sync::ResourceAccess flags) noexcept
{
    return (access & flags) == flags;
}

/// @brief Returns the raw bits of an access mask.
/// @param[in] access Access mask.
/// @return Underlying 32-bit value.
[[nodiscard]] constexpr gp::UInt32 toBits(const sync::ResourceAccess access) noexcept
{
    return static_cast<gp::UInt32>(access);
}

}   // namespace

bool validateShadingRateImageType(const VulkanShadingRateImageType type) noexcept
{
    switch (type)
    {
    case VulkanShadingRateImageType::Palette:
        return true;

    case VulkanShadingRateImageType::Fractional:
        return true;

    case VulkanShadingRateImageType::NotSupported:
        GP_CHECKF(
            false,
            "A texture was marked as a shading rate source but attachment VRS is not supported on this device. Ensure " "attachment variable rate shading is supported and enabled before specifying a shading rate attachment."
        );
        break;

    default:
        break;
    }

    return false;
}

VulkanStageAccessLayout getVkStageAndAccessFlags(
    sync::ResourceAccess access,
    const sync::TransitionResourceType resourceType,
    const VkBufferUsageFlags usageFlags,
    const bool isDepthStencil,
    const bool supportsReadOnlyOptimal,
    const bool supportsLocalRead,
    const bool isSourceState,
    const VulkanBarrierTranslationDesc& desc
) noexcept
{
    using sync::ResourceAccess;

    // A multisample resolve through a render pass attachment writes the resolve target exactly like a render target.
    // Callers flag that case with RTV | ResolveDst, which is reduced to plain RTV here.
    constexpr ResourceAccess kResolveAttachmentAccess = ResourceAccess::RTV | ResourceAccess::ResolveDst;
    if (access == kResolveAttachmentAccess)
    {
        access = ResourceAccess::RTV;
    }

    // BVHRead may be combined with SRV bits; it is always handled as BVHRead alone.
    if (hasAnyAccess(access, ResourceAccess::BVHRead))
    {
        access &= ~ResourceAccess::SRVMask;
    }

    VulkanStageAccessLayout result{};
    result.layout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Layout used for SRV reads. Depth/stencil planes are transitioned separately elsewhere, and this function is
    // never called for depth-only or stencil-only transitions, so the generic read-only layout is enough for them.
    const VkImageLayout srvLayout = isDepthStencil            ? VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
                                    : supportsReadOnlyOptimal ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                              : VK_IMAGE_LAYOUT_GENERAL;

    // States which cannot be combined.
    switch (access)
    {
    case ResourceAccess::Discard:
        // Treated like Unknown for now, which over-synchronizes.
        result.stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        result.accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        result.layout = isSourceState ? VK_IMAGE_LAYOUT_UNDEFINED : srvLayout;
        return result;

    case ResourceAccess::Unknown:
        // The previous user is unknown, so stall everything.
        result.stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        result.accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        return result;

    case ResourceAccess::CPURead:
        result.stageFlags = VK_PIPELINE_STAGE_HOST_BIT;
        result.accessFlags = VK_ACCESS_HOST_READ_BIT;
        result.layout = VK_IMAGE_LAYOUT_GENERAL;
        return result;

    case ResourceAccess::Present:
        result.stageFlags =
            isSourceState ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        // Leaving present, the acquire semaphore covers access. Entering present, vkQueuePresentKHR guarantees
        // visibility.
        result.accessFlags = 0u;
        result.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        return result;

    case ResourceAccess::ResolveDst:
        // Color resolve targets are written by the render pass or dynamic rendering resolve, which runs in the color
        // attachment output stage and expects COLOR_ATTACHMENT_OPTIMAL. Handled exactly like RTV.
        [[fallthrough]];
    case ResourceAccess::RTV:
        result.stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        result.accessFlags = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (supportsLocalRead)
        {
            // Targets readable as input attachments stay in the local read layout for the whole render pass instance
            // so they can be read while still attached (VK_KHR_dynamic_rendering_local_read).
            result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            result.accessFlags |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            result.layout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ;
        }
        else
        {
            result.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        return result;

    case ResourceAccess::CopyDest:
        result.stageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        result.accessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        return result;

    case ResourceAccess::BVHRead:
        result.stageFlags =
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (desc.supportsRayTracingShaders)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
        result.accessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        return result;

    case ResourceAccess::BVHWrite:
        result.stageFlags =
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (desc.supportsRayTracingShaders)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
        result.accessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        return result;

    default:
        break;
    }

    // DSVWrite decides the layout, so every other flag is ignored when it is set.
    if (hasAnyAccess(access, ResourceAccess::DSVWrite))
    {
        GP_CHECK(isDepthStencil);
        result.stageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        return result;
    }

    // The remaining flags can be combined.
    result.stageFlags = 0u;
    result.accessFlags = 0u;
    gp::UInt32 processedFlags = 0u;

    if (hasAnyAccess(access, ResourceAccess::IndirectArgs))
    {
        GP_CHECK(resourceType != sync::TransitionResourceType::Texture);
        result.stageFlags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        result.accessFlags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        processedFlags |= toBits(ResourceAccess::IndirectArgs);
    }

    if (hasAnyAccess(access, ResourceAccess::VertexOrIndexBuffer))
    {
        GP_CHECK(resourceType != sync::TransitionResourceType::Texture);
        result.stageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        switch (resourceType)
        {
        case sync::TransitionResourceType::Buffer:
            if ((usageFlags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0u)
            {
                result.accessFlags |= VK_ACCESS_INDEX_READ_BIT;
            }
            if ((usageFlags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0u)
            {
                result.accessFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            }
            break;
        default:
            GP_CHECKF(false, "VertexOrIndexBuffer access is only valid on buffers.");
            break;
        }

        processedFlags |= toBits(ResourceAccess::VertexOrIndexBuffer);
    }

    if (hasAnyAccess(access, ResourceAccess::DSVRead))
    {
        GP_CHECK(isDepthStencil);
        result.stageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        // An SRV flag below sets the layout to srvLayout again, which resolves to the same value for depth/stencil.
        result.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

        processedFlags |= toBits(ResourceAccess::DSVRead);
    }

    if (hasAnyAccess(access, ResourceAccess::SRVGraphicsNonPixel))
    {
        result.stageFlags |=
            (desc.graphicsPipelineStageBits &
             ~static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        if ((usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u)
        {
            result.accessFlags |= VK_ACCESS_UNIFORM_READ_BIT;
        }
        if ((usageFlags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0u)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            result.accessFlags |= VK_ACCESS_INDEX_READ_BIT;
        }
        if ((usageFlags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0u)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            result.accessFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        }
        result.layout = srvLayout;

        processedFlags |= toBits(ResourceAccess::SRVGraphicsNonPixel);
    }

    if (hasAnyAccess(access, ResourceAccess::SRVGraphicsPixel))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        if ((usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u)
        {
            result.accessFlags |= VK_ACCESS_UNIFORM_READ_BIT;
        }
        result.layout = srvLayout;

        processedFlags |= toBits(ResourceAccess::SRVGraphicsPixel);
    }

    if (hasAnyAccess(access, ResourceAccess::SRVCompute))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        // Images ping-ponged between UAVCompute and SRVCompute would need fewer barriers if they stayed in GENERAL,
        // but that pattern cannot be detected here. Callers can use UAV to UAV transitions for it instead.
        result.layout = srvLayout;

        processedFlags |= toBits(ResourceAccess::SRVCompute);
    }

    if (hasAnyAccess(access, ResourceAccess::UAVGraphics))
    {
        result.stageFlags |= desc.graphicsPipelineStageBits;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_GENERAL;

        processedFlags |= toBits(ResourceAccess::UAVGraphics);
    }

    if (hasAnyAccess(access, ResourceAccess::UAVCompute))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_GENERAL;

        processedFlags |= toBits(ResourceAccess::UAVCompute);
    }

    // ResolveSrc is a copy source from Vulkan's point of view.
    if (hasAnyAccess(access, ResourceAccess::CopySrc | ResourceAccess::ResolveSrc))
    {
        if (resourceType == sync::TransitionResourceType::Texture)
        {
            if (result.layout == VK_IMAGE_LAYOUT_UNDEFINED)
            {
                // CopySrc alone gets the dedicated transfer layout.
                result.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                result.stageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
                result.accessFlags = VK_ACCESS_TRANSFER_READ_BIT;
            }
            else
            {
                // Mixed with any other read, the image has to go to the catch-all GENERAL layout.
                result.layout = VK_IMAGE_LAYOUT_GENERAL;
                result.stageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                result.accessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
            }
        }
        else
        {
            result.stageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            result.accessFlags = VK_ACCESS_TRANSFER_READ_BIT;
        }

        processedFlags |= toBits(ResourceAccess::CopySrc | ResourceAccess::ResolveSrc);
    }

    if (hasAnyAccess(access, ResourceAccess::ShadingRateSource) &&
        validateShadingRateImageType(desc.shadingRateImageType))
    {
        GP_CHECKF(
            resourceType == sync::TransitionResourceType::Texture,
            "A non-texture resource was tagged as a shading rate source; only textures (Texture2D and Texture2DArray) " "can be used for this purpose."
        );

        if (desc.shadingRateImageType == VulkanShadingRateImageType::Palette)
        {
            result.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
            result.accessFlags = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
            result.layout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
        }

        if (desc.shadingRateImageType == VulkanShadingRateImageType::Fractional)
        {
            result.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
            result.accessFlags = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
            result.layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
        }

        processedFlags |= toBits(ResourceAccess::ShadingRateSource);
    }

    const gp::UInt32 remainingFlags = toBits(access) & (~processedFlags);
    static_cast<void>(GP_ENSURE_MSG(
        remainingFlags == 0u,
        "Some access flags were not processed. Access=%x, ProcessedFlags=%x, RemainingFlags=%x",
        toBits(access),
        processedFlags,
        remainingFlags
    ));

    return result;
}

VulkanStageAccessFlags getDepthOrStencilStageAndAccessFlags(const sync::ResourceAccess access) noexcept
{
    using sync::ResourceAccess;

    VulkanStageAccessFlags result{};

    if (access == ResourceAccess::Unknown || access == ResourceAccess::Discard)
    {
        result.stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        result.accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        return result;
    }

    gp::UInt32 processedFlags = 0u;

    if (hasAllAccess(access, ResourceAccess::ResolveDst))
    {
        // Resolves run in the color attachment output stage, even for depth/stencil targets.
        result.stageFlags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        result.accessFlags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        processedFlags |= toBits(ResourceAccess::ResolveDst);
    }

    if (hasAnyAccess(access, ResourceAccess::DSVWrite))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags |=
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        processedFlags |= toBits(ResourceAccess::DSVWrite);
    }

    if (hasAnyAccess(access, ResourceAccess::DSVRead))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        processedFlags |= toBits(ResourceAccess::DSVRead);
    }

    if (hasAnyAccess(access, ResourceAccess::SRVGraphics))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        processedFlags |= toBits(ResourceAccess::SRVGraphics);
    }

    if (hasAnyAccess(access, ResourceAccess::UAVGraphics))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        processedFlags |= toBits(ResourceAccess::UAVGraphics);
    }

    if (hasAnyAccess(access, ResourceAccess::SRVCompute))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        processedFlags |= toBits(ResourceAccess::SRVCompute);
    }

    if (hasAnyAccess(access, ResourceAccess::UAVCompute))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        processedFlags |= toBits(ResourceAccess::UAVCompute);
    }

    if (hasAnyAccess(access, ResourceAccess::CopySrc))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        result.accessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
        processedFlags |= toBits(ResourceAccess::CopySrc);
    }

    if (hasAnyAccess(access, ResourceAccess::CopyDest))
    {
        result.stageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        result.accessFlags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        processedFlags |= toBits(ResourceAccess::CopyDest);
    }

    const gp::UInt32 remainingFlags = toBits(access) & (~processedFlags);
    static_cast<void>(GP_ENSURE_MSG(
        remainingFlags == 0u,
        "Some access flags were not processed. Access=%x, ProcessedFlags=%x, RemainingFlags=%x",
        toBits(access),
        processedFlags,
        remainingFlags
    ));

    return result;
}

}   // namespace gp::rhi::vulkan
