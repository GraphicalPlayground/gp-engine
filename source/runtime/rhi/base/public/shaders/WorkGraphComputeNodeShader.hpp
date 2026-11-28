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

/// @brief Compiled work graph compute node shader stage resource.
class GP_RHI_API WorkGraphComputeNodeShader : public WorkGraphShader
{
public:
    /// @brief Compile-time pipeline stage for `WorkGraphComputeNodeShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::WorkGraphComputeNode;

    /// @brief Constructs a work graph compute node shader resource.
    WorkGraphComputeNodeShader() noexcept;

    WorkGraphComputeNodeShader(const WorkGraphComputeNodeShader&) = delete;
    WorkGraphComputeNodeShader& operator=(const WorkGraphComputeNodeShader&) = delete;
    WorkGraphComputeNodeShader(WorkGraphComputeNodeShader&&) = delete;
    WorkGraphComputeNodeShader& operator=(WorkGraphComputeNodeShader&&) = delete;

protected:
    /// @copydoc WorkGraphShader::~WorkGraphShader()
    virtual ~WorkGraphComputeNodeShader() noexcept override;
};

}   // namespace gp::rhi::shader
