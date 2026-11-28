// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandForward.hpp"       // IWYU pragma: keep
#include "commands/CommandListImmediate.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::command
{

/// @brief Central orchestrator managing the singleton `CommandListImmediate`, bypass latching, command replay, and
///        deferred resource deletion submission.
/// @details Coordinates the render-thread command stream lifecycle: latches bypass mode safely between frames,
///          replays recorded `CommandListBase` packet streams into `DynamicRHI` contexts, finalizes and submits
///          platform command lists, and drains `ResourceDeletionQueue` batches when `SubmitFlags::DeleteResources`
///          is set.
class GP_RHI_API CommandListExecutor final
{
public:
    /// @brief Constructs the executor with bypass mode disabled until explicitly latched.
    CommandListExecutor() noexcept = default;

    CommandListExecutor(const CommandListExecutor&) = delete;
    CommandListExecutor& operator=(const CommandListExecutor&) = delete;
    CommandListExecutor(CommandListExecutor&&) = delete;
    CommandListExecutor& operator=(CommandListExecutor&&) = delete;

    /// @brief Destroys the executor and resets its immediate command list.
    ~CommandListExecutor() noexcept = default;

public:
    /// @brief Returns the global singleton `CommandListImmediate`.
    /// @return Mutable reference to the engine's immediate command list.
    [[nodiscard]] GP_FORCEINLINE_HINT CommandListImmediate& getImmediateCommandList() noexcept
    {
        return m_commandListImmediate;
    }

    /// @brief Reports whether bypass mode is currently active (commands execute directly into `ICommandContext`).
    /// @return True when bypass mode is enabled.
    [[nodiscard]] GP_FORCEINLINE_HINT bool bypass() const noexcept
    {
        return m_bypass;
    }

    /// @brief Requests enabling or disabling bypass mode on the next `latchBypass()` boundary.
    /// @param[in] enableBypass True to execute immediate commands directly without packet queuing.
    void setBypassRequested(const bool enableBypass) noexcept
    {
        m_bypassRequested = enableBypass;
    }

    /// @brief Reports whether bypass mode has been requested for the next latch point.
    /// @return Requested bypass flag.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isBypassRequested() const noexcept
    {
        return m_bypassRequested;
    }

    /// @brief Applies `isBypassRequested()` to the live bypass flag after flushing any queued immediate commands.
    void latchBypass() noexcept;

    /// @brief Immediately enables or disables bypass mode, flushing queued immediate commands first.
    /// @param[in] enableBypass True for synchronous bypass execution; false for queued command packet recording.
    void setBypass(bool enableBypass) noexcept;

    /// @brief Replays a single command list's recorded packets into backend contexts and resets its arena.
    /// @param[in,out] cmdList Command list to execute.
    void executeList(CommandListBase& cmdList) noexcept;

    /// @brief Executes queued immediate commands plus `additionalCommandLists`, finalizes backend contexts, submits
    ///        platform command lists to `DynamicRHI`, and optionally drains deferred resource deletions.
    /// @param[in] additionalCommandLists Optional span of deferred command lists to execute after the immediate list.
    /// @param[in] submitFlags Bitflags controlling resource deletion and submission behavior.
    void submit(
        gp::ConstVectorView<CommandListBase*> additionalCommandLists = {}, SubmitFlags submitFlags = SubmitFlags::None
    ) noexcept;

    /// @brief Executes a flush on the immediate command list according to `type` and `submitFlags`.
    /// @param[in] type Requested flush granularity.
    /// @param[in] submitFlags Submission bitflags.
    void immediateFlush(ImmediateFlushType type, SubmitFlags submitFlags = SubmitFlags::None) noexcept;

    /// @brief Returns the cumulative count of command list executions performed by this executor (for diagnostics).
    /// @return Total executed command list count since startup or `reset()`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getExecutedListCount() const noexcept
    {
        return m_executedListCount;
    }

    /// @brief Returns the cumulative count of `submit()` calls processed by this executor (for diagnostics).
    /// @return Total submission count since startup or `reset()`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getSubmitCount() const noexcept
    {
        return m_submitCount;
    }

    /// @brief Resets the immediate command list and diagnostic counters (used during `rhiExit()` and unit tests).
    void reset() noexcept;

private:
    /// @brief Engine-wide immediate command list instance.
    CommandListImmediate m_commandListImmediate{};

    /// @brief Cumulative count of `executeList()` calls.
    gp::UInt64 m_executedListCount{ 0u };

    /// @brief Cumulative count of `submit()` calls.
    gp::UInt64 m_submitCount{ 0u };

    /// @brief Active bypass mode flag.
    bool m_bypass{ false };

    /// @brief Pending bypass mode value latched at frame/flush boundaries.
    bool m_bypassRequested{ false };
};

/// @brief Returns the global `CommandListExecutor` singleton.
/// @return Mutable reference to the process-wide `CommandListExecutor`.
[[nodiscard]] GP_RHI_API CommandListExecutor& getCommandListExecutor() noexcept;

/// @brief Convenience accessor returning the global `CommandListImmediate` singleton.
/// @return Mutable reference to `getCommandListExecutor().getImmediateCommandList()`.
[[nodiscard]] inline CommandListImmediate& getImmediateCommandList() noexcept
{
    return getCommandListExecutor().getImmediateCommandList();
}

}   // namespace gp::rhi::command
