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

/// @brief Immutable GPU multi-target color blend state resource.
class GP_RHI_API BlendState : public resource::Resource
{
public:
    /// @brief Constructs a blend state tagged with `ResourceType::BlendState`.
    BlendState() noexcept;

    BlendState(const BlendState&) = delete;
    BlendState& operator=(const BlendState&) = delete;
    BlendState(BlendState&&) = delete;
    BlendState& operator=(BlendState&&) = delete;

public:
    /// @brief Populates `outDesc` with the `BlendStateDesc` used to create this state object.
    /// @param[out] outDesc Receives the creation parameters when supported by the concrete backend.
    /// @return True if `outDesc` was populated; false if the backend does not retain creation parameters.
    [[nodiscard]] virtual bool getInitializer(BlendStateDesc& outDesc) const noexcept;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~BlendState() noexcept override;
};

}   // namespace gp::rhi::pipeline
