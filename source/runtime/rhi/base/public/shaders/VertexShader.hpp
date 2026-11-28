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

/// @brief Compiled vertex shader stage resource.
class GP_RHI_API VertexShader : public GraphicsShader
{
public:
    /// @brief Compile-time pipeline stage for `VertexShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::Vertex;

    /// @brief Constructs a vertex shader resource.
    VertexShader() noexcept;

    VertexShader(const VertexShader&) = delete;
    VertexShader& operator=(const VertexShader&) = delete;
    VertexShader(VertexShader&&) = delete;
    VertexShader& operator=(VertexShader&&) = delete;

protected:
    /// @copydoc GraphicsShader::~GraphicsShader()
    virtual ~VertexShader() noexcept override;
};

}   // namespace gp::rhi::shader
