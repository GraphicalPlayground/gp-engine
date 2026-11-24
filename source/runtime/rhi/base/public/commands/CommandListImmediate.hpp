// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandForward.hpp"       // IWYU pragma: keep
#include "commands/CommandList.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "presentation/PresentationForward.hpp"
#include "resources/TextureInitializer.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include <type_traits>
#include <utility>

namespace gp::rhi::command
{

/// @brief Render-thread immediate command list owning viewport presentation, synchronous readbacks, and flushing.
/// @details Adds zero data members beyond `CommandListBase`. Calls into `CommandListExecutor` when flushing queued
///          commands, submitting deferred command lists, or draining deferred resource deletion batches.
class GP_RHI_API CommandListImmediate : public CommandList
{
public:
    /// @brief Constructs the immediate command list (`isImmediate() == true`).
    /// @param[in] gpuMask Initial multi-GPU execution mask.
    explicit CommandListImmediate(const GPUMask gpuMask = GPUMask::all()) noexcept
        : CommandList(gpuMask, true)
    {}

    CommandListImmediate(const CommandListImmediate&) = delete;
    CommandListImmediate& operator=(const CommandListImmediate&) = delete;
    CommandListImmediate(CommandListImmediate&&) = delete;
    CommandListImmediate& operator=(CommandListImmediate&&) = delete;

public:
    /// @brief Appends a lambda receiving `CommandListImmediate&` on replay (or executes immediately in bypass mode).
    /// @tparam InLambdaType Movable callable invocable with `(CommandListImmediate&)`.
    /// @param[in,out] inLambda Callable to execute.
    template <typename InLambdaType>
    void enqueueLambda(InLambdaType&& inLambda) noexcept
    {
        if (isBottomOfPipe())
        {
            inLambda(*this);
            return;
        }
        using CmdType = LambdaCommand<CommandListImmediate, std::decay_t<InLambdaType>>;
        emplaceCommand<CmdType>(std::forward<InLambdaType>(inLambda));
    }

    /// @brief Flushes queued commands and optionally submits to the GPU or drains deferred resource deletions.
    /// @param[in] type Flush level (`DispatchToRHIThread`, `FlushRHIThread`, `FlushRHIThreadFlushResources`, etc.).
    /// @param[in] submitFlags Submission bitflags forwarded to `CommandListExecutor::submit()`.
    void immediateFlush(
        ImmediateFlushType type = ImmediateFlushType::FlushRHIThread, SubmitFlags submitFlags = SubmitFlags::None
    ) noexcept;

    /// @brief Queues one or more already-finished deferred command lists for execution on the immediate timeline.
    /// @param[in] commandLists Span of deferred `CommandListBase*` instances to replay and finalize.
    void queueAsyncCommandListSubmit(gp::ConstVectorView<CommandListBase*> commandLists) noexcept;

    /// @brief Flushes pending commands and blocks until GPU execution completes across active queues.
    /// @param[in] flushResources True to also drain deferred resource deletion queues.
    void blockUntilGPUIdle(bool flushResources = false) noexcept;

    /// @brief Begins rendering into `viewport` and optionally binds `renderTarget` as the initial target.
    /// @param[in] viewport Target presentation viewport.
    /// @param[in] renderTarget Optional backbuffer texture override.
    void beginDrawingViewport(presentation::Viewport* viewport, resource::Texture* renderTarget = nullptr) noexcept;

    /// @brief Ends rendering into `viewport` and optionally presents the swapchain.
    /// @param[in] viewport Target presentation viewport.
    /// @param[in] present True to present the swapchain backbuffer.
    /// @param[in] lockToVsync True to synchronize presentation to vertical blank.
    void endDrawingViewport(presentation::Viewport* viewport, bool present, bool lockToVsync) noexcept;

    /// @brief Marks the start of a new rendering frame on the RHI timeline.
    void beginFrame() noexcept;

    /// @brief Marks the end of the current rendering frame, flushing commands and deferred deletions.
    void endFrame() noexcept;

    /// @brief Advances the RHI frame counter and notifies the backend of a frame boundary.
    void advanceFrameForGetViewportBackBuffer(presentation::Viewport* viewport) noexcept;

    /// @brief Reads back 8-bit BGRA pixels from `texture` within `rect` into `outData`.
    /// @param[in] texture Source GPU texture.
    /// @param[in] rect Sub-rectangle in texels to read.
    /// @param[out] outData Destination vector receiving `rect.width() * rect.height()` colors.
    /// @param[in] flags Mip, slice, face, and conversion flags.
    void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::Color>& outData,
        const resource::ReadSurfaceDataFlags& flags = {}
    ) noexcept;

