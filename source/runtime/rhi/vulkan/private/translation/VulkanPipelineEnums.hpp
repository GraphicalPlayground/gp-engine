// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Translates an RHI `PrimitiveType` draw topology into its `VkPrimitiveTopology`.
/// @details `QuadList` and `RectList` have no direct Vulkan topology enumerator and return
///          `VK_PRIMITIVE_TOPOLOGY_MAX_ENUM`.
/// @param[in] primitiveType Draw-call primitive topology.
/// @return Matching `VkPrimitiveTopology`, or `VK_PRIMITIVE_TOPOLOGY_MAX_ENUM` when unsupported.
[[nodiscard]] inline constexpr VkPrimitiveTopology
    primitiveTypeToVkTopology(const pipeline::PrimitiveType primitiveType) noexcept
{
    switch (primitiveType)
    {
    case pipeline::PrimitiveType::PointList:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case pipeline::PrimitiveType::LineList:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case pipeline::PrimitiveType::TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case pipeline::PrimitiveType::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case pipeline::PrimitiveType::QuadList:
    case pipeline::PrimitiveType::RectList:
    default:
        return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }
}

/// @brief Translates a coarse `PrimitiveTopologyType` class into its default `VkPrimitiveTopology`.
/// @param[in] topologyType Pipeline topology class (`Triangle`, `Patch`, `Line`, or `Point`).
/// @return Matching `VkPrimitiveTopology`, or `VK_PRIMITIVE_TOPOLOGY_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkPrimitiveTopology
    primitiveTopologyTypeToVkTopology(const pipeline::PrimitiveTopologyType topologyType) noexcept
{
    switch (topologyType)
    {
    case pipeline::PrimitiveTopologyType::Triangle:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case pipeline::PrimitiveTopologyType::Patch:
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    case pipeline::PrimitiveTopologyType::Line:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case pipeline::PrimitiveTopologyType::Point:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    default:
        return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }
}

/// @brief Translates an RHI depth/stencil `CompareFunction` into its `VkCompareOp`.
/// @param[in] op Comparison function enumerator.
/// @return Matching `VkCompareOp`, or `VK_COMPARE_OP_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkCompareOp compareFunctionToVkCompareOp(const pipeline::CompareFunction op) noexcept
{
    switch (op)
    {
    case pipeline::CompareFunction::Less:
        return VK_COMPARE_OP_LESS;
    case pipeline::CompareFunction::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case pipeline::CompareFunction::Greater:
        return VK_COMPARE_OP_GREATER;
    case pipeline::CompareFunction::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case pipeline::CompareFunction::Equal:
        return VK_COMPARE_OP_EQUAL;
    case pipeline::CompareFunction::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case pipeline::CompareFunction::Never:
        return VK_COMPARE_OP_NEVER;
    case pipeline::CompareFunction::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        return VK_COMPARE_OP_MAX_ENUM;
    }
}

/// @brief Translates an RHI `StencilOp` into its `VkStencilOp`.
/// @param[in] op Stencil update operation.
/// @return Matching `VkStencilOp`, or `VK_STENCIL_OP_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkStencilOp stencilOpToVkStencilOp(const pipeline::StencilOp op) noexcept
{
    switch (op)
    {
    case pipeline::StencilOp::Keep:
        return VK_STENCIL_OP_KEEP;
    case pipeline::StencilOp::Zero:
        return VK_STENCIL_OP_ZERO;
    case pipeline::StencilOp::Replace:
        return VK_STENCIL_OP_REPLACE;
    case pipeline::StencilOp::SaturatedIncrement:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case pipeline::StencilOp::SaturatedDecrement:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case pipeline::StencilOp::Invert:
        return VK_STENCIL_OP_INVERT;
    case pipeline::StencilOp::Increment:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case pipeline::StencilOp::Decrement:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
        return VK_STENCIL_OP_MAX_ENUM;
    }
}

/// @brief Translates an RHI `BlendOperation` into its `VkBlendOp`.
/// @param[in] op Blend arithmetic operation.
/// @return Matching `VkBlendOp`, or `VK_BLEND_OP_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkBlendOp blendOperationToVkBlendOp(const pipeline::BlendOperation op) noexcept
{
    switch (op)
    {
    case pipeline::BlendOperation::Add:
        return VK_BLEND_OP_ADD;
    case pipeline::BlendOperation::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case pipeline::BlendOperation::Min:
        return VK_BLEND_OP_MIN;
    case pipeline::BlendOperation::Max:
        return VK_BLEND_OP_MAX;
    case pipeline::BlendOperation::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    default:
        return VK_BLEND_OP_MAX_ENUM;
    }
}

