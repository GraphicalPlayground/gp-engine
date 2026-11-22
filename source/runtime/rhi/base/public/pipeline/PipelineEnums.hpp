// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::pipeline
{

/// @brief Texture sampling filter mode.
enum class SamplerFilter : gp::UInt8
{
    Point = 0,
    Bilinear,
    Trilinear,
    AnisotropicPoint,
    AnisotropicLinear
};

/// @brief Texture coordinate wrapping behavior outside `[0, 1]`.
enum class SamplerAddressMode : gp::UInt8
{
    Wrap = 0,
    Clamp,
    Mirror,
    /// @brief Not supported on all backends.
    Border
};

/// @brief Depth comparison mode for shadow/comparison samplers.
enum class SamplerCompareFunction : gp::UInt8
{
    Never = 0,
    Less
};

/// @brief Polygon rasterization fill mode.
enum class RasterizerFillMode : gp::UInt8
{
    Point = 0,
    Wireframe,
    Solid
};

/// @brief Triangle face culling mode.
enum class RasterizerCullMode : gp::UInt8
{
    None = 0,
    CW,
    CCW
};

/// @brief Depth clipping vs. clamping behavior outside the near/far planes.
enum class RasterizerDepthClipMode : gp::UInt8
{
    DepthClip = 0,
    DepthClamp
};

/// @brief Per-channel color write enable mask for blend targets.
enum class ColorWriteMask : gp::UInt8
{
    None = 0x00u,
    Red = 0x01u,
    Green = 0x02u,
    Blue = 0x04u,
    Alpha = 0x08u,
    RGB = 0x01u | 0x02u | 0x04u,
    RGBA = 0x01u | 0x02u | 0x04u | 0x08u,
    RG = 0x01u | 0x02u,
    BA = 0x04u | 0x08u
};

/// @brief Comparison operator used by depth and stencil tests.
enum class CompareFunction : gp::UInt8
{
    Less = 0,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Never,
    Always,

    /// @brief Reversed-Z depth aliases.
    DepthNearOrEqual = GreaterEqual,
    DepthNear = Greater,
    DepthFartherOrEqual = LessEqual,
    DepthFarther = Less
};

/// @brief Predefined stencil bitmask presets.
enum class StencilMask : gp::UInt8
{
    Default = 0,
    Mask255,
    Mask1,
    Mask2,
    Mask4,
    Mask8,
    Mask16,
    Mask32,
    Mask64,
    Mask128
};

/// @brief Operation applied to the stencil buffer on test pass or fail.
enum class StencilOp : gp::UInt8
{
    Keep = 0,
    Zero,
    Replace,
    SaturatedIncrement,
    SaturatedDecrement,
    Invert,
    Increment,
    Decrement
};

/// @brief Arithmetic operation combining source and destination blend terms.
enum class BlendOperation : gp::UInt8
{
    Add = 0,
    Subtract,
    Min,
    Max,
    ReverseSubtract
};

/// @brief Multiplier applied to source or destination channels during blending.
enum class BlendFactor : gp::UInt8
{
    Zero = 0,
    One,
    SourceColor,
    InverseSourceColor,
    SourceAlpha,
    InverseSourceAlpha,
    DestAlpha,
    InverseDestAlpha,
    DestColor,
    InverseDestColor,
    ConstantBlendFactor,
    InverseConstantBlendFactor,
    Source1Color,
    InverseSource1Color,
    Source1Alpha,
    InverseSource1Alpha
};

/// @brief Component format of a vertex input attribute.
enum class VertexElementType : gp::UInt8
{
    None = 0,
    Float1,
    Float2,
    Float3,
    Float4,
    PackedNormal,
    UByte4,
    UByte4N,
    Color,
    Short2,
    Short4,
    /// @brief Signed 16-bit normalized pair `(value / 32767.0, value / 32767.0, 0, 1)`.
    Short2N,
    Half2,
    Half4,
    Short4N,
    UShort2,
    UShort4,
    /// @brief Unsigned 16-bit normalized pair `(value / 65535.0, value / 65535.0, 0, 1)`.
    UShort2N,
    UShort4N,
    /// @brief 10-bit RGB and 2-bit alpha normalized unsigned integer.
    URGB10A2N,
    UInt,
    UInt2
};

/// @brief Coarse geometry topology class specified on a graphics pipeline state.
enum class PrimitiveTopologyType : gp::UInt8
{
    Triangle = 0,
    Patch,
    Line,
    Point
};

/// @brief Exact primitive topology passed to draw commands.
enum class PrimitiveType : gp::UInt8
{
    TriangleList = 0,
    TriangleStrip,
    LineList,
    /// @brief Supported only when the backend reports quad topology support.
    QuadList,
    PointList,
    /// @brief Three-vertex screen-aligned rectangle (upper-left, upper-right, lower-left); backend-dependent.
    RectList
};

/// @brief Per-axis pixel coarseness for Variable Rate Shading.
enum class VRSAxisShadingRate : gp::UInt8
{
    Rate1X = 0x0u,
    Rate2X = 0x1u,
    Rate4X = 0x2u
};

/// @brief Combined X/Y shading rate tile dimensions for Variable Rate Shading.
enum class VRSShadingRate : gp::UInt8
{
    Rate1x1 = (0x0u << 2) | 0x0u,
    Rate1x2 = (0x0u << 2) | 0x1u,
    Rate2x1 = (0x1u << 2) | 0x0u,
    Rate2x2 = (0x1u << 2) | 0x1u,
    Rate2x4 = (0x1u << 2) | 0x2u,
    Rate4x2 = (0x2u << 2) | 0x1u,
    Rate4x4 = (0x2u << 2) | 0x2u
};

/// @brief Combiner applied between draw-level, primitive-level, and image-level VRS rates.
enum class VRSRateCombiner : gp::UInt8
{
    Passthrough = 0,
    Override,
    Min,
    Max,
    Sum
};

/// @brief Encoding format of a screen-space VRS rate attachment.
enum class VRSImageDataType : gp::UInt8
{
    NotSupported = 0,
    Palette,
    Fractional
};

}   // namespace gp::rhi::pipeline

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::pipeline::ColorWriteMask);
