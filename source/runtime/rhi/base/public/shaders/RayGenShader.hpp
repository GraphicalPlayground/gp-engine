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

/// @brief Compiled ray generation shader stage resource.
class GP_RHI_API RayGenShader : public RayTracingShader
{
public:
    /// @brief Compile-time pipeline stage for `RayGenShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::RayGen;

    /// @brief Constructs a ray generation shader resource.
    RayGenShader() noexcept;

    RayGenShader(const RayGenShader&) = delete;
    RayGenShader& operator=(const RayGenShader&) = delete;
    RayGenShader(RayGenShader&&) = delete;
    RayGenShader& operator=(RayGenShader&&) = delete;

protected:
    /// @copydoc RayTracingShader::~RayTracingShader()
    virtual ~RayGenShader() noexcept override;
};

}   // namespace gp::rhi::shader
