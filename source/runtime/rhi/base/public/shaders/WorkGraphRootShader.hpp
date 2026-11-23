// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "RHIBase.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/WorkGraphShader.hpp"

namespace gp::rhi::shader
{

/// @brief Compiled work graph root shader stage resource.
class GP_RHI_API WorkGraphRootShader : public WorkGraphShader
{
public:
    /// @brief Compile-time pipeline stage for `WorkGraphRootShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::WorkGraphRoot;

    /// @brief Constructs a work graph root shader resource.
    WorkGraphRootShader() noexcept;

    WorkGraphRootShader(const WorkGraphRootShader&) = delete;
    WorkGraphRootShader& operator=(const WorkGraphRootShader&) = delete;
    WorkGraphRootShader(WorkGraphRootShader&&) = delete;
    WorkGraphRootShader& operator=(WorkGraphRootShader&&) = delete;

protected:
    /// @copydoc WorkGraphShader::~WorkGraphShader()
    virtual ~WorkGraphRootShader() noexcept override;
};

}   // namespace gp::rhi::shader
