// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "RHIBase.hpp"
#include "shaders/GraphicsShader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::shader
{

/// @brief Compiled pixel (fragment) shader stage resource.
class GP_RHI_API PixelShader : public GraphicsShader
{
public:
    /// @brief Compile-time pipeline stage for `PixelShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::Pixel;

    /// @brief Constructs a pixel shader resource.
    PixelShader() noexcept;

    PixelShader(const PixelShader&) = delete;
    PixelShader& operator=(const PixelShader&) = delete;
    PixelShader(PixelShader&&) = delete;
    PixelShader& operator=(PixelShader&&) = delete;

protected:
    /// @copydoc GraphicsShader::~GraphicsShader()
    virtual ~PixelShader() noexcept override;
};

}   // namespace gp::rhi::shader
