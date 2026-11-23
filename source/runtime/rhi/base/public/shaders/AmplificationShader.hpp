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

/// @brief Compiled amplification (task) shader stage resource.
class GP_RHI_API AmplificationShader : public GraphicsShader
{
public:
    /// @brief Compile-time pipeline stage for `AmplificationShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::Amplification;

    /// @brief Constructs an amplification shader resource.
    AmplificationShader() noexcept;

    AmplificationShader(const AmplificationShader&) = delete;
    AmplificationShader& operator=(const AmplificationShader&) = delete;
    AmplificationShader(AmplificationShader&&) = delete;
    AmplificationShader& operator=(AmplificationShader&&) = delete;

protected:
    /// @copydoc GraphicsShader::~GraphicsShader()
    virtual ~AmplificationShader() noexcept override;
};

}   // namespace gp::rhi::shader
