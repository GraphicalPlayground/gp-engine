// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "pipeline/PipelineEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Translates an RHI `SamplerFilter` into its `VkSamplerMipmapMode`.
/// @param[in] filter Texture sampling filter mode.
/// @return `VK_SAMPLER_MIPMAP_MODE_NEAREST` or `VK_SAMPLER_MIPMAP_MODE_LINEAR`, or `VK_SAMPLER_MIPMAP_MODE_MAX_ENUM` if
///         out of range.
[[nodiscard]] inline constexpr VkSamplerMipmapMode
    samplerFilterToVkMipmapMode(const pipeline::SamplerFilter filter) noexcept
{
    switch (filter)
    {
    case pipeline::SamplerFilter::Point:
    case pipeline::SamplerFilter::Bilinear:
    case pipeline::SamplerFilter::AnisotropicPoint:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case pipeline::SamplerFilter::Trilinear:
    case pipeline::SamplerFilter::AnisotropicLinear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}

/// @brief Translates an RHI `SamplerFilter` into its minification and magnification `VkFilter`.
/// @param[in] filter Texture sampling filter mode.
/// @return `VK_FILTER_NEAREST` or `VK_FILTER_LINEAR`, or `VK_FILTER_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkFilter samplerFilterToVkFilter(const pipeline::SamplerFilter filter) noexcept
{
    switch (filter)
    {
    case pipeline::SamplerFilter::Point:
        return VK_FILTER_NEAREST;
    case pipeline::SamplerFilter::Bilinear:
    case pipeline::SamplerFilter::Trilinear:
    case pipeline::SamplerFilter::AnisotropicPoint:
    case pipeline::SamplerFilter::AnisotropicLinear:
        return VK_FILTER_LINEAR;
    default:
        return VK_FILTER_MAX_ENUM;
    }
}

/// @brief Translates an RHI `SamplerAddressMode` into its `VkSamplerAddressMode`.
/// @param[in] addressMode Texture coordinate addressing mode.
/// @return Matching `VkSamplerAddressMode`, or `VK_SAMPLER_ADDRESS_MODE_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkSamplerAddressMode
    samplerAddressModeToVkAddressMode(const pipeline::SamplerAddressMode addressMode) noexcept
{
    switch (addressMode)
    {
    case pipeline::SamplerAddressMode::Wrap:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case pipeline::SamplerAddressMode::Clamp:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case pipeline::SamplerAddressMode::Mirror:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case pipeline::SamplerAddressMode::Border:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default:
        return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }
}

/// @brief Translates an RHI `SamplerCompareFunction` into its `VkCompareOp`.
/// @param[in] compareFunction Shadow or comparison sampler function (`Never` or `Less`).
/// @return Matching `VkCompareOp`, or `VK_COMPARE_OP_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkCompareOp
    samplerCompareFunctionToVkCompareOp(const pipeline::SamplerCompareFunction compareFunction) noexcept
{
    switch (compareFunction)
    {
    case pipeline::SamplerCompareFunction::Never:
        return VK_COMPARE_OP_NEVER;
    case pipeline::SamplerCompareFunction::Less:
        return VK_COMPARE_OP_LESS;
    default:
        return VK_COMPARE_OP_MAX_ENUM;
    }
}

}   // namespace gp::rhi::vulkan
