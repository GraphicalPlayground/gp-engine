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

/// @brief Compiled geometry shader stage resource.
class GP_RHI_API GeometryShader : public GraphicsShader
{
public:
    /// @brief Compile-time pipeline stage for `GeometryShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::Geometry;

    /// @brief Constructs a geometry shader resource.
    GeometryShader() noexcept;

    GeometryShader(const GeometryShader&) = delete;
    GeometryShader& operator=(const GeometryShader&) = delete;
    GeometryShader(GeometryShader&&) = delete;
    GeometryShader& operator=(GeometryShader&&) = delete;

protected:
    /// @copydoc GraphicsShader::~GraphicsShader()
    virtual ~GeometryShader() noexcept override;
};

}   // namespace gp::rhi::shader
