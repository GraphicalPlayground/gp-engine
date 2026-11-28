// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceForward.hpp"              // IWYU pragma: keep
#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"

namespace gp::rhi::vulkan
{

/// @brief Resolves the command contexts that resource operations record into.
/// @details This is the seam between resources and the command list contexts. The production implementation is
///          backed by the dynamic RHI and its graphics, async compute and upload contexts. Tests plug in a fake that
///          captures commands and runs every lambda inline.
/// @note Called on the RHI timeline (bottom of pipe) unless stated otherwise.
class IVulkanCommandContextProvider
{
public:
    /// @brief Virtual destructor.
    virtual ~IVulkanCommandContextProvider() noexcept = default;

    /// @brief Returns the recorder of the command list context executing `cmdList`.
    /// @details Copies recorded here are ordered with the draws and dispatches of that command list.
    /// @param[in,out] cmdList Executing command list.
    /// @return Recorder, or `nullptr` when no context is available.
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getCommandListRecorder(command::CommandListBase& cmdList) noexcept = 0;

    /// @brief Returns the recorder of the upload context attached to `cmdList`.
    /// @details Upload work is submitted ahead of the graphics work, so only first writes may use it.
    /// @param[in,out] cmdList Executing command list.
    /// @return Recorder, or `nullptr` when no context is available.
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getUploadRecorder(command::CommandListBase& cmdList) noexcept = 0;

    /// @brief Returns the recorder wrapping a pipeline context handed to a multi-pipe lambda.
    /// @param[in] context Context from a `ContextArray`, may be `nullptr`.
    /// @return Recorder, or `nullptr` when `context` is null or not a Vulkan context.
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getContextRecorder(command::IComputeContext* context) noexcept = 0;

    /// @brief Returns the pipelines `context` last tracked `resource` on.
    /// @param[in] context Pipeline context.
    /// @param[in] resource Resource to look up.
    /// @return Pipeline mask, `None` when the resource is untracked.
    [[nodiscard]] virtual sync::PipelineStage
        getTrackedPipelines(command::IComputeContext* context, const resource::Resource* resource) noexcept = 0;

    /// @brief Submits everything recorded on the immediate command list and waits for the GPU to go idle.
    /// @param[in,out] cmdList Immediate command list.
    virtual void submitAndBlockUntilGPUIdle(command::CommandListImmediate& cmdList) noexcept = 0;

    /// @brief Reports whether high-level code relies on copies being followed by a full barrier.
    /// @details False while a batch of copies is grouped in a pass that issues its own transitions.
    /// @param[in] cmdList Command list issuing the copy (top of pipe).
    /// @return True when the copy must be followed by a barrier.
    [[nodiscard]] virtual bool needsExtraTransitions(const command::CommandListBase& cmdList) const noexcept = 0;
};

}   // namespace gp::rhi::vulkan

