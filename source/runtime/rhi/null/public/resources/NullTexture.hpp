// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "RHINull.hpp"

namespace gp::rhi::null
{

/// @brief Concrete no-op GPU texture resource allocated by `NullDynamicRHI`.
class GP_RHI_NULL_API NullTexture final : public resource::Texture
{
public:
    /// @brief Constructs a null GPU texture retaining `createDesc`.
    /// @param[in] createDesc Texture creation descriptor.
    explicit NullTexture(const resource::TextureCreateDesc& createDesc) noexcept
        : Texture(createDesc)
    {}

protected:
    /// @copydoc resource::Texture::~Texture()
    virtual ~NullTexture() noexcept override = default;
};

}   // namespace gp::rhi::null
