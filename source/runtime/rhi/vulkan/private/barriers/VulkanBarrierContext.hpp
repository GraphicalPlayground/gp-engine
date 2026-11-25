// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "commands/CommandForward.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "sync/SyncEnums.hpp"

namespace gp::rhi::vulkan
{

/// @brief References every collaborator needed to turn transitions into recorded barriers.
/// @details One context is built per command list context and per pipeline. It bundles:
///          - the RHI context holding tracked resource states (used to resolve `ResourceAccess::Unknown`),
///          - the resolver mapping RHI resources to Vulkan handles,
///          - the recorder receiving barriers, events and semaphores,
///          - the pipeline the work executes on, and snapshots of the device, translation and setting values.
/// @note The referenced objects must outlive the context. The context itself is cheap to copy.
struct VulkanBarrierContext
{
    /// @brief Constructs a context.
    /// @param[in,out] inTrackingContext RHI context holding tracked access per resource.
    /// @param[in] inResolver Resource resolver.
    /// @param[in,out] inRecorder Command recorder.
    /// @param[in] inExecutingPipeline Pipeline the recorded work runs on.
    /// @param[in] inDevice Device snapshot.
    /// @param[in] inTranslation Translation inputs.
    /// @param[in] inSettings Barrier settings.
    VulkanBarrierContext(
        command::IComputeContext& inTrackingContext,
        const IVulkanBarrierResourceResolver& inResolver,
        IVulkanBarrierCommandRecorder& inRecorder,
        const sync::PipelineStage inExecutingPipeline,
        const VulkanBarrierDeviceDesc& inDevice,
        const VulkanBarrierTranslationDesc& inTranslation,
        const VulkanBarrierSettings& inSettings
    ) noexcept
        : trackingContext(&inTrackingContext)
        , resolver(&inResolver)
        , recorder(&inRecorder)
        , executingPipeline(inExecutingPipeline)
        , device(inDevice)
        , translation(inTranslation)
        , settings(inSettings)
    {}

    /// @brief RHI context holding tracked access per resource.
    command::IComputeContext* trackingContext{ nullptr };

    /// @brief Resolver mapping RHI resources to Vulkan handles.
    const IVulkanBarrierResourceResolver* resolver{ nullptr };

    /// @brief Recorder receiving barriers, events and semaphores.
    IVulkanBarrierCommandRecorder* recorder{ nullptr };

    /// @brief Pipeline the recorded work runs on.
    sync::PipelineStage executingPipeline{ sync::PipelineStage::Graphics };

    /// @brief Device snapshot.
    VulkanBarrierDeviceDesc device{};

    /// @brief Translation inputs.
    VulkanBarrierTranslationDesc translation{};

    /// @brief Barrier settings.
    VulkanBarrierSettings settings{};
};

}   // namespace gp::rhi::vulkan
