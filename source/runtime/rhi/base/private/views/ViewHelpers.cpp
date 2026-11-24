// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/ViewHelpers.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "resources/TextureDesc.hpp"

namespace gp::rhi::view
{

TextureViewDimension adjustViewInfoDimensionForNarrowing(
    const ViewDesc::Texture::ViewInfo& viewInfo, const resource::TextureDesc& textureDesc
) noexcept
{
    TextureViewDimension viewInfoDimension = viewInfo.dimension;

    // Backends that cannot address a specific slice through a plain 2D view support binding a single-slice 2D array
    // view in its place.
    if (viewInfoDimension == TextureViewDimension::Texture2D &&
        (textureDesc.isTextureCube() || textureDesc.isTextureArray()))
    {
        viewInfoDimension = TextureViewDimension::Texture2DArray;
        GP_ENSURE_MSG(
            viewInfo.arrayRange.num == 1u || textureDesc.arraySize == 1u,
            "Trying to create a 2D texture view with more than 1 element."
        );
    }

    return viewInfoDimension;
}

}   // namespace gp::rhi::view
