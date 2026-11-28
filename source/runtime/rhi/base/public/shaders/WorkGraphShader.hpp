// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "RHIBase.hpp"
#include "shaders/Shader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::shader
{

/// @brief Shared base class for GPU work graph shader stages.
class GP_RHI_API WorkGraphShader : public Shader
{
public:
    WorkGraphShader() = delete;

    /// @brief Constructs a work graph shader tagged with `frequency`.
    /// @param[in] frequency Work graph `ShaderFrequency` stage (`WorkGraphRoot` or `WorkGraphComputeNode`).
    explicit WorkGraphShader(ShaderFrequency frequency) noexcept;

    WorkGraphShader(const WorkGraphShader&) = delete;
    WorkGraphShader& operator=(const WorkGraphShader&) = delete;
    WorkGraphShader(WorkGraphShader&&) = delete;
    WorkGraphShader& operator=(WorkGraphShader&&) = delete;

protected:
    /// @copydoc Shader::~Shader()
    virtual ~WorkGraphShader() noexcept override;
};

}   // namespace gp::rhi::shader
