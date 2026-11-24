// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/CommandListImmediate.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "device/DynamicRHI.hpp"
#include <utility>

namespace gp::rhi::command
{

namespace
{

void appendUniqueContext(gp::Vector<IComputeContext*>& outContexts, IComputeContext* const ctx) noexcept
{
    if (ctx == nullptr)
    {
        return;
    }
    for (IComputeContext* const existing: outContexts)
    {
        if (existing == ctx)
        {
            return;
        }
    }
    outContexts.emplaceBack(ctx);
}

}   // namespace

void CommandListExecutor::latchBypass() noexcept
{
    if (m_bypass == m_bypassRequested)
    {
        return;
    }

    if (m_commandListImmediate.hasCommands())
    {
        submit({}, SubmitFlags::SubmitToGPU);
    }
    m_bypass = m_bypassRequested;
}

void CommandListExecutor::setBypass(const bool enableBypass) noexcept
{
    m_bypassRequested = enableBypass;
    latchBypass();
}

void CommandListExecutor::executeList(CommandListBase& cmdList) noexcept
{
    cmdList.execute();
    ++m_executedListCount;
}

void CommandListExecutor::submit(
    const gp::ConstVectorView<CommandListBase*> additionalCommandLists, const SubmitFlags submitFlags
) noexcept
{
    FinalizeContextsArgs finalizeArgs{};
    bool executedImmediate = false;

    if (m_commandListImmediate.hasCommands())
    {
        executeList(m_commandListImmediate);
        executedImmediate = true;
    }

    // Gather active contexts from the immediate list in both queued and bypass modes.
    if (executedImmediate || m_commandListImmediate.m_activePipelines != sync::PipelineStage::None)
    {
        for (IComputeContext* const ctx: m_commandListImmediate.m_contexts)
        {
            appendUniqueContext(finalizeArgs.contexts, ctx);
        }
        m_commandListImmediate.detachContexts();
    }

    for (CommandListBase* const cmdList: additionalCommandLists)
    {
        if (cmdList != nullptr && cmdList->hasCommands())
        {
            executeList(*cmdList);
            for (IComputeContext* const ctx: cmdList->m_contexts)
            {
                appendUniqueContext(finalizeArgs.contexts, ctx);
            }
            cmdList->detachContexts();
        }
    }

    DynamicRHI* const rhi = getDynamicRHI();
    if (rhi != nullptr)
    {
        if (!finalizeArgs.contexts.isEmpty())
        {
            FinalizeContextsResult finalizeResult{};
            rhi->finalizeContexts(std::move(finalizeArgs), finalizeResult);
            if (!finalizeResult.commandLists.isEmpty())
            {
                SubmitCommandListsArgs submitArgs{};
                submitArgs.commandLists = std::move(finalizeResult.commandLists);
                submitArgs.flags = submitFlags;
                rhi->submitCommandLists(std::move(submitArgs));
            }
        }
    }

    if ((submitFlags & SubmitFlags::DeleteResources) != SubmitFlags::None)
    {
        const bool includeExtended = resource::ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources();
        resource::ResourceDeletionQueue::flushPendingDeletes(includeExtended);
        if (rhi != nullptr)
        {
            rhi->processDeleteQueue();
        }
    }

    if ((submitFlags & SubmitFlags::EndFrame) != SubmitFlags::None && rhi != nullptr)
    {
        rhi->endFrameRenderThread();
    }

    if ((submitFlags & SubmitFlags::EnableBypass) != SubmitFlags::None)
    {
        m_bypassRequested = true;
        m_bypass = true;
    }
    else if ((submitFlags & SubmitFlags::DisableBypass) != SubmitFlags::None)
    {
        m_bypassRequested = false;
        m_bypass = false;
    }

    ++m_submitCount;
}

void CommandListExecutor::immediateFlush(const ImmediateFlushType type, const SubmitFlags submitFlags) noexcept
{
    switch (type)
    {
    case ImmediateFlushType::WaitForOutstandingTasksOnly:
        return;

    case ImmediateFlushType::DispatchToRHIThread:
        submit({}, submitFlags | SubmitFlags::SubmitToGPU);
        break;

    case ImmediateFlushType::FlushRHIThread:
        submit({}, submitFlags | SubmitFlags::SubmitToGPU | SubmitFlags::FlushRHIThread);
        break;

    case ImmediateFlushType::FlushRHIThreadFlushResources:
        submit({}, submitFlags | SubmitFlags::SubmitToGPU | SubmitFlags::FlushRHIThread | SubmitFlags::DeleteResources);
        break;

    default:
        break;
    }
}

void CommandListExecutor::reset() noexcept
{
    m_commandListImmediate.reset();
    m_commandListImmediate.detachContexts();
    m_commandListImmediate.m_activePipeline = sync::PipelineStage::Graphics;
    m_commandListImmediate.m_initialPipeline = sync::PipelineStage::Graphics;
    m_commandListImmediate.m_persistentState = CommandListBase::PersistentState{};
    m_executedListCount = 0u;
    m_submitCount = 0u;
    m_bypass = false;
    m_bypassRequested = false;
}

CommandListExecutor& getCommandListExecutor() noexcept
{
    static CommandListExecutor s_executor{};
    return s_executor;
}

}   // namespace gp::rhi::command
