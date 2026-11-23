// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "RHIBase.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::sync
{

/// @brief GPU render query resource for occlusion sample counts and hardware timestamps.
/// @details Backend subclasses store the native query handle or pool slot index. Instances are typically leased
///          through `RenderQueryPool` and returned automatically by `PooledRenderQuery`.
class GP_RHI_API RenderQuery : public resource::Resource
{
public:
    /// @brief Constructs a render query tagged with `ResourceType::RenderQuery`.
    RenderQuery() noexcept;

    RenderQuery(const RenderQuery&) = delete;
    RenderQuery& operator=(const RenderQuery&) = delete;
    RenderQuery(RenderQuery&&) = delete;
    RenderQuery& operator=(RenderQuery&&) = delete;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~RenderQuery() noexcept override;
};

}   // namespace gp::rhi::sync
