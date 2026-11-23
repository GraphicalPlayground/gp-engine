// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"

namespace gp::rhi::pipeline
{

/// @brief Immutable GPU fixed-function polygon rasterizer state resource.
class GP_RHI_API RasterizerState : public resource::Resource
{
public:
    /// @brief Constructs a rasterizer state tagged with `ResourceType::RasterizerState`.
    RasterizerState() noexcept;

    RasterizerState(const RasterizerState&) = delete;
    RasterizerState& operator=(const RasterizerState&) = delete;
    RasterizerState(RasterizerState&&) = delete;
    RasterizerState& operator=(RasterizerState&&) = delete;

public:
    /// @brief Populates `outDesc` with the `RasterizerStateDesc` used to create this state object.
    /// @param[out] outDesc Receives the creation parameters when supported by the concrete backend.
    /// @return True if `outDesc` was populated; false if the backend does not retain creation parameters.
    [[nodiscard]] virtual bool getInitializer(RasterizerStateDesc& outDesc) const noexcept;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~RasterizerState() noexcept override;
};

}   // namespace gp::rhi::pipeline
