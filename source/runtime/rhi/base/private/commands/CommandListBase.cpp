// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandListBase.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/CommandPacket.hpp"
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "memory/allocators/PagePool.hpp"
#include "memory/Memory.hpp"
#include "sync/Transition.hpp"
#include <new>   // IWYU pragma: keep

namespace gp::rhi::command
{

namespace
{

constexpr gp::USize kCommandListPageSizeBytes = 64u * 1024u;
constexpr gp::UInt32 kCommandListPageCount = 64u;

}   // namespace

gp::memory::PagePool& CommandListBase::getSharedPagePool() noexcept
{
    static gp::memory::PagePool s_pagePool(
        kCommandListPageSizeBytes, kCommandListPageCount, nullptr, "RHI.CmdListPool"
    );
    return s_pagePool;
}

CommandListBase::CommandListBase(const GPUMask gpuMask, const bool isImmediateList) noexcept
    : m_memManager(&getSharedPagePool(), nullptr, true, "RHI.CommandList")
    , m_commandLink(&m_root)
    , m_scratchParameterAllocator(&getSharedPagePool())
    , m_scratchShaderParameters(m_scratchParameterAllocator)
    , m_isImmediate(isImmediateList)
{
    m_persistentState.initialGPUMask = gpuMask;
    m_persistentState.currentGPUMask = gpuMask;
}

CommandListBase::~CommandListBase() noexcept
{
    reset();
}

void* CommandListBase::alloc(const gp::USize allocSize, const gp::UInt32 alignment) noexcept
{
    if (allocSize == 0u)
    {
        return nullptr;
    }
    return m_memManager.allocate(allocSize, alignment);
}

void* CommandListBase::allocCopy(
    const void* const inSourceData, const gp::USize sizeInBytes, const gp::UInt32 alignment
) noexcept
{
    if (sizeInBytes == 0u || inSourceData == nullptr)
    {
        return nullptr;
    }

    void* const dst = alloc(sizeInBytes, alignment);
    GP_CHECKF(dst != nullptr, "CommandListBase::allocCopy failed to allocate arena bytes.");
    gp::memory::copyMemory(dst, inSourceData, sizeInBytes);
    return dst;
}

const char* CommandListBase::allocString(const gp::StringView text) noexcept
{
    if (text.isEmpty())
    {
        return "";
    }

    const gp::USize length = text.size();
    char* const dst = static_cast<char*>(alloc(length + 1u, 1u));
    GP_CHECKF(dst != nullptr, "CommandListBase::allocString failed to allocate string bytes.");
    gp::memory::copyMemory(dst, text.data(), length);
    dst[length] = '\0';
    return dst;
}

void* CommandListBase::allocCommand(const gp::USize allocSize, const gp::UInt32 alignment) noexcept
{
    GP_CHECKF_SLOW(!m_isExecuting, "Cannot allocate command packets while executing a command list.");
    GP_CHECKF_SLOW(!bypass(), "Cannot allocate command packets while bypass mode is enabled.");
    void* const mem = m_memManager.allocate(allocSize, alignment);
    GP_CHECKF(mem != nullptr, "CommandListBase::allocCommand failed to allocate command packet.");
    CommandBase* const cmd = static_cast<CommandBase*>(mem);
    *m_commandLink = cmd;
    m_commandLink = &cmd->next;
    ++m_numCommands;
    return mem;
}

bool CommandListBase::needsShaderUnbinds() const noexcept
{
    return getRHIGlobals().needsShaderUnbinds;
}

CommandListImmediate& CommandListBase::getAsImmediate() noexcept
{
    GP_CHECKF(m_isImmediate, "CommandListBase::getAsImmediate() called on a non-immediate command list.");
    return static_cast<CommandListImmediate&>(*this);
}

bool CommandListBase::bypass() const noexcept
{
    return m_isImmediate && getCommandListExecutor().bypass();
}

sync::PipelineStage CommandListBase::switchPipeline(const sync::PipelineStage pipeline) noexcept
{
    const sync::PipelineStage previous = m_activePipeline;
    if (pipeline == previous)
    {
        return previous;
    }

    m_activePipeline = pipeline;
    if (pipeline != sync::PipelineStage::None)
    {
        GP_CHECKF(sync::isSinglePipelineStage(pipeline), "switchPipeline requires a single pipeline bit or None.");
        m_activePipelines |= pipeline;
        if (isBottomOfPipe())
        {
            activatePipelines(pipeline);
            m_computeContext = m_contexts[pipeline];
        }
        else
        {
            emplaceCommand<CommandSwitchPipeline>(pipeline);
        }
    }

    return previous;
}

void CommandListBase::activatePipelines(const sync::PipelineStage pipelines) noexcept
{
    if (pipelines == sync::PipelineStage::None)
    {
        return;
    }

    m_activePipelines |= pipelines;
    DynamicRHI* const rhi = getDynamicRHI();
    if (rhi == nullptr)
    {
        return;
    }

    if ((pipelines & sync::PipelineStage::Graphics) != sync::PipelineStage::None)
    {
        if (m_contexts[sync::PipelineStage::Graphics] == nullptr)
        {
            ICommandContext* ctx = nullptr;
            if (!m_isImmediate)
            {
                IComputeContext* const parallelCtx = rhi->getCommandContext(sync::PipelineStage::Graphics);
                if (parallelCtx != nullptr)
                {
                    ctx = static_cast<ICommandContext*>(parallelCtx);
                }
            }
            if (ctx == nullptr)
            {
                ctx = rhi->getDefaultContext();
            }
            if (ctx != nullptr)
            {
                ctx->setGPUMask(m_persistentState.currentGPUMask);
            }
            m_contexts[sync::PipelineStage::Graphics] = ctx;
            m_graphicsContext = ctx;
        }
    }

    if ((pipelines & sync::PipelineStage::AsyncCompute) != sync::PipelineStage::None)
    {
        if (m_contexts[sync::PipelineStage::AsyncCompute] == nullptr)
        {
            IComputeContext* ctx = nullptr;
            if (!m_isImmediate)
            {
                ctx = rhi->getCommandContext(sync::PipelineStage::AsyncCompute);
            }
            if (ctx == nullptr)
            {
                ctx = rhi->getDefaultAsyncComputeContext();
            }
            if (ctx != nullptr)
            {
                ctx->setGPUMask(m_persistentState.currentGPUMask);
            }
            m_contexts[sync::PipelineStage::AsyncCompute] = ctx;
        }
    }

    if (m_activePipeline != sync::PipelineStage::None)
    {
        m_computeContext = m_contexts[m_activePipeline];
    }
}

ICommandContext& CommandListBase::getContext() noexcept
{
    if (m_graphicsContext == nullptr)
    {
        activatePipelines(sync::PipelineStage::Graphics);
    }
    GP_CHECKF(m_graphicsContext != nullptr, "CommandListBase::getContext() has no bound graphics ICommandContext.");
    return *m_graphicsContext;
}

IComputeContext& CommandListBase::getComputeContext() noexcept
{
    const sync::PipelineStage target =
        (m_activePipeline != sync::PipelineStage::None) ? m_activePipeline : sync::PipelineStage::Graphics;
    if (m_contexts[target] == nullptr)
    {
        activatePipelines(target);
    }
    m_computeContext = m_contexts[target];
    GP_CHECKF(m_computeContext != nullptr, "CommandListBase::getComputeContext() has no bound IComputeContext.");
    return *m_computeContext;
}

void CommandListBase::setContext(const sync::PipelineStage pipeline, IComputeContext* const context) noexcept
{
    GP_CHECKF(sync::isSinglePipelineStage(pipeline), "setContext requires a single-bit PipelineStage.");
    m_contexts[pipeline] = context;
    if (pipeline == sync::PipelineStage::Graphics)
    {
        m_graphicsContext = static_cast<ICommandContext*>(context);
    }
    if (m_activePipeline == pipeline)
    {
        m_computeContext = context;
    }
    if (context != nullptr)
    {
        m_activePipelines |= pipeline;
    }
}

void CommandListBase::finishRecording() noexcept
{
    m_isFinished = true;
}

void CommandListBase::execute() noexcept
{
    if (m_root == nullptr)
    {
        reset();
        return;
    }

    const sync::PipelineStage pipelinesToActivate =
        (m_activePipelines != sync::PipelineStage::None) ? m_activePipelines : m_initialPipeline;
    activatePipelines(
        (pipelinesToActivate != sync::PipelineStage::None) ? pipelinesToActivate : sync::PipelineStage::Graphics
    );

    const sync::PipelineStage savedPipeline = m_activePipeline;
    const GPUMask savedGPUMask = m_persistentState.currentGPUMask;
    const sync::PipelineStage startPipeline =
        (m_initialPipeline != sync::PipelineStage::None) ? m_initialPipeline : sync::PipelineStage::Graphics;
    m_activePipeline = startPipeline;
    m_computeContext = m_contexts[startPipeline];
    m_persistentState.currentGPUMask = m_persistentState.initialGPUMask;
    m_isExecuting = true;

    for (IComputeContext* const ctx: m_contexts)
    {
        if (ctx != nullptr)
        {
            ctx->setExecutingCommandList(this);
            ctx->setGPUMask(m_persistentState.initialGPUMask);
        }
    }

    CommandBase* current = m_root;
    while (current != nullptr)
    {
        CommandBase* const next = current->next;
        current->executeAndDestruct(*this);
        current = next;
    }

    for (IComputeContext* const ctx: m_contexts)
    {
        if (ctx != nullptr)
        {
            ctx->setExecutingCommandList(nullptr);
        }
    }

    m_isExecuting = false;
    m_activePipeline = savedPipeline;
    m_initialPipeline = savedPipeline;
    m_persistentState.currentGPUMask = savedGPUMask;
    m_persistentState.initialGPUMask = savedGPUMask;
    m_root = nullptr;
    m_commandLink = &m_root;
    m_numCommands = 0u;
    reset();
}

void CommandListBase::reset() noexcept
{
    // Destroy any unexecuted packets cleanly if reset() was called without execute().
    if (m_root != nullptr && !m_isExecuting)
    {
        CommandBase* current = m_root;
        while (current != nullptr)
        {
            CommandBase* const next = current->next;
            current->destruct();
            current = next;
        }
    }

    releaseBatchedShaderParameterAllocators();
    m_scratchShaderParameters.reset();
    m_scratchParameterAllocator.reset();
    m_scratchShaderUnbinds.reset();

    m_root = nullptr;
    m_commandLink = &m_root;
    m_numCommands = 0u;
    m_isFinished = false;
    m_initialPipeline = m_activePipeline;
    m_persistentState.initialGPUMask = m_persistentState.currentGPUMask;
    m_memManager.reset();
}

shader::BatchedShaderParameters& CommandListBase::getScratchShaderParameters() noexcept
{
    if (m_scratchShaderParameters.hasParameters())
    {
        m_scratchShaderParameters.reset();
    }
    return m_scratchShaderParameters;
}

shader::BatchedShaderUnbinds& CommandListBase::getScratchShaderUnbinds() noexcept
{
    if (m_scratchShaderUnbinds.hasParameters())
    {
        m_scratchShaderUnbinds.reset();
    }
    return m_scratchShaderUnbinds;
}

shader::BatchedShaderParametersAllocator* CommandListBase::createBatchedShaderParameterAllocator() noexcept
{
    void* const mem = alloc(
        sizeof(shader::BatchedShaderParametersAllocator),
        static_cast<gp::UInt32>(alignof(shader::BatchedShaderParametersAllocator))
    );
    shader::BatchedShaderParametersAllocator* const allocator =
        ::new (mem) shader::BatchedShaderParametersAllocator(&getSharedPagePool());
    allocator->next = m_parameterAllocatorHead;
    m_parameterAllocatorHead = allocator;
    return allocator;
}

void CommandListBase::releaseBatchedShaderParameterAllocators() noexcept
{
    shader::BatchedShaderParametersAllocator* current = m_parameterAllocatorHead;
    m_parameterAllocatorHead = nullptr;
    while (current != nullptr)
    {
        shader::BatchedShaderParametersAllocator* const next = current->next;
        current->~BatchedShaderParametersAllocator();
        current = next;
    }
}

resource::BufferInitializer
    CommandListBase::createBufferInitializer(const resource::BufferCreateDesc& createDesc) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::createBufferInitializer requires a valid DynamicRHI.");
    return rhi->createBufferInitializer(*this, createDesc);
}

