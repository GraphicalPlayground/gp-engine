// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "RHIBase.hpp"
#include "shaders/RayTracingShader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::shader
{

/// @brief Compiled ray callable shader stage resource.
class GP_RHI_API RayCallableShader : public RayTracingShader
{
public:
    /// @brief Compile-time pipeline stage for `RayCallableShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::RayCallable;

    /// @brief Constructs a ray callable shader resource.
    RayCallableShader() noexcept;

    RayCallableShader(const RayCallableShader&) = delete;
    RayCallableShader& operator=(const RayCallableShader&) = delete;
    RayCallableShader(RayCallableShader&&) = delete;
    RayCallableShader& operator=(RayCallableShader&&) = delete;

protected:
    /// @copydoc RayTracingShader::~RayTracingShader()
    virtual ~RayCallableShader() noexcept override;
};

}   // namespace gp::rhi::shader