    /// @brief Reads back 32-bit linear float RGBA pixels from `texture` within `rect` into `outData`.
    /// @param[in] texture Source GPU texture.
    /// @param[in] rect Sub-rectangle in texels to read.
    /// @param[out] outData Destination vector receiving linear colors.
    /// @param[in] flags Mip, slice, face, and conversion flags.
    void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::LinearColor>& outData,
        const resource::ReadSurfaceDataFlags& flags = {}
    ) noexcept;

    /// @brief Reads back 16-bit half-float RGBA pixels from `texture` within `rect` into `outData`.
    /// @param[in] texture Source GPU texture.
    /// @param[in] rect Sub-rectangle in texels to read.
    /// @param[out] outData Destination vector receiving half-float colors.
    /// @param[in] cubeFace Target cubemap face.
    /// @param[in] arrayIndex Target array slice.
    /// @param[in] mipIndex Target mip level.
    void readSurfaceFloatData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<resource::Float16Color>& outData,
        resource::CubeFace cubeFace = resource::CubeFace::PosX,
        gp::Int32 arrayIndex = 0,
        gp::Int32 mipIndex = 0
    ) noexcept;

    /// @brief Reads the result of a previously ended render query.
    /// @param[in] renderQuery Target render query.
    /// @param[out] outResult Destination query result (sample count or timestamp).
    /// @param[in] wait True to flush and block until the query result is ready.
    /// @param[in] gpuIndex Multi-GPU node index (`0` default).
    /// @return True when the query result is available and written to `outResult`.
    [[nodiscard]] bool getRenderQueryResult(
        sync::RenderQuery* renderQuery, gp::UInt64& outResult, bool wait, gp::UInt32 gpuIndex = 0u
    ) noexcept;

    /// @brief Locks a texture subresource for CPU read or write access after flushing pending commands if needed.
    /// @param[in] args Subresource coordinates and lock mode.
    /// @return Mapped pointer, row pitch, and byte count.
    [[nodiscard]] resource::TextureLockResult lockTexture(const resource::TextureLockArgs& args) noexcept;

    /// @brief Unlocks a texture subresource previously locked via `lockTexture()`.
    /// @param[in] args Matching subresource lock arguments.
    void unlockTexture(const resource::TextureLockArgs& args) noexcept;

    /// @brief Allocates a staged CPU upload buffer for updating a 3D volume texture region.
    /// @param[in] texture Target 3D texture.
    /// @param[in] mipIndex Target mip level.
    /// @param[in] updateRegion Sub-box in texels.
    /// @return Staged upload buffer descriptor.
    [[nodiscard]] resource::UpdateTexture3DData beginUpdateTexture3D(
        resource::Texture* texture, gp::UInt32 mipIndex, const resource::UpdateTextureRegion3D& updateRegion
    ) noexcept;

    /// @brief Commits a staged 3D texture upload allocated by `beginUpdateTexture3D()`.
    /// @param[in,out] updateData Upload descriptor populated by the caller.
    void endUpdateTexture3D(resource::UpdateTexture3DData& updateData) noexcept;

    /// @brief Maps a CPU-readable staging buffer after flushing if `fence` is null or still pending.
    /// @param[in] stagingBuffer Target staging buffer.
    /// @param[in] fence Optional fence signaling completion of the GPU copy.
    /// @param[in] offset Byte offset into the staging buffer.
    /// @param[in] sizeRHI Number of bytes to map.
    /// @return Mapped read-only CPU pointer, or `nullptr` if unavailable.
    [[nodiscard]] void* lockStagingBuffer(
        resource::StagingBuffer* stagingBuffer, sync::GPUFence* fence, gp::UInt32 offset, gp::UInt32 sizeRHI
    ) noexcept;

    /// @brief Unmaps a staging buffer previously locked via `lockStagingBuffer()`.
    /// @param[in] stagingBuffer Target staging buffer.
    void unlockStagingBuffer(resource::StagingBuffer* stagingBuffer) noexcept;

    /// @brief Acquires an optional backend-native upload context for batched staging transfers.
    /// @return Non-owning upload context pointer, or `nullptr` if unsupported.
    [[nodiscard]] IUploadContext* acquireUploadContext() noexcept;
};

static_assert(
    sizeof(CommandListImmediate) == sizeof(CommandListBase),
    "CommandListImmediate must not add data members beyond CommandListBase."
);

