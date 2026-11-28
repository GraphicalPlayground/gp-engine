// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "device/DeviceForward.hpp"             // IWYU pragma: keep
#include "device/DynamicRHI.hpp"
#include "memory/pointers/RefCountedPtr.hpp"    // IWYU pragma: keep
#include "pipeline/ComputePipelineState.hpp"    // IWYU pragma: keep
#include "pipeline/GraphicsPipelineState.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"

namespace gp::rhi
{

/// @brief Intermediate `DynamicRHI` base supplying fallback PSO creation for backends without native PSOs.
class GP_RHI_API DynamicRHIPSOFallback : public DynamicRHI
{
public:
    /// @brief Constructs the PSO fallback base.
    DynamicRHIPSOFallback() noexcept = default;

    /// @copydoc DynamicRHI::~DynamicRHI()
    virtual ~DynamicRHIPSOFallback() noexcept override = default;

public:
    /// @copydoc DynamicRHI::createGraphicsPipelineState()
    [[nodiscard]] virtual pipeline::GraphicsPipelineStateRef
        createGraphicsPipelineState(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept override;

    /// @copydoc DynamicRHI::createComputePipelineState(shader::ComputeShader*)
    [[nodiscard]] virtual pipeline::ComputePipelineStateRef
        createComputePipelineState(shader::ComputeShader* computeShader) noexcept override;

    using DynamicRHI::createComputePipelineState;
};

}   // namespace gp::rhi
