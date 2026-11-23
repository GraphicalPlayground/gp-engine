// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "pipeline/PipelineForward.hpp"
#include "RHIBase.hpp"
#include "shaders/Shader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::shader
{

/// @brief Compiled compute shader stage resource.
/// @details Optionally attaches a non-owning `pipeline::PipelineStateStats` pointer so compute dispatches can record
///          frame usage and bind counts without requiring a separate wrapper lookup.
class GP_RHI_API ComputeShader : public Shader
{
public:
    /// @brief Compile-time pipeline stage for `ComputeShader`.
    static constexpr ShaderFrequency kStaticFrequency = ShaderFrequency::Compute;

    /// @brief Constructs a compute shader resource.
    ComputeShader() noexcept;

    ComputeShader(const ComputeShader&) = delete;
    ComputeShader& operator=(const ComputeShader&) = delete;
    ComputeShader(ComputeShader&&) = delete;
    ComputeShader& operator=(ComputeShader&&) = delete;

protected:
    /// @copydoc Shader::~Shader()
    virtual ~ComputeShader() noexcept override;

public:
    /// @brief Attaches a pipeline usage stats record to this compute shader.
    /// @param[in] stats Non-owning pointer to a `pipeline::PipelineStateStats` record, or `nullptr` to detach.
    GP_FORCEINLINE_HINT void setStats(pipeline::PipelineStateStats* const stats) noexcept
    {
        m_stats = stats;
    }

    /// @brief Returns the attached pipeline usage stats record, if any.
    /// @return Pointer to `pipeline::PipelineStateStats`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT pipeline::PipelineStateStats* getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Updates attached pipeline usage counters when this compute shader is bound during `currentFrame`.
    /// @param[in] currentFrame Monotonic engine frame index.
    void updateStats(gp::UInt64 currentFrame) noexcept;

private:
    /// @brief Optional non-owning pointer to cached pipeline usage statistics.
    pipeline::PipelineStateStats* m_stats{ nullptr };
};

}   // namespace gp::rhi::shader
