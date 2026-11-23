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

/// @brief Compiled ray hit group shader stage resource.
class GP_RHI_API RayHitGroupShader : public RayTracingShader
{
public:
    /// @brief Compile-time pipeline stage for `RayHitGroupShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::RayHitGroup;

    /// @brief Constructs a ray hit group shader resource.
    RayHitGroupShader() noexcept;

    RayHitGroupShader(const RayHitGroupShader&) = delete;
    RayHitGroupShader& operator=(const RayHitGroupShader&) = delete;
    RayHitGroupShader(RayHitGroupShader&&) = delete;
    RayHitGroupShader& operator=(RayHitGroupShader&&) = delete;

protected:
    /// @copydoc RayTracingShader::~RayTracingShader()
    virtual ~RayHitGroupShader() noexcept override;
};

}   // namespace gp::rhi::shader
