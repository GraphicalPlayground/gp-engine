// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "commands/CommandTypes.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "templates/Function.hpp"

namespace gp::rhi::command
{

/// @brief Parameters for a single MSAA source-to-destination texture resolve extracted from `RenderPassInfo`.
struct ResolveTextureInfo
{
    /// @brief Multisampled source texture to resolve from.
    resource::Texture* sourceTexture{ nullptr };

    /// @brief Single-sampled destination texture receiving the resolved output.
    resource::Texture* destTexture{ nullptr };

    /// @brief Target mip level index to resolve.
    gp::UInt8 mipLevel{ 0u };

    /// @brief Target array slice index (`-1` selects all slices).
    gp::Int32 arraySlice{ -1 };

    /// @brief Optional sub-rectangle limiting the resolved texel area.
    ResolveRect resolveRect{};

    /// @brief Constructs an empty resolve descriptor.
    constexpr ResolveTextureInfo() noexcept = default;

    /// @brief Constructs a resolve descriptor from explicit source, destination, subresource, and resolve region.
    /// @param[in] inSourceTexture Multisampled source texture.
    /// @param[in] inDestTexture Single-sampled destination texture.
    /// @param[in] inMipLevel Target mip level.
    /// @param[in] inArraySlice Target array slice (`-1` for all slices).
    /// @param[in] inResolveRect Optional resolve sub-rectangle.
    constexpr ResolveTextureInfo(
        resource::Texture* const inSourceTexture,
        resource::Texture* const inDestTexture,
        const gp::UInt8 inMipLevel,
        const gp::Int32 inArraySlice,
        const ResolveRect inResolveRect
    ) noexcept
        : sourceTexture(inSourceTexture)
        , destTexture(inDestTexture)
        , mipLevel(inMipLevel)
        , arraySlice(inArraySlice)
        , resolveRect(inResolveRect)
    {}

    /// @brief Compares two resolve descriptors for equality.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const ResolveTextureInfo& rhs) const noexcept = default;
};

/// @brief Iterates every color and depth-stencil attachment in `info` and invokes `resolveFunction` for each valid MSAA
///        resolve pair.
/// @details Skips attachments with null source, null destination, or matching source and destination pointers. Asserts
///          format equality, extent equality, multisample-to-single-sample pairing, 2D-only depth-stencil resolves, and
///          that neither texture is 3D.
/// @param[in] info Active render pass descriptor containing color, depth-stencil, and resolve attachments.
/// @param[in] resolveFunction Callback invoked once for each validated resolve target pair.
GP_RHI_API void resolveRenderPassTargets(
    const RenderPassInfo& info, gp::FunctionRef<void(ResolveTextureInfo)> resolveFunction
) noexcept;

}   // namespace gp::rhi::command