resource::BufferRef CommandListBase::createBuffer(const resource::BufferCreateDesc& createDesc) noexcept
{
    resource::BufferInitializer initializer = createBufferInitializer(createDesc);
    return initializer.finalize();
}

resource::TextureInitializer
    CommandListBase::createTextureInitializer(const resource::TextureCreateDesc& createDesc) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::createTextureInitializer requires a valid DynamicRHI.");
    if (createDesc.initialState == sync::ResourceAccess::Unknown)
    {
        resource::TextureCreateDesc resolvedDesc = createDesc;
        resolvedDesc.determineInitialState();
        return rhi->createTextureInitializer(*this, resolvedDesc);
    }
    return rhi->createTextureInitializer(*this, createDesc);
}

resource::TextureRef CommandListBase::createTexture(const resource::TextureCreateDesc& createDesc) noexcept
{
    resource::TextureInitializer initializer = createTextureInitializer(createDesc);
    return initializer.finalize();
}

resource::TextureReferenceRef
    CommandListBase::createTextureReference(resource::Texture* const referencedTexture) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::createTextureReference requires a valid DynamicRHI.");
    return rhi->createTextureReference(*this, referencedTexture);
}

void CommandListBase::updateTextureReference(
    resource::TextureReference* const textureReference, resource::Texture* const newTexture
) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::updateTextureReference requires a valid DynamicRHI.");
    rhi->updateTextureReference(*this, textureReference, newTexture);
}

