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

/// @brief Compiled ray miss shader stage resource.
class GP_RHI_API RayMissShader : public RayTracingShader
{
public:
    /// @brief Compile-time pipeline stage for `RayMissShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::RayMiss;

    /// @brief Constructs a ray miss shader resource.
    RayMissShader() noexcept;

    RayMissShader(const RayMissShader&) = delete;
    RayMissShader& operator=(const RayMissShader&) = delete;
    RayMissShader(RayMissShader&&) = delete;
    RayMissShader& operator=(RayMissShader&&) = delete;

protected:
    /// @copydoc RayTracingShader::~RayTracingShader()
    virtual ~RayMissShader() noexcept override;
};

}   // namespace gp::rhi::shader
