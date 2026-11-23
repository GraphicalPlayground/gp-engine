// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceEnums.hpp"
#include "RHIBase.hpp"
#include "shaders/Shader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::shader
{

/// @brief Shared base class for all graphics pipeline shader stages.
class GP_RHI_API GraphicsShader : public Shader
{
public:
    GraphicsShader() = delete;
    GraphicsShader(const GraphicsShader&) = delete;
    GraphicsShader& operator=(const GraphicsShader&) = delete;
    GraphicsShader(GraphicsShader&&) = delete;
    GraphicsShader& operator=(GraphicsShader&&) = delete;

protected:
    /// @brief Constructs a graphics shader stage resource.
    /// @param[in] resourceType Concrete `resource::ResourceType` tag.
    /// @param[in] frequency Graphics `ShaderFrequency` stage.
    explicit GraphicsShader(resource::ResourceType resourceType, ShaderFrequency frequency) noexcept;

    /// @copydoc Shader::~Shader()
    virtual ~GraphicsShader() noexcept override;
};

}   // namespace gp::rhi::shader
