// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"

namespace gp::rhi::view
{

/// @brief Promotes a `Texture2D` view dimension to `Texture2DArray` when narrowing a slice of a cube or array texture.
/// @details Several graphics APIs cannot create a plain 2D view targeting an arbitrary slice inside a 2D array or
///          cubemap, while still allowing a single-slice 2D array view to bind as a `Texture2D` in shaders.
/// @param[in] viewInfo Resolved texture view parameters.
/// @param[in] textureDesc Underlying texture descriptor.
/// @return Adjusted `TextureViewDimension` suitable for backend view creation.
[[nodiscard]] GP_RHI_API TextureViewDimension adjustViewInfoDimensionForNarrowing(
    const ViewDesc::Texture::ViewInfo& viewInfo, const resource::TextureDesc& textureDesc
) noexcept;

}   // namespace gp::rhi::view
