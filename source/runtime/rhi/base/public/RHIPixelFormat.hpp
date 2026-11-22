// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/color/PixelFormat.hpp"   // IWYU pragma: export
#include "templates/Enums.hpp"

namespace gp::rhi
{

using gp::math::getPixelFormatChannels;
using gp::math::getPixelFormatInfo;
using gp::math::isASTCBlockCompressed;
using gp::math::isBCBlockCompressed;
using gp::math::isBlockCompressed;
using gp::math::isDepthFormat;
using gp::math::isDepthOrStencilFormat;
using gp::math::isETC2BlockCompressed;
using gp::math::isFloatFormat;
using gp::math::isHDRFormat;
using gp::math::isIntegerFormat;
using gp::math::isNormalizedFormat;
using gp::math::isSignedFormat;
using gp::math::isStencilFormat;
using gp::math::isValidPixelFormat;
using gp::math::kPixelFormatCount;
using gp::math::kPixelFormatTable;
using gp::math::PixelFormat;
using gp::math::PixelFormatChannelFlags;
using gp::math::PixelFormatComponentType;
using gp::math::PixelFormatCompression;
using gp::math::pixelFormatFromString;
using gp::math::PixelFormatInfo;
using gp::math::requiresBlock4Alignment;
using gp::math::supportsSRGB;
using gp::math::toString;

/// @brief Runtime hardware usage capabilities supported by an active RHI device for a given `PixelFormat`.
/// @details Queried per `IDevice` rather than stored in a mutable global table so multiple devices and backend
///          swaps (e.g., `rhi/null` vs `rhi/vulkan`) remain isolated and thread-safe.
enum class PixelFormatCapabilities : gp::UInt32
{
    None = 0u,

    Texture1D = 1u << 0,
    Texture2D = 1u << 1,
    Texture3D = 1u << 2,
    TextureCube = 1u << 3,
    RenderTarget = 1u << 4,
    DepthStencil = 1u << 5,
    TextureMipmaps = 1u << 6,
    TextureLoad = 1u << 7,
    TextureSample = 1u << 8,
    TextureGather = 1u << 9,
    TextureAtomics = 1u << 10,
    TextureBlendable = 1u << 11,
    TextureStore = 1u << 12,
    TextureFilterable = 1u << 13,

    Buffer = 1u << 14,
    VertexBuffer = 1u << 15,
    IndexBuffer = 1u << 16,
    BufferLoad = 1u << 17,
    BufferStore = 1u << 18,
    BufferAtomics = 1u << 19,

    UAV = 1u << 20,
    TypedUAVLoad = 1u << 21,
    TypedUAVStore = 1u << 22,

    LossyCompressible = 1u << 23,

    AnyTexture = Texture1D | Texture2D | Texture3D | TextureCube,
    AllTextureFlags = AnyTexture | RenderTarget | DepthStencil | TextureMipmaps | TextureLoad | TextureSample |
                      TextureGather | TextureAtomics | TextureBlendable | TextureStore | TextureFilterable,
    AllBufferFlags = Buffer | VertexBuffer | IndexBuffer | BufferLoad | BufferStore | BufferAtomics,
    AllUAVFlags = UAV | TypedUAVLoad | TypedUAVStore,
    AllFlags = AllTextureFlags | AllBufferFlags | AllUAVFlags | LossyCompressible
};

}   // namespace gp::rhi

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::PixelFormatCapabilities);

namespace gp::rhi
{

/// @brief Checks whether `supported` contains every capability flag requested in `required`.
/// @param[in] supported Capability bitmask reported by the active RHI device.
/// @param[in] required Required capability bitmask to test.
/// @return True when all bits in `required` are set in `supported`.
[[nodiscard]] inline constexpr bool
    hasPixelFormatCapabilities(const PixelFormatCapabilities supported, const PixelFormatCapabilities required) noexcept
{
    return (supported & required) == required;
}

}   // namespace gp::rhi
