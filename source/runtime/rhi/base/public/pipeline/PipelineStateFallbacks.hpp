// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/ComputePipelineState.hpp"
#include "pipeline/GraphicsPipelineState.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "shaders/GraphicsShader.hpp"
#include "shaders/ShaderEnums.hpp"
#include <utility>

namespace gp::rhi::pipeline
{

/// @brief Fallback `GraphicsPipelineState` storing the raw `GraphicsPipelineStateInitializer` for non-PSO backends.
/// @details Backends without native monolithic PSOs (such as `rhi/null` or legacy APIs) instantiate this class via
///          `DynamicRHIPSOFallback` and unpack `initializer` into disjoint fixed-function state binds inside
///          `ICommandContextPSOFallback::setGraphicsPipelineState()`.
class GP_RHI_API GraphicsPipelineStateFallback final : public GraphicsPipelineState
{
public:
    /// @brief Constructs a fallback graphics PSO retaining a copy of `inInitializer` and optional `BoundShaderState`.
    /// @param[in] inInitializer Full graphics pipeline creation descriptor.
    /// @param[in] inBoundShaderState Optional pre-created bound shader state matching `inInitializer.boundShaderState`.
    explicit GraphicsPipelineStateFallback(
        const GraphicsPipelineStateInitializer& inInitializer, BoundShaderStateRef inBoundShaderState = {}
    ) noexcept
        : initializer(inInitializer)
        , boundShaderState(std::move(inBoundShaderState))
    {}

    /// @copydoc GraphicsPipelineState::getShader()
    [[nodiscard]] virtual shader::GraphicsShader*
        getShader(const shader::ShaderFrequency frequency) const noexcept override
    {
        if (!shader::isValidGraphicsFrequency(frequency))
        {
            return nullptr;
        }
        return static_cast<shader::GraphicsShader*>(initializer.boundShaderState.getShader(frequency));
    }

public:
    /// @brief Retained creation descriptor unpacked during fallback command context binding.
    GraphicsPipelineStateInitializer initializer{};

    /// @brief Pre-created bound shader state passed to `ICommandContextPSOFallback::setBoundShaderState()`.
    BoundShaderStateRef boundShaderState{};

protected:
    /// @copydoc GraphicsPipelineState::~GraphicsPipelineState()
    virtual ~GraphicsPipelineStateFallback() noexcept override = default;
};

/// @brief Fallback `ComputePipelineState` wrapping a `ComputeShader` for backends without native compute PSOs.
class GP_RHI_API ComputePipelineStateFallback final : public ComputePipelineState
{
public:
    /// @brief Constructs a fallback compute PSO bound to `inComputeShader`.
    /// @param[in] inComputeShader Non-null compute shader resource.
    explicit ComputePipelineStateFallback(shader::ComputeShader* const inComputeShader) noexcept
        : ComputePipelineState(inComputeShader)
    {}

protected:
    /// @copydoc ComputePipelineState::~ComputePipelineState()
    virtual ~ComputePipelineStateFallback() noexcept override = default;
};

}   // namespace gp::rhi::pipeline