/// @brief Translates an RHI `BlendFactor` into its `VkBlendFactor`.
/// @param[in] factor Source or destination blend multiplier.
/// @return Matching `VkBlendFactor`, or `VK_BLEND_FACTOR_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkBlendFactor blendFactorToVkBlendFactor(const pipeline::BlendFactor factor) noexcept
{
    switch (factor)
    {
    case pipeline::BlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case pipeline::BlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case pipeline::BlendFactor::SourceColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case pipeline::BlendFactor::InverseSourceColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case pipeline::BlendFactor::SourceAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case pipeline::BlendFactor::InverseSourceAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case pipeline::BlendFactor::DestAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case pipeline::BlendFactor::InverseDestAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case pipeline::BlendFactor::DestColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case pipeline::BlendFactor::InverseDestColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case pipeline::BlendFactor::ConstantBlendFactor:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case pipeline::BlendFactor::InverseConstantBlendFactor:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case pipeline::BlendFactor::Source1Color:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case pipeline::BlendFactor::InverseSource1Color:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case pipeline::BlendFactor::Source1Alpha:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case pipeline::BlendFactor::InverseSource1Alpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    default:
        return VK_BLEND_FACTOR_MAX_ENUM;
    }
}

/// @brief Translates an RHI `ColorWriteMask` bitmask into `VkColorComponentFlags`.
/// @param[in] mask Channel write enable mask.
/// @return Combination of `VK_COLOR_COMPONENT_{R,G,B,A}_BIT` flags.
[[nodiscard]] inline constexpr VkColorComponentFlags
    colorWriteMaskToVkColorComponentFlags(const pipeline::ColorWriteMask mask) noexcept
{
    VkColorComponentFlags flags = 0u;
    if ((mask & pipeline::ColorWriteMask::Red) != pipeline::ColorWriteMask::None)
    {
        flags |= VK_COLOR_COMPONENT_R_BIT;
    }
    if ((mask & pipeline::ColorWriteMask::Green) != pipeline::ColorWriteMask::None)
    {
        flags |= VK_COLOR_COMPONENT_G_BIT;
    }
    if ((mask & pipeline::ColorWriteMask::Blue) != pipeline::ColorWriteMask::None)
    {
        flags |= VK_COLOR_COMPONENT_B_BIT;
    }
    if ((mask & pipeline::ColorWriteMask::Alpha) != pipeline::ColorWriteMask::None)
    {
        flags |= VK_COLOR_COMPONENT_A_BIT;
    }
    return flags;
}

/// @brief Translates an RHI `RasterizerFillMode` into its `VkPolygonMode`.
/// @param[in] fillMode Polygon fill mode (`Point`, `Wireframe`, or `Solid`).
/// @return Matching `VkPolygonMode`, or `VK_POLYGON_MODE_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkPolygonMode
    rasterizerFillModeToVkPolygonMode(const pipeline::RasterizerFillMode fillMode) noexcept
{
    switch (fillMode)
    {
    case pipeline::RasterizerFillMode::Point:
        return VK_POLYGON_MODE_POINT;
    case pipeline::RasterizerFillMode::Wireframe:
        return VK_POLYGON_MODE_LINE;
    case pipeline::RasterizerFillMode::Solid:
        return VK_POLYGON_MODE_FILL;
    default:
        return VK_POLYGON_MODE_MAX_ENUM;
    }
}

/// @brief Translates an RHI `RasterizerCullMode` into `VkCullModeFlags`.
/// @details Assumes `VK_FRONT_FACE_CLOCKWISE` so `CW` culls front faces and `CCW` culls back faces.
/// @param[in] cullMode Face culling mode (`None`, `CW`, or `CCW`).
/// @return Matching `VkCullModeFlags` (`VK_CULL_MODE_NONE` for unrecognized values).
[[nodiscard]] inline constexpr VkCullModeFlags
    rasterizerCullModeToVkCullMode(const pipeline::RasterizerCullMode cullMode) noexcept
{
    switch (cullMode)
    {
    case pipeline::RasterizerCullMode::None:
        return VK_CULL_MODE_NONE;
    case pipeline::RasterizerCullMode::CW:
        return VK_CULL_MODE_FRONT_BIT;
    case pipeline::RasterizerCullMode::CCW:
        return VK_CULL_MODE_BACK_BIT;
    default:
        return VK_CULL_MODE_NONE;
    }
}

