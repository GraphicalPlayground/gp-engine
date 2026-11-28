// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Context describing active device capabilities when translating `sync::ResourceAccess` to Vulkan barriers.
struct VulkanAccessTranslationContext final
{
    /// @brief Combined graphics pipeline shader stage mask supported by the logical device.
    VkPipelineStageFlags deviceGraphicsPipelineStageBits{ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };

    /// @brief Resource kind targeted by the barrier (`Texture`, `Buffer`, `UAV`, or `BVH`).
    sync::TransitionResourceType resourceType{ sync::TransitionResourceType::Unknown };

    /// @brief Native `VkBufferUsageFlags` for buffer resources (`0` for images).
    VkBufferUsageFlags bufferUsageFlags{ 0u };

    /// @brief Active VRS image encoding mode when translating `ResourceAccess::ShadingRateSource`.
    pipeline::VRSImageDataType vrsImageDataType{ pipeline::VRSImageDataType::NotSupported };

    /// @brief True when the target image uses a depth or depth-stencil format.
    bool isDepthStencil{ false };

    /// @brief True when the image supports `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` for SRV reads.
    bool supportsReadOnlyOptimal{ true };

    /// @brief True when `VK_KHR_dynamic_rendering_local_read` is active on an input-attachment-readable target.
    bool supportsLocalRead{ false };

    /// @brief True when hardware ray tracing shader stages are enabled on the device.
    bool supportsRayTracingShaders{ false };

    /// @brief True when translating the source (before) state of a transition rather than the destination (after)
    /// state.
    bool isSourceState{ false };
};

/// @brief Translated Vulkan pipeline stage mask, memory access mask, and target image layout.
struct VulkanBarrierState final
{
    /// @brief Translated `VkPipelineStageFlags` bitmask.
    VkPipelineStageFlags stageFlags{ 0u };

    /// @brief Translated `VkAccessFlags` bitmask.
    VkAccessFlags accessFlags{ 0u };

    /// @brief Translated `VkImageLayout` (`VK_IMAGE_LAYOUT_UNDEFINED` for non-image or unspecified states).
    VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };

    /// @brief Bitmask of unrecognized `sync::ResourceAccess` bits that were not translated (zero on valid inputs).
    gp::UInt32 unprocessedFlags{ 0u };
};

/// @brief Translates an RHI `TextureDimension` into its underlying `VkImageType`.
/// @param[in] dimension Engine texture dimensionality.
/// @return `VK_IMAGE_TYPE_2D` for 2D/array/cube textures, `VK_IMAGE_TYPE_3D` for volume textures, or
///         `VK_IMAGE_TYPE_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkImageType
    textureDimensionToVkImageType(const resource::TextureDimension dimension) noexcept
{
    switch (dimension)
    {
    case resource::TextureDimension::Texture2D:
    case resource::TextureDimension::Texture2DArray:
    case resource::TextureDimension::TextureCube:
    case resource::TextureDimension::TextureCubeArray:
        return VK_IMAGE_TYPE_2D;
    case resource::TextureDimension::Texture3D:
        return VK_IMAGE_TYPE_3D;
    default:
        return VK_IMAGE_TYPE_MAX_ENUM;
    }
}

/// @brief Translates an RHI `TextureDimension` into its `VkImageViewType`.
/// @param[in] dimension Engine texture dimensionality.
/// @return Matching `VkImageViewType`, or `VK_IMAGE_VIEW_TYPE_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkImageViewType
    textureDimensionToVkImageViewType(const resource::TextureDimension dimension) noexcept
{
    switch (dimension)
    {
    case resource::TextureDimension::Texture2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case resource::TextureDimension::Texture2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case resource::TextureDimension::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case resource::TextureDimension::TextureCube:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case resource::TextureDimension::TextureCubeArray:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    default:
        return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }
}