view::ShaderResourceViewRef
    CommandListBase::createShaderResourceView(resource::Buffer* const buffer, const view::ViewDesc& desc) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::createShaderResourceView requires a valid DynamicRHI.");
    return rhi->createShaderResourceView(*this, buffer, desc);
}

view::ShaderResourceViewRef
    CommandListBase::createShaderResourceView(resource::Texture* const texture, const view::ViewDesc& desc) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::createShaderResourceView requires a valid DynamicRHI.");
    return rhi->createShaderResourceView(*this, texture, desc);
}

view::UnorderedAccessViewRef
    CommandListBase::createUnorderedAccessView(resource::Buffer* const buffer, const view::ViewDesc& desc) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::createUnorderedAccessView requires a valid DynamicRHI.");
    return rhi->createUnorderedAccessView(*this, buffer, desc);
}

view::UnorderedAccessViewRef
    CommandListBase::createUnorderedAccessView(resource::Texture* const texture, const view::ViewDesc& desc) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::createUnorderedAccessView requires a valid DynamicRHI.");
    return rhi->createUnorderedAccessView(*this, texture, desc);
}

void CommandListBase::updateUniformBuffer(
    resource::UniformBuffer* const uniformBuffer, const void* const contents
) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::updateUniformBuffer requires a valid DynamicRHI.");
    rhi->updateUniformBuffer(*this, uniformBuffer, contents);
}