/// @brief RAII scope switching a command list's active pipeline and restoring the previous pipeline on exit.
class CommandListScopedPipeline final
{
public:
    /// @brief Switches `inCmdList` to `targetPipeline` and remembers the previous pipeline.
    /// @param[in,out] inCmdList Target command list.
    /// @param[in] targetPipeline Single-bit pipeline stage to activate.
    CommandListScopedPipeline(CommandListBase& inCmdList, const sync::PipelineStage targetPipeline) noexcept
        : m_cmdList(inCmdList)
        , m_previousPipeline(inCmdList.switchPipeline(targetPipeline))
    {}

    /// @brief Restores the command list to its previous active pipeline.
    ~CommandListScopedPipeline() noexcept
    {
        static_cast<void>(m_cmdList.switchPipeline(m_previousPipeline));
    }

    CommandListScopedPipeline(const CommandListScopedPipeline&) = delete;
    CommandListScopedPipeline& operator=(const CommandListScopedPipeline&) = delete;
    CommandListScopedPipeline(CommandListScopedPipeline&&) = delete;
    CommandListScopedPipeline& operator=(CommandListScopedPipeline&&) = delete;

private:
    CommandListBase& m_cmdList;
    sync::PipelineStage m_previousPipeline{ sync::PipelineStage::None };
};

/// @brief RAII guard ensuring `inCmdList` is switched to `Graphics` (or `targetPipeline`) only if currently unbound.
class CommandListScopedPipelineGuard final
{
public:
    /// @brief Activates `targetPipeline` if `inCmdList.getPipeline() == PipelineStage::None`.
    /// @param[in,out] inCmdList Target command list.
    /// @param[in] targetPipeline Fallback pipeline stage (defaults to `Graphics`).
    explicit CommandListScopedPipelineGuard(
        CommandListBase& inCmdList, const sync::PipelineStage targetPipeline = sync::PipelineStage::Graphics
    ) noexcept
        : m_cmdList(inCmdList)
    {
        if (m_cmdList.getPipeline() == sync::PipelineStage::None)
        {
            static_cast<void>(m_cmdList.switchPipeline(targetPipeline));
            m_switched = true;
        }
    }

    /// @brief Restores `PipelineStage::None` only when this guard performed the initial switch.
    ~CommandListScopedPipelineGuard() noexcept
    {
        if (m_switched)
        {
            static_cast<void>(m_cmdList.switchPipeline(sync::PipelineStage::None));
        }
    }

    CommandListScopedPipelineGuard(const CommandListScopedPipelineGuard&) = delete;
    CommandListScopedPipelineGuard& operator=(const CommandListScopedPipelineGuard&) = delete;
    CommandListScopedPipelineGuard(CommandListScopedPipelineGuard&&) = delete;
    CommandListScopedPipelineGuard& operator=(CommandListScopedPipelineGuard&&) = delete;

private:
    CommandListBase& m_cmdList;
    bool m_switched{ false };
};

/// @brief RAII scope overriding a `ComputeCommandList`'s active `GPUMask` and restoring it on exit.
class ScopedGPUMask final
{
public:
    /// @brief Applies `gpuMask` to `inCmdList` if it differs from the current mask.
    /// @param[in,out] inCmdList Target command list.
    /// @param[in] gpuMask Temporary GPU mask.
    ScopedGPUMask(ComputeCommandList& inCmdList, const GPUMask gpuMask) noexcept
        : m_cmdList(inCmdList)
        , m_previousMask(inCmdList.getGPUMask())
    {
        if (m_previousMask != gpuMask)
        {
            m_cmdList.setGPUMask(gpuMask);
            m_changed = true;
        }
    }

    /// @brief Restores the original GPU mask when modified.
    ~ScopedGPUMask() noexcept
    {
        if (m_changed)
        {
            m_cmdList.setGPUMask(m_previousMask);
        }
    }

    ScopedGPUMask(const ScopedGPUMask&) = delete;
    ScopedGPUMask& operator=(const ScopedGPUMask&) = delete;
    ScopedGPUMask(ScopedGPUMask&&) = delete;
    ScopedGPUMask& operator=(ScopedGPUMask&&) = delete;

private:
    ComputeCommandList& m_cmdList;
    GPUMask m_previousMask{ GPUMask::all() };
    bool m_changed{ false };
};