/// @brief Translates an integer MSAA sample count into its `VkSampleCountFlagBits` enumerator.
/// @param[in] numSamples Power-of-two sample count (`1`, `2`, `4`, `8`, `16`, `32`, or `64`).
/// @return Matching `VkSampleCountFlagBits`, or `VK_SAMPLE_COUNT_1_BIT` for unsupported counts.
[[nodiscard]] inline constexpr VkSampleCountFlagBits
    sampleCountToVkSampleCountFlagBits(const gp::UInt32 numSamples) noexcept
{
    switch (numSamples)
    {
    case 1u:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2u:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4u:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8u:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16u:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32u:
        return VK_SAMPLE_COUNT_32_BIT;
    case 64u:
        return VK_SAMPLE_COUNT_64_BIT;
    default:
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

/// @brief Translates an index buffer element stride in bytes into its `VkIndexType`.
/// @param[in] strideInBytes Byte size of a single index (`1`, `2`, or `4`).
/// @return `VK_INDEX_TYPE_UINT8_KHR`, `VK_INDEX_TYPE_UINT16`, or `VK_INDEX_TYPE_UINT32`, or `VK_INDEX_TYPE_MAX_ENUM`
///         for unsupported strides.
[[nodiscard]] inline constexpr VkIndexType indexStrideToVkIndexType(const gp::UInt32 strideInBytes) noexcept
{
    switch (strideInBytes)
    {
    case 1u:
        return VK_INDEX_TYPE_UINT8_KHR;
    case 2u:
        return VK_INDEX_TYPE_UINT16;
    case 4u:
        return VK_INDEX_TYPE_UINT32;
    default:
        return VK_INDEX_TYPE_MAX_ENUM;
    }
}

/// @brief Translates an RHI `RenderQueryType` into its `VkQueryType`.
/// @param[in] queryType Engine GPU render query type.
/// @return `VK_QUERY_TYPE_OCCLUSION` or `VK_QUERY_TYPE_TIMESTAMP`, or `VK_QUERY_TYPE_MAX_ENUM` for `Undefined` or
///         out-of-range values.
[[nodiscard]] inline constexpr VkQueryType renderQueryTypeToVkQueryType(const sync::RenderQueryType queryType) noexcept
{
    switch (queryType)
    {
    case sync::RenderQueryType::Occlusion:
        return VK_QUERY_TYPE_OCCLUSION;
    case sync::RenderQueryType::AbsoluteTime:
        return VK_QUERY_TYPE_TIMESTAMP;
    case sync::RenderQueryType::Undefined:
    default:
        return VK_QUERY_TYPE_MAX_ENUM;
    }
}

/// @brief Merges separate depth and stencil image layouts into a single combined `VkImageLayout`.
/// @details Used when `VK_KHR_separate_depth_stencil_layouts` is unavailable or disabled on the platform.
/// @param[in] depthLayout Layout desired for the depth aspect.
/// @param[in] stencilLayout Layout desired for the stencil aspect.
/// @return Combined `VkImageLayout` compatible with non-separate depth-stencil barriers and render passes.
[[nodiscard]] inline constexpr VkImageLayout
    getMergedDepthStencilLayout(const VkImageLayout depthLayout, const VkImageLayout stencilLayout) noexcept
{
    if (depthLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || stencilLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    if (depthLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || stencilLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    if (depthLayout == VK_IMAGE_LAYOUT_UNDEFINED && stencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    if (depthLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
        stencilLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    const auto isMergedLayout = [](const VkImageLayout layout) noexcept -> bool
    {
        return layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
               layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
               layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ||
               layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
    };

    if (isMergedLayout(depthLayout))
    {
        return depthLayout;
    }
    if (isMergedLayout(stencilLayout))
    {
        return stencilLayout;
    }

    if (depthLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
    {
        if (stencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL || stencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
    }

    if (depthLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
    {
        if (stencilLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL || stencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
    }

    return (stencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                                                 : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
}

/// @brief Derives the conservative `VkAccessFlags` mask associated with an internal `VkImageLayout` transition.
/// @param[in] layout Vulkan image layout.
/// @param[in] isDepthOrStencil True when the image uses a depth or depth-stencil format.
/// @return Matching `VkAccessFlags` bitmask (`0u` for layouts that require no explicit cache access flags).
[[nodiscard]] inline constexpr VkAccessFlags
    getVkAccessMaskForLayout(const VkImageLayout layout, const bool isDepthOrStencil) noexcept
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ:
        return isDepthOrStencil ? (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
                                : (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                   VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return 0u;

    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
        return VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;

    case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
        return VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;

    case VK_IMAGE_LAYOUT_GENERAL:
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return 0u;

    case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;

    default:
        return 0u;
    }
}

/// @brief Derives the `VkPipelineStageFlags` mask associated with an internal `VkImageLayout` transition.
/// @param[in] layout Vulkan image layout.
/// @param[in] isDepthOrStencil True when the image uses a depth or depth-stencil format.
/// @return Matching `VkPipelineStageFlags` bitmask (`0u` for unrecognized layouts).
[[nodiscard]] inline constexpr VkPipelineStageFlags
    getVkStageFlagsForLayout(const VkImageLayout layout, const bool isDepthOrStencil) noexcept
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ:
        return isDepthOrStencil
                   ? (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
                   : (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
        return VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;

    case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

    case VK_IMAGE_LAYOUT_GENERAL:
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    default:
        return 0u;
    }
}

/// @brief Translates a `sync::ResourceAccess` bitmask into Vulkan pipeline stages, memory access flags, and layout.
/// @details Handles exclusive single states (`Discard`, `Unknown`, `CPURead`, `Present`, `RTV`, `ResolveDst`,
///          `CopyDest`, `BVHRead`, `BVHWrite`, `DSVWrite`) and combinable read/UAV states. Any unrecognized access
///          bits are reported in `VulkanBarrierState::unprocessedFlags`.
/// @param[in] access Resource access state mask to translate.
/// @param[in] context Device capability and resource descriptor context.
/// @return Populated `VulkanBarrierState` containing `stageFlags`, `accessFlags`, `layout`, and `unprocessedFlags`.
[[nodiscard]] inline constexpr VulkanBarrierState
    translateResourceAccessToVulkan(sync::ResourceAccess access, const VulkanAccessTranslationContext& context) noexcept
{
    VulkanBarrierState result{};

    // Multisample resolve targets written during a render pass behave as color attachments in Vulkan.
    const sync::ResourceAccess resolveAttachmentAccess = sync::ResourceAccess::RTV | sync::ResourceAccess::ResolveDst;
    if (access == resolveAttachmentAccess)
    {
        access = sync::ResourceAccess::RTV;
    }

    // BVHRead combined with SRV flags is handled as acceleration-structure read access.
    if ((access & sync::ResourceAccess::BVHRead) != sync::ResourceAccess::None)
    {
        access &= ~sync::ResourceAccess::SRVMask;
    }

    const VkImageLayout srvLayout =
        context.isDepthStencil
            ? VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
            : (context.supportsReadOnlyOptimal ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL);

    switch (access)
    {
    case sync::ResourceAccess::Discard:
        result.stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        result.accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        result.layout = context.isSourceState ? VK_IMAGE_LAYOUT_UNDEFINED : srvLayout;
        return result;

    case sync::ResourceAccess::Unknown:
        result.stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        result.accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        return result;

    case sync::ResourceAccess::CPURead:
        result.stageFlags = VK_PIPELINE_STAGE_HOST_BIT;
        result.accessFlags = VK_ACCESS_HOST_READ_BIT;
        result.layout = VK_IMAGE_LAYOUT_GENERAL;
        return result;

    case sync::ResourceAccess::Present:
        result.stageFlags = context.isSourceState ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                                  : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        result.accessFlags = 0u;
        result.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        return result;

    case sync::ResourceAccess::ResolveDst:
    case sync::ResourceAccess::RTV:
        result.stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        result.accessFlags = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (context.supportsLocalRead)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            result.accessFlags |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            result.layout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ;
        }
        else
        {
            result.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        return result;

    case sync::ResourceAccess::CopyDest:
        result.stageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        result.accessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        return result;

    case sync::ResourceAccess::BVHRead:
        result.stageFlags =
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (context.supportsRayTracingShaders)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
        result.accessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        return result;

    case sync::ResourceAccess::BVHWrite:
        result.stageFlags =
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if (context.supportsRayTracingShaders)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
        result.accessFlags = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        return result;

    default:
        break;
    }

    if ((access & sync::ResourceAccess::DSVWrite) != sync::ResourceAccess::None)
    {
        result.stageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        return result;
    }

    gp::UInt32 processedFlags = 0u;

    if ((access & sync::ResourceAccess::IndirectArgs) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        result.accessFlags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::IndirectArgs);
    }

    if ((access & sync::ResourceAccess::VertexOrIndexBuffer) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        if (context.resourceType == sync::TransitionResourceType::Buffer)
        {
            if ((context.bufferUsageFlags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0u)
            {
                result.accessFlags |= VK_ACCESS_INDEX_READ_BIT;
            }
            if ((context.bufferUsageFlags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0u)
            {
                result.accessFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            }
        }
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::VertexOrIndexBuffer);
    }

    if ((access & sync::ResourceAccess::DSVRead) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        result.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::DSVRead);
    }

    if ((access & sync::ResourceAccess::SRVGraphicsNonPixel) != sync::ResourceAccess::None)
    {
        result.stageFlags |=
            (context.deviceGraphicsPipelineStageBits &
             ~static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        if ((context.bufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u)
        {
            result.accessFlags |= VK_ACCESS_UNIFORM_READ_BIT;
        }
        if ((context.bufferUsageFlags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0u)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            result.accessFlags |= VK_ACCESS_INDEX_READ_BIT;
        }
        if ((context.bufferUsageFlags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0u)
        {
            result.stageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            result.accessFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        }
        result.layout = srvLayout;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::SRVGraphicsNonPixel);
    }

    if ((access & sync::ResourceAccess::SRVGraphicsPixel) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        if ((context.bufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u)
        {
            result.accessFlags |= VK_ACCESS_UNIFORM_READ_BIT;
        }
        result.layout = srvLayout;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::SRVGraphicsPixel);
    }

    if ((access & sync::ResourceAccess::SRVCompute) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        result.layout = srvLayout;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::SRVCompute);
    }

    if ((access & sync::ResourceAccess::UAVGraphics) != sync::ResourceAccess::None)
    {
        result.stageFlags |= context.deviceGraphicsPipelineStageBits;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_GENERAL;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::UAVGraphics);
    }

    if ((access & sync::ResourceAccess::UAVCompute) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        result.layout = VK_IMAGE_LAYOUT_GENERAL;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::UAVCompute);
    }

    if ((access & (sync::ResourceAccess::CopySrc | sync::ResourceAccess::ResolveSrc)) != sync::ResourceAccess::None)
    {
        if (context.resourceType == sync::TransitionResourceType::Texture)
        {
            if (result.layout == VK_IMAGE_LAYOUT_UNDEFINED)
            {
                result.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                result.stageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
                result.accessFlags = VK_ACCESS_TRANSFER_READ_BIT;
            }
            else
            {
                result.layout = VK_IMAGE_LAYOUT_GENERAL;
                result.stageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                result.accessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
            }
        }
        else
        {
            // Preserve earlier combined read-only flags on buffers (for example SRVCompute | CopySrc).
            result.stageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            result.accessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
        }
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::CopySrc | sync::ResourceAccess::ResolveSrc);
    }

    if ((access & sync::ResourceAccess::ShadingRateSource) != sync::ResourceAccess::None)
    {
        if (context.vrsImageDataType == pipeline::VRSImageDataType::Palette)
        {
            result.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
            result.accessFlags = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
            result.layout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
            processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::ShadingRateSource);
        }
        else if (context.vrsImageDataType == pipeline::VRSImageDataType::Fractional)
        {
            result.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
            result.accessFlags = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
            result.layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
            processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::ShadingRateSource);
        }
    }

    result.unprocessedFlags = static_cast<gp::UInt32>(access) & (~processedFlags);
    return result;
}

/// @brief Translates a `sync::SubresourceRange` plane slice selector into depth-stencil `VkImageAspectFlags`.
/// @param[in] planeSlice Plane slice index (`kAllSubresources`, `kDepthPlaneSlice`, or `kStencilPlaneSlice`).
/// @return Matching `VkImageAspectFlags` (`0u` for unrecognized plane slice indices).
[[nodiscard]] inline constexpr VkImageAspectFlags getDepthStencilAspectMask(const gp::UInt16 planeSlice) noexcept
{
    if (planeSlice == sync::SubresourceRange::kAllSubresources)
    {
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (planeSlice == sync::SubresourceRange::kDepthPlaneSlice)
    {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (planeSlice == sync::SubresourceRange::kStencilPlaneSlice)
    {
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    return 0u;
}

/// @brief Returns the `synchronization2` `VkImageLayout` for an individual depth or stencil plane transition.
/// @param[in] access Resource access mask for the target depth or stencil plane.
/// @return `VK_IMAGE_LAYOUT_UNDEFINED`, `TRANSFER_SRC_OPTIMAL`, `TRANSFER_DST_OPTIMAL`, `ATTACHMENT_OPTIMAL`, or
///         `READ_ONLY_OPTIMAL`.
[[nodiscard]] inline constexpr VkImageLayout getDepthOrStencilLayout(const sync::ResourceAccess access) noexcept
{
    if (access == sync::ResourceAccess::Unknown || access == sync::ResourceAccess::Discard)
    {
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    if ((access & sync::ResourceAccess::CopySrc) != sync::ResourceAccess::None)
    {
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    if ((access & sync::ResourceAccess::CopyDest) != sync::ResourceAccess::None)
    {
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    if ((access & sync::ResourceAccess::DSVWrite) != sync::ResourceAccess::None)
    {
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    }

    return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
}

/// @brief Translates a `sync::ResourceAccess` mask for an individual depth or stencil plane barrier.
/// @details Resolves pipeline stage flags, memory access flags, and the `synchronization2` depth/stencil image layout.
///          Any unrecognized access bits are returned in `VulkanBarrierState::unprocessedFlags`.
/// @param[in] access Resource access mask for the target depth or stencil plane.
/// @return Populated `VulkanBarrierState` for the depth or stencil plane.
[[nodiscard]] inline constexpr VulkanBarrierState
    translateDepthOrStencilAccessToVulkan(const sync::ResourceAccess access) noexcept
{
    VulkanBarrierState result{};
    result.layout = getDepthOrStencilLayout(access);

    if (access == sync::ResourceAccess::Unknown || access == sync::ResourceAccess::Discard)
    {
        result.stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        result.accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        return result;
    }

    gp::UInt32 processedFlags = 0u;

    if ((access & sync::ResourceAccess::ResolveDst) == sync::ResourceAccess::ResolveDst)
    {
        // Depth-stencil resolve writes execute during the color attachment output stage in Vulkan.
        result.stageFlags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        result.accessFlags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::ResolveDst);
    }

    if ((access & sync::ResourceAccess::DSVWrite) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags |=
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::DSVWrite);
    }

    if ((access & sync::ResourceAccess::DSVRead) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        result.accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::DSVRead);
    }

    if ((access & sync::ResourceAccess::SRVGraphics) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::SRVGraphics);
    }

    if ((access & sync::ResourceAccess::UAVGraphics) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::UAVGraphics);
    }

    if ((access & sync::ResourceAccess::SRVCompute) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::SRVCompute);
    }

    if ((access & sync::ResourceAccess::UAVCompute) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        result.accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::UAVCompute);
    }

    if ((access & sync::ResourceAccess::CopySrc) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        result.accessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::CopySrc);
    }

    if ((access & sync::ResourceAccess::CopyDest) != sync::ResourceAccess::None)
    {
        result.stageFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        result.accessFlags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        processedFlags |= static_cast<gp::UInt32>(sync::ResourceAccess::CopyDest);
    }

    result.unprocessedFlags = static_cast<gp::UInt32>(access) & (~processedFlags);
    return result;
}

}   // namespace gp::rhi::vulkan
