// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/IComputeContext.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep
#include "sync/TransitionDesc.hpp"

namespace gp::rhi::sync
{

/// @brief Resolves concrete before/after access masks and pipeline stages for a single `TransitionInfo`.
/// @details Replaces `ResourceAccess::Unknown` (and `ResourceAccess::Discard` when
///          `getRHIGlobals().needsTransientDiscardStateTracking` is active) using the tracked state recorded on
///          `context` for the underlying `resource::ViewableResource`.
struct ResourceState final
{
public:
    /// @brief Resolved GPU access state prior to the barrier.
    ResourceAccess accessBefore{ ResourceAccess::Unknown };

    /// @brief Resolved GPU access state after the barrier.
    ResourceAccess accessAfter{ ResourceAccess::Unknown };

    /// @brief Source execution pipelines; may differ from the declared source mask when tracked state was collapsed
    ///        across pipelines (for example `PipelineStage::All`).
    PipelineStage srcPipelines{ PipelineStage::None };

    /// @brief Destination execution pipelines receiving the transitioned resource.
    PipelineStage dstPipelines{ PipelineStage::None };

public:
    /// @brief Resolves before/after states and pipeline masks for `info` against `context`.
    /// @param[in] context Active backend compute or graphics context holding tracked resource states.
    /// @param[in] inSrcPipelines Declared source execution pipeline mask.
    /// @param[in] inDstPipelines Declared destination execution pipeline mask.
    /// @param[in] info Per-resource transition descriptor to resolve.
    ResourceState(
        command::IComputeContext& context,
        const PipelineStage inSrcPipelines,
        const PipelineStage inDstPipelines,
        const TransitionInfo& info
    ) noexcept
        : accessBefore(info.accessBefore)
        , accessAfter(info.accessAfter)
        , srcPipelines(inSrcPipelines)
        , dstPipelines(inDstPipelines)
    {
        if (resource::ViewableResource* const viewableResource = getViewableResource(info); viewableResource != nullptr)
        {
            if (needToReplaceBeforeInfo())
            {
                srcPipelines = context.getTrackedPipelines(viewableResource);
                accessBefore = context.getTrackedAccess(viewableResource);
            }

            if (accessAfter == ResourceAccess::Unknown)
            {
                accessAfter = context.getTrackedAccess(viewableResource);
            }

            GP_CHECK(accessBefore != ResourceAccess::Unknown);
            GP_CHECK(accessAfter != ResourceAccess::Unknown);
        }
    }

private:
    /// @brief Checks whether `accessBefore` and `srcPipelines` must be fetched from the context's tracked state.
    /// @return True when `accessBefore` is `Unknown`, or `Discard` under transient discard state tracking.
    [[nodiscard]] bool needToReplaceBeforeInfo() const noexcept
    {
        if (accessBefore == ResourceAccess::Unknown)
        {
            return true;
        }

        if (accessBefore == ResourceAccess::Discard && getRHIGlobals().needsTransientDiscardStateTracking)
        {
            return true;
        }

        return false;
    }
};

}   // namespace gp::rhi::sync
