// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "views/DescriptorHandle.hpp"

namespace gp::rhi::pipeline
{

/// @brief Immutable GPU texture sampler state resource.
/// @details Backend subclasses store the native sampler object or descriptor index and may optionally override
///          `isImmutable()` and `getBindlessHandle()` for static/bindless sampler heaps.
class GP_RHI_API SamplerState : public resource::Resource
{
public:
    /// @brief Constructs a sampler state tagged with `ResourceType::SamplerState`.
    SamplerState() noexcept;

    SamplerState(const SamplerState&) = delete;
    SamplerState& operator=(const SamplerState&) = delete;
    SamplerState(SamplerState&&) = delete;
    SamplerState& operator=(SamplerState&&) = delete;

public:
    /// @brief Reports whether this sampler maps to a static or immutable hardware sampler slot.
    /// @return True when backed by an immutable hardware sampler; false by default.
    [[nodiscard]] virtual bool isImmutable() const noexcept;

    /// @brief Returns the bindless descriptor heap handle for this sampler when bindless sampling is active.
    /// @return Valid `view::DescriptorHandle` on bindless-enabled backends, or an invalid handle by default.
    [[nodiscard]] virtual view::DescriptorHandle getBindlessHandle() const noexcept;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~SamplerState() noexcept override;
};

}   // namespace gp::rhi::pipeline
