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

/// @brief Compiled mesh shader stage resource.
class GP_RHI_API MeshShader : public GraphicsShader
{
public:
    /// @brief Compile-time pipeline stage for `MeshShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::Mesh;

    /// @brief Constructs a mesh shader resource.
    MeshShader() noexcept;

    MeshShader(const MeshShader&) = delete;
    MeshShader& operator=(const MeshShader&) = delete;
    MeshShader(MeshShader&&) = delete;
    MeshShader& operator=(MeshShader&&) = delete;

protected:
    /// @copydoc GraphicsShader::~GraphicsShader()
    virtual ~MeshShader() noexcept override;
};

}   // namespace gp::rhi::shader