/// @brief Translates an RHI `VRSRateCombiner` into `VkFragmentShadingRateCombinerOpKHR`.
/// @details Vulkan has no sum combiner operator, so `VRSRateCombiner::Sum` falls back to
///          `VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR`.
/// @param[in] combiner Variable Rate Shading combiner mode.
/// @return Matching `VkFragmentShadingRateCombinerOpKHR`, or `VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR` if
///         unrecognized.
[[nodiscard]] inline constexpr VkFragmentShadingRateCombinerOpKHR
    vrsRateCombinerToVkCombinerOp(const pipeline::VRSRateCombiner combiner) noexcept
{
    switch (combiner)
    {
    case pipeline::VRSRateCombiner::Passthrough:
        return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
    case pipeline::VRSRateCombiner::Override:
        return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR;
    case pipeline::VRSRateCombiner::Min:
        return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR;
    case pipeline::VRSRateCombiner::Max:
    case pipeline::VRSRateCombiner::Sum:
        return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;
    default:
        return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
    }
}

/// @brief Decodes a single-axis `VRSAxisShadingRate` enumerator into its pixel step (`1`, `2`, or `4`).
/// @param[in] rate Per-axis shading rate enumerator.
/// @return Pixel step along that axis (`1u`, `2u`, or `4u`).
[[nodiscard]] inline constexpr gp::UInt32
    vrsAxisShadingRateToExtentDimension(const pipeline::VRSAxisShadingRate rate) noexcept
{
    switch (rate)
    {
    case pipeline::VRSAxisShadingRate::Rate2X:
        return 2u;
    case pipeline::VRSAxisShadingRate::Rate4X:
        return 4u;
    case pipeline::VRSAxisShadingRate::Rate1X:
    default:
        return 1u;
    }
}

/// @brief Decodes a combined `VRSShadingRate` into a Vulkan `VkExtent2D` fragment size.
/// @param[in] rate Combined X/Y shading rate enumerator.
/// @return Fragment tile extent (`width` and `height` each in `{1, 2, 4}`).
[[nodiscard]] inline constexpr VkExtent2D vrsShadingRateToVkExtent2D(const pipeline::VRSShadingRate rate) noexcept
{
    const gp::UInt8 raw = static_cast<gp::UInt8>(rate);
    const pipeline::VRSAxisShadingRate rateX = static_cast<pipeline::VRSAxisShadingRate>((raw >> 2u) & 0x3u);
    const pipeline::VRSAxisShadingRate rateY = static_cast<pipeline::VRSAxisShadingRate>(raw & 0x3u);
    return VkExtent2D{ vrsAxisShadingRateToExtentDimension(rateX), vrsAxisShadingRateToExtentDimension(rateY) };
}

/// @brief Translates an RHI `RenderTargetLoadAction` into its `VkAttachmentLoadOp`.
/// @param[in] loadAction Attachment pass-entry load policy.
/// @return Matching `VkAttachmentLoadOp`, or `VK_ATTACHMENT_LOAD_OP_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkAttachmentLoadOp
    renderTargetLoadActionToVkLoadOp(const command::RenderTargetLoadAction loadAction) noexcept
{
    switch (loadAction)
    {
    case command::RenderTargetLoadAction::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case command::RenderTargetLoadAction::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case command::RenderTargetLoadAction::NoAction:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    default:
        return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
    }
}

/// @brief Translates an RHI `RenderTargetStoreAction` into its `VkAttachmentStoreOp`.
/// @details When `storeAction` is `MultisampleResolve`, memoryless tile-only targets use
///          `VK_ATTACHMENT_STORE_OP_DONT_CARE` while backed targets use `VK_ATTACHMENT_STORE_OP_STORE`.
/// @param[in] storeAction Attachment pass-exit store policy.
/// @param[in] isMemoryless True when the attachment exists only in tile memory (`TextureCreateFlags::Memoryless`).
/// @return Matching `VkAttachmentStoreOp`, or `VK_ATTACHMENT_STORE_OP_MAX_ENUM` if out of range.
[[nodiscard]] inline constexpr VkAttachmentStoreOp renderTargetStoreActionToVkStoreOp(
    const command::RenderTargetStoreAction storeAction, const bool isMemoryless = false
) noexcept
{
    switch (storeAction)
    {
    case command::RenderTargetStoreAction::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case command::RenderTargetStoreAction::NoAction:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case command::RenderTargetStoreAction::MultisampleResolve:
        return isMemoryless ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    default:
        return VK_ATTACHMENT_STORE_OP_MAX_ENUM;
    }
}

}   // namespace gp::rhi::vulkan