void CommandListBase::updateStreamSourceSlot(
    resource::StreamSourceSlot* const slot, resource::Buffer* const buffer, const gp::UInt32 offset
) noexcept
{
    if (slot == nullptr)
    {
        return;
    }

    if (isBottomOfPipe())
    {
        slot->update(buffer, offset);
        return;
    }

    resource::StreamSourceSlotRef slotRef(slot);
    resource::BufferRef bufferRef(buffer);
    enqueueLambda(
        [slotRef = std::move(slotRef), bufferRef = std::move(bufferRef), offset](CommandListBase&) noexcept -> void
    {
        slotRef->update(bufferRef.get(), offset);
    }
    );
}

void CommandListBase::updateTexture2D(
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion2D& updateRegion,
    const gp::UInt32 sourcePitch,
    const gp::UInt8* const sourceData
) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::updateTexture2D requires a valid DynamicRHI.");
    rhi->updateTexture2D(*this, texture, mipIndex, updateRegion, sourcePitch, sourceData);
}

void CommandListBase::updateTexture3D(
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion,
    const gp::UInt32 sourceRowPitch,
    const gp::UInt32 sourceDepthPitch,
    const gp::UInt8* const sourceData
) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::updateTexture3D requires a valid DynamicRHI.");
    rhi->updateTexture3D(*this, texture, mipIndex, updateRegion, sourceRowPitch, sourceDepthPitch, sourceData);
}

void CommandListBase::replaceResources(const gp::ConstVectorView<resource::ResourceReplaceInfo> infos) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::replaceResources requires a valid DynamicRHI.");
    rhi->replaceResources(*this, infos);
}

void CommandListBase::bindDebugLabelName(resource::Texture* const texture, const char* const name) noexcept
{
    if (texture != nullptr && name != nullptr)
    {
        texture->setName(gp::Name(name));
    }
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->bindDebugLabelName(*this, texture, name);
    }
}

