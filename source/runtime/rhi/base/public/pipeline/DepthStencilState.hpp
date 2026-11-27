// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ExclusiveDepthStencil.hpp"
#include "core/Resource.hpp"
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"

namespace gp::rhi::pipeline
{

/// @brief Immutable GPU depth-stencil test and write state resource.
class GP_RHI_API DepthStencilState : public resource::Resource
{
public:
    /// @brief Constructs a depth-stencil state tagged with `ResourceType::DepthStencilState`.
    DepthStencilState() noexcept;

    DepthStencilState(const DepthStencilState&) = delete;
    DepthStencilState& operator=(const DepthStencilState&) = delete;
    DepthStencilState(DepthStencilState&&) = delete;
    DepthStencilState& operator=(DepthStencilState&&) = delete;

public:
    /// @brief Populates `outDesc` with the `DepthStencilStateDesc` used to create this state object.
    /// @param[out] outDesc Receives the creation parameters when supported by the concrete backend.
    /// @return True if `outDesc` was populated; false if the backend does not retain creation parameters.
    [[nodiscard]] virtual bool getInitializer(DepthStencilStateDesc& outDesc) const noexcept;

#if GP_ENABLE_RHI_VALIDATION
    /// @brief Stores the actual depth/stencil read-write mode derived from `DepthStencilStateDesc`.
    resource::ExclusiveDepthStencil actualDSMode{};
#endif

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~DepthStencilState() noexcept override;
};

}   // namespace gp::rhi::pipeline