/// @brief RAII scope pushing temporary static uniform buffer bindings and restoring the prior set on exit.
class ScopedUniformBufferStaticBindings final
{
public:
    /// @brief Applies `newBindings` to `inCmdList` and snapshots the prior static uniform buffer bindings.
    /// @param[in,out] inCmdList Target command list.
    /// @param[in] newBindings Temporary static uniform buffer binding set.
    ScopedUniformBufferStaticBindings(
        ComputeCommandList& inCmdList, const shader::UniformBufferStaticBindings& newBindings
    ) noexcept
        : m_cmdList(inCmdList)
        , m_previousBindings(inCmdList.getStaticUniformBuffers())
    {
        m_cmdList.setStaticUniformBuffers(newBindings);
    }

    /// @brief Restores the previous static uniform buffer bindings.
    ~ScopedUniformBufferStaticBindings() noexcept
    {
        m_cmdList.setStaticUniformBuffers(m_previousBindings);
    }

    ScopedUniformBufferStaticBindings(const ScopedUniformBufferStaticBindings&) = delete;
    ScopedUniformBufferStaticBindings& operator=(const ScopedUniformBufferStaticBindings&) = delete;
    ScopedUniformBufferStaticBindings(ScopedUniformBufferStaticBindings&&) = delete;
    ScopedUniformBufferStaticBindings& operator=(ScopedUniformBufferStaticBindings&&) = delete;

private:
    ComputeCommandList& m_cmdList;
    shader::UniformBufferStaticBindings m_previousBindings{};
};

/// @brief RAII scope that flushes an immediate command list upon entry and marks it bottom-of-pipe for direct backend
/// calls.
class CommandListScopedFlushAndExecute final
{
public:
    /// @brief Flushes `inCmdList` with `inFlushType` and marks `m_isExecuting = true` for the scope duration.
    /// @param[in,out] inCmdList Immediate command list to flush.
    /// @param[in] inFlushType Flush level invoked on entry.
    explicit CommandListScopedFlushAndExecute(
        CommandListImmediate& inCmdList, const ImmediateFlushType inFlushType = ImmediateFlushType::FlushRHIThread
    ) noexcept
        : m_cmdList(inCmdList)
        , m_wasExecuting(inCmdList.m_isExecuting)
    {
        m_cmdList.immediateFlush(inFlushType);
        m_cmdList.m_isExecuting = true;
    }

    /// @brief Restores `m_isExecuting` to its prior value on scope exit.
    ~CommandListScopedFlushAndExecute() noexcept
    {
        m_cmdList.m_isExecuting = m_wasExecuting;
    }

    CommandListScopedFlushAndExecute(const CommandListScopedFlushAndExecute&) = delete;
    CommandListScopedFlushAndExecute& operator=(const CommandListScopedFlushAndExecute&) = delete;
    CommandListScopedFlushAndExecute(CommandListScopedFlushAndExecute&&) = delete;
    CommandListScopedFlushAndExecute& operator=(CommandListScopedFlushAndExecute&&) = delete;

private:
    CommandListImmediate& m_cmdList;
    bool m_wasExecuting{ false };
};

/// @brief RAII scope extending deferred resource lifetimes on the render thread and flushing deletes when the outermost
///        scope exits.
class CommandListScopedExtendResourceLifetime final
{
public:
    /// @brief Acquires one lifetime-extension reference via `ResourceDeletionQueue::addLifetimeRef()`.
    /// @param[in,out] inCmdList Immediate command list used to dispatch deletion flush on final release.
    explicit CommandListScopedExtendResourceLifetime(CommandListImmediate& inCmdList) noexcept
        : m_cmdList(inCmdList)
    {
        resource::ResourceDeletionQueue::addLifetimeRef(1);
    }

    /// @brief Releases the lifetime-extension reference and triggers a deletion flush if the count reached zero.
    ~CommandListScopedExtendResourceLifetime() noexcept
    {
        if (resource::ResourceDeletionQueue::releaseLifetimeRef(1))
        {
            m_cmdList.immediateFlush(ImmediateFlushType::DispatchToRHIThread, SubmitFlags::DeleteResources);
        }
    }

    CommandListScopedExtendResourceLifetime(const CommandListScopedExtendResourceLifetime&) = delete;
    CommandListScopedExtendResourceLifetime& operator=(const CommandListScopedExtendResourceLifetime&) = delete;
    CommandListScopedExtendResourceLifetime(CommandListScopedExtendResourceLifetime&&) = delete;
    CommandListScopedExtendResourceLifetime& operator=(CommandListScopedExtendResourceLifetime&&) = delete;

private:
    CommandListImmediate& m_cmdList;
};

}   // namespace gp::rhi::command