void CommandListBase::bindDebugLabelName(resource::Buffer* const buffer, const char* const name) noexcept
{
    if (buffer != nullptr && name != nullptr)
    {
        buffer->setName(gp::Name(name));
    }
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->bindDebugLabelName(*this, buffer, name);
    }
}

void CommandListBase::bindDebugLabelName(view::UnorderedAccessView* const uav, const char* const name) noexcept
{
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->bindDebugLabelName(*this, uav, name);
    }
}

void* CommandListBase::lockBuffer(
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 size,
    const resource::ResourceLockMode lockMode
) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::lockBuffer requires a valid DynamicRHI.");
    return rhi->lockBuffer(*this, buffer, offset, size, lockMode);
}

void CommandListBase::unlockBuffer(resource::Buffer* const buffer) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListBase::unlockBuffer requires a valid DynamicRHI.");
    rhi->unlockBuffer(*this, buffer);
}

void CommandListBase::transitionInternal(
    const gp::ConstVectorView<sync::TransitionInfo> infos,
    const sync::PipelineStage srcPipelines,
    const sync::PipelineStage dstPipelines,
    const sync::TransitionCreateFlags flags
) noexcept
{
    if (infos.isEmpty())
    {
        return;
    }

    sync::PipelineStage effectiveSrc = srcPipelines;
    sync::PipelineStage effectiveDst = dstPipelines;
    if (!getRHIGlobals().supportsAsyncCompute)
    {
        effectiveSrc = sync::PipelineStage::Graphics;
        effectiveDst = sync::PipelineStage::Graphics;
    }

    const bool isSinglePipe = (effectiveSrc == effectiveDst) && sync::isSinglePipelineStage(effectiveSrc);
    const sync::TransitionCreateFlags effectiveFlags =
        isSinglePipe ? (flags | sync::TransitionCreateFlags::NoSplit) : flags;

    const sync::Transition* const transition =
        createTransition(sync::TransitionCreateInfo(effectiveSrc, effectiveDst, effectiveFlags, infos));
    if (transition == nullptr)
    {
        return;
    }

    if (isSinglePipe)
    {
        const sync::PipelineStage previousPipeline = switchPipeline(effectiveSrc);
        if (isBottomOfPipe())
        {
            IComputeContext& ctx = getComputeContext();
            const gp::ConstVectorView<const sync::Transition*> span(&transition, 1u);
            ctx.beginTransitions(span);
            ctx.endTransitions(span);
            sync::TransitionController::markBegin(transition, effectiveSrc);
            sync::TransitionController::markEnd(transition, effectiveDst);
        }
        else
        {
            emplaceCommand<CommandResourceTransition>(transition, effectiveSrc);
        }
        static_cast<void>(switchPipeline(previousPipeline));
        return;
    }

    const sync::PipelineStage previousPipeline = m_activePipeline;
    const sync::PipelineStage kStages[sync::kPipelineStageCount] = { sync::PipelineStage::Graphics,
                                                                     sync::PipelineStage::AsyncCompute };

    for (const sync::PipelineStage stage: kStages)
    {
        if ((effectiveSrc & stage) != sync::PipelineStage::None)
        {
            static_cast<void>(switchPipeline(stage));
            const gp::ConstVectorView<const sync::Transition*> span(&transition, 1u);
            if (isBottomOfPipe())
            {
                getComputeContext().beginTransitions(span);
                sync::TransitionController::markBegin(transition, stage);
            }
            else
            {
                emplaceCommand<CommandBeginTransitions>(allocArray(span));
            }
        }
    }

    for (const sync::PipelineStage stage: kStages)
    {
        if ((effectiveDst & stage) != sync::PipelineStage::None)
        {
            static_cast<void>(switchPipeline(stage));
            const gp::ConstVectorView<const sync::Transition*> span(&transition, 1u);
            if (isBottomOfPipe())
            {
                getComputeContext().endTransitions(span);
                sync::TransitionController::markEnd(transition, stage);
            }
            else
            {
                emplaceCommand<CommandEndTransitions>(allocArray(span));
            }
        }
    }

    static_cast<void>(switchPipeline(previousPipeline));
}

void CommandListBase::detachContexts() noexcept
{
    for (IComputeContext*& ctx: m_contexts)
    {
        ctx = nullptr;
    }
    m_graphicsContext = nullptr;
    m_computeContext = nullptr;
    m_activePipelines = sync::PipelineStage::None;
}

}   // namespace gp::rhi::command
