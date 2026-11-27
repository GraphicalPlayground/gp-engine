// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"     // IWYU pragma: keep
#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "commands/CommandPacket.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/RenderPassInfo.hpp"       // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/Breadcrumbs.hpp"
#include "memory/allocators/PagedLinearAllocator.hpp"
#include "memory/Memory.hpp"                   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/StreamSourceSlot.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/TextureReference.hpp"
#include "resources/UniformBuffer.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/Transition.hpp"            // IWYU pragma: keep
#include "sync/TransitionDesc.hpp"
#include "views/ShaderResourceView.hpp"   // IWYU pragma: keep
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include <new>   // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace gp::rhi::command
{

/// @brief Base class for all RHI command lists storing the linear command packet arena, context bindings, and state.
/// @details Derived classes (`ComputeCommandList`, `CommandList`, `CommandListImmediate`) add zero data members so any
///          command list can be cast safely across the hierarchy without slicing or hidden storage overhead. Commands
///          either execute directly into the bound `ICommandContext`/`IComputeContext` (bypass mode) or allocate
///          intrusive packets inside `m_memManager` (`gp::memory::PagedLinearAllocator`) for deferred replay in
///          `execute()`.
class GP_RHI_API CommandListBase
{
    friend class CommandListExecutor;
    friend class CommandListScopedFlushAndExecute;
    friend struct CommandSetGPUMask;
    friend struct CommandSwitchPipeline;
    template <typename InLambdaType>
    friend struct LambdaCommandMultiPipe;

public:
    /// @brief Shared page pool supplying memory pages to every command list arena and shader parameter allocator.
    /// @return Reference to the process-wide command-list `gp::memory::PagePool`.
    [[nodiscard]] static gp::memory::PagePool& getSharedPagePool() noexcept;

public:
    /// @brief Constructs a command list bound to `gpuMask` and optional `immediate` mode flag.
    /// @param[in] gpuMask Initial multi-GPU execution mask.
    /// @param[in] isImmediateList True only when constructing the singleton `CommandListImmediate`.
    explicit CommandListBase(GPUMask gpuMask = GPUMask::all(), bool isImmediateList = false) noexcept;

    CommandListBase(const CommandListBase&) = delete;
    CommandListBase& operator=(const CommandListBase&) = delete;
    CommandListBase(CommandListBase&&) = delete;
    CommandListBase& operator=(CommandListBase&&) = delete;

    /// @brief Destroys the command list, releasing any chained parameter allocators and arena pages.
    ~CommandListBase() noexcept;

public:
    /// @brief Allocates `allocSize` bytes aligned to `alignment` inside this command list's linear packet arena.
    /// @param[in] allocSize Byte size to allocate.
    /// @param[in] alignment Power-of-two byte alignment.
    /// @return Pointer to uninitialized arena memory valid until `reset()`.
    [[nodiscard]] void* alloc(gp::USize allocSize, gp::UInt32 alignment) noexcept;

    /// @brief Copies `sizeInBytes` from `inSourceData` into arena-backed memory aligned to `alignment`.
    /// @param[in] inSourceData Source pointer (may be `nullptr` when `sizeInBytes == 0`).
    /// @param[in] sizeInBytes Number of bytes to copy.
    /// @param[in] alignment Power-of-two byte alignment.
    /// @return Pointer to the arena copy, or `nullptr` if `sizeInBytes == 0`.
    [[nodiscard]] void* allocCopy(const void* inSourceData, gp::USize sizeInBytes, gp::UInt32 alignment) noexcept;

    /// @brief Copies a contiguous span of trivially copyable elements into the command list arena.
    /// @tparam InElementType Trivially copyable element type.
    /// @param[in] elements Source element span.
    /// @return Arena-backed `gp::ConstVectorView<InElementType>` valid until `reset()`.
    template <typename InElementType>
    [[nodiscard]] gp::ConstVectorView<InElementType>
        allocArray(const gp::ConstVectorView<InElementType> elements) noexcept
    {
        static_assert(
            std::is_trivially_copyable_v<InElementType>,
            "CommandListBase::allocArray requires trivially copyable elements."
        );
        if (elements.isEmpty())
        {
            return gp::ConstVectorView<InElementType>{};
        }

        InElementType* const dst = static_cast<InElementType*>(allocCopy(
            elements.data(), elements.size() * sizeof(InElementType), static_cast<gp::UInt32>(alignof(InElementType))
        ));
        return gp::ConstVectorView<InElementType>(dst, elements.size());
    }

    /// @brief Allocates and default-constructs a single `InType` object inside the command list arena.
    /// @tparam InType Trivially destructible type allocated in the linear arena.
    /// @tparam InArgs Constructor argument types.
    /// @param[in] args Arguments forwarded to `InType`'s constructor.
    /// @return Pointer to the arena-owned object valid until `reset()`.
    template <typename InType, typename... InArgs>
    [[nodiscard]] InType* alloc(InArgs&&... args) noexcept
    {
        void* const mem = alloc(sizeof(InType), static_cast<gp::UInt32>(alignof(InType)));
        return ::new (mem) InType(std::forward<InArgs>(args)...);
    }

    /// @brief Copies a null-terminated string into the command list arena.
    /// @param[in] text Source string view (returns `""` if empty).
    /// @return Arena-owned null-terminated C string valid until `reset()`.
    [[nodiscard]] const char* allocString(gp::StringView text) noexcept;

    /// @brief Allocates raw storage for a command packet and appends it to the intrusive command stream.
    /// @param[in] allocSize Byte size of the concrete command packet.
    /// @param[in] alignment Required alignment of the concrete command packet.
    /// @return Pointer where the caller placement-constructs the `CommandBase`-derived packet.
    [[nodiscard]] void* allocCommand(gp::USize allocSize, gp::UInt32 alignment) noexcept;

    /// @brief Allocates and placement-constructs a concrete `InCmdType` packet at the tail of the command stream.
    /// @tparam InCmdType Concrete `Command<InCmdType>` subclass.
    /// @tparam InArgs Constructor argument types.
    /// @param[in] args Arguments forwarded to `InCmdType`'s constructor.
    /// @return Pointer to the newly constructed command packet.
    template <typename InCmdType, typename... InArgs>
    InCmdType* emplaceCommand(InArgs&&... args) noexcept
    {
        void* const mem = allocCommand(sizeof(InCmdType), static_cast<gp::UInt32>(alignof(InCmdType)));
        return ::new (mem) InCmdType(std::forward<InArgs>(args)...);
    }

    /// @brief Appends a lambda command packet that executes `inLambda(*this)` during replay (or immediately in bypass).
    /// @tparam InLambdaType Movable callable invocable with `(CommandListBase&)`.
    /// @param[in,out] inLambda Callable to execute.
    template <typename InLambdaType>
    void enqueueLambda(InLambdaType&& inLambda) noexcept
    {
        if (isBottomOfPipe())
        {
            inLambda(*this);
            return;
        }
        using CmdType = LambdaCommand<CommandListBase, std::decay_t<InLambdaType>>;
        emplaceCommand<CmdType>(std::forward<InLambdaType>(inLambda));
    }

    /// @brief Appends a lambda command packet without emitting any implicit breadcrumb marker.
    /// @tparam InLambdaType Movable callable invocable with `(CommandListBase&)`.
    /// @param[in,out] inLambda Callable to execute.
    template <typename InLambdaType>
    void enqueueLambdaNoMarker(InLambdaType&& inLambda) noexcept
    {
        enqueueLambda(std::forward<InLambdaType>(inLambda));
    }

    /// @brief Appends a multi-pipe lambda activating `pipelines` and receiving active contexts in a `PipelineArray`.
    /// @tparam InLambdaType Movable callable invocable with `(sync::PipelineArray<IComputeContext*>&)`.
    /// @param[in] pipelines Bitmask of pipeline stages whose contexts must be active for `inLambda`.
    /// @param[in,out] inLambda Callable to execute across active contexts.
    template <typename InLambdaType>
    void enqueueLambdaMultiPipe(const sync::PipelineStage pipelines, InLambdaType&& inLambda) noexcept
    {
        activatePipelines(pipelines);
        if (isBottomOfPipe())
        {
            inLambda(m_contexts);
            return;
        }
        using CmdType = LambdaCommandMultiPipe<std::decay_t<InLambdaType>>;
        emplaceCommand<CmdType>(std::forward<InLambdaType>(inLambda));
    }

    /// @brief Convenience overload activating all pipelines (`Graphics | AsyncCompute`) for `inLambda`.
    /// @tparam InLambdaType Movable callable invocable with `(sync::PipelineArray<IComputeContext*>&)`.
    /// @param[in,out] inLambda Callable to execute across active contexts.
    template <typename InLambdaType>
    void enqueueLambdaMultiPipe(InLambdaType&& inLambda) noexcept
    {
        enqueueLambdaMultiPipe(
            sync::PipelineStage::Graphics | sync::PipelineStage::AsyncCompute, std::forward<InLambdaType>(inLambda)
        );
    }

public:
    /// @brief Reports whether any command packets are currently queued in this command list.
    /// @return True when `m_numCommands > 0`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasCommands() const noexcept
    {
        return m_numCommands > 0u;
    }

    /// @brief Returns the number of queued command packets awaiting replay.
    /// @return Queued packet count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getNumCommands() const noexcept
    {
        return m_numCommands;
    }

    /// @brief Returns total bytes currently allocated from this command list's linear packet arena.
    /// @return Arena memory usage in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::USize getUsedMemory() const noexcept
    {
        return m_memManager.getUsedBytes();
    }

    /// @brief Reports whether this command list is currently replaying packets inside `execute()`.
    /// @return True during `execute()`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isExecuting() const noexcept
    {
        return m_isExecuting;
    }

    /// @brief Reports whether this command list is the singleton `CommandListImmediate`.
    /// @return True for `CommandListImmediate`, false for deferred command lists.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isImmediate() const noexcept
    {
        return m_isImmediate;
    }

    /// @brief Casts this command list to `CommandListImmediate&` (asserts `isImmediate()`).
    /// @return Reference to `*this` as `CommandListImmediate`.
    [[nodiscard]] CommandListImmediate& getAsImmediate() noexcept;

    /// @brief Reports whether commands execute synchronously into the backend context rather than being recorded.
    /// @return True when bypass mode is enabled and this command list is immediate.
    [[nodiscard]] bool bypass() const noexcept;

    /// @brief Reports whether commands execute directly at the bottom of the RHI pipe (`isExecuting() || bypass()`).
    /// @return True when bottom-of-pipe backend calls may execute immediately.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isBottomOfPipe() const noexcept
    {
        return m_isExecuting || bypass();
    }

    /// @brief Reports whether the caller is recording at the top of the RHI pipe (`!isBottomOfPipe()`).
    /// @return True when commands are recorded as packets for later replay.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isTopOfPipe() const noexcept
    {
        return !isBottomOfPipe();
    }

    /// @brief Returns the currently selected execution pipeline (`Graphics` or `AsyncCompute`).
    /// @return Active `sync::PipelineStage`.
    [[nodiscard]] GP_FORCEINLINE_HINT sync::PipelineStage getPipeline() const noexcept
    {
        return m_activePipeline;
    }

    /// @brief Reports whether the currently active execution pipeline is `PipelineStage::Graphics`.
    /// @return True when recording or executing on the graphics pipeline.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isGraphics() const noexcept
    {
        return m_activePipeline == sync::PipelineStage::Graphics;
    }

    /// @brief Reports whether the currently active execution pipeline is `PipelineStage::AsyncCompute`.
    /// @return True when recording or executing on the async compute pipeline.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isAsyncCompute() const noexcept
    {
        return m_activePipeline == sync::PipelineStage::AsyncCompute;
    }

    /// @brief Reports whether the active backend requires explicit shader SRV/UAV unbind commands.
    /// @return True when `getRHIGlobals().needsShaderUnbinds` is enabled.
    [[nodiscard]] bool needsShaderUnbinds() const noexcept;

    /// @brief Returns the bitmask of pipelines activated on this command list since the last `reset()`.
    /// @return Activated `sync::PipelineStage` mask.
    [[nodiscard]] GP_FORCEINLINE_HINT sync::PipelineStage getPipelines() const noexcept
    {
        return m_activePipelines;
    }

    /// @brief Switches the active execution pipeline (`Graphics` or `AsyncCompute`), returning the previous pipeline.
    /// @param[in] pipeline Target single-bit pipeline stage.
    /// @return Previous active `sync::PipelineStage`.
    sync::PipelineStage switchPipeline(sync::PipelineStage pipeline) noexcept;

    /// @brief Ensures contexts are acquired for all pipelines specified in `pipelines`.
    /// @param[in] pipelines Bitmask of pipeline stages to activate.
    void activatePipelines(sync::PipelineStage pipelines) noexcept;

    /// @brief Returns the active graphics `ICommandContext` bound to this command list.
    /// @return Reference to the active graphics context.
    [[nodiscard]] ICommandContext& getContext() noexcept;

    /// @brief Returns the active `IComputeContext` for the current execution pipeline (`Graphics` or `AsyncCompute`).
    /// @return Reference to the active compute context.
    [[nodiscard]] IComputeContext& getComputeContext() noexcept;

    /// @brief Binds an explicit backend context directly to `pipeline` (primarily used by tests and executor setup).
    /// @param[in] pipeline Target single-bit pipeline (`Graphics` or `AsyncCompute`).
    /// @param[in] context Backend context pointer (may be `nullptr`).
    void setContext(sync::PipelineStage pipeline, IComputeContext* context) noexcept;

    /// @brief Returns the currently tracked multi-GPU execution mask on this command list.
    /// @return Active `GPUMask`.
    [[nodiscard]] GP_FORCEINLINE_HINT GPUMask getGPUMask() const noexcept
    {
        return m_persistentState.currentGPUMask;
    }

    /// @brief Reports whether this command list is currently inside an active graphics render pass.
    /// @return True between `beginRenderPass()` and `endRenderPass()`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isInsideRenderPass() const noexcept
    {
        return m_persistentState.insideRenderPass;
    }

    /// @brief Reports whether this command list is currently outside any graphics render pass.
    /// @return True when no render pass is active.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isOutsideRenderPass() const noexcept
    {
        return !m_persistentState.insideRenderPass;
    }

    /// @brief Reports whether this command list is currently inside a viewport drawing scope.
    /// @return True between `beginDrawingViewport()` and `endDrawingViewport()`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isInsideDrawingViewport() const noexcept
    {
        return m_persistentState.insideDrawingViewport;
    }

    /// @brief Returns the cached render target info from the currently active render pass.
    /// @return Immutable reference to `pipeline::GraphicsPipelineRenderTargetsInfo`.
    [[nodiscard]] GP_FORCEINLINE_HINT const pipeline::GraphicsPipelineRenderTargetsInfo&
        getCachedRenderTargetsInfo() const noexcept
    {
        return m_persistentState.cachedRenderTargetsInfo;
    }

    /// @brief Returns the active static uniform buffer bindings tracked on this command list.
    /// @return Immutable reference to `shader::UniformBufferStaticBindings`.
    [[nodiscard]] GP_FORCEINLINE_HINT const shader::UniformBufferStaticBindings&
        getStaticUniformBuffers() const noexcept
    {
        return m_persistentState.staticUniformBuffers;
    }

    /// @brief Finalizes recording on a deferred command list so it is ready for submission.
    void finishRecording() noexcept;

    /// @brief Reports whether `finishRecording()` has been called since the last `reset()`.
    /// @return True if finalized.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isFinished() const noexcept
    {
        return m_isFinished;
    }

    /// @brief Replays every recorded command packet in FIFO order against the bound backend contexts, then resets.
    void execute() noexcept;

    /// @brief Clears all queued packets, detaches contexts, resets persistent flags, and returns arena pages to the
    ///        shared `PagePool`.
    void reset() noexcept;

    /// @brief Detaches all bound backend contexts and clears `m_activePipelines` after submission.
    void detachContexts() noexcept;

#if GP_WITH_RHI_BREADCRUMBS

public:
    /// @brief Returns (lazily creating if needed) the `BreadcrumbAllocator` owned by this command list.
    /// @return Mutable reference to `diagnostics::BreadcrumbAllocator`.
    [[nodiscard]] diagnostics::BreadcrumbAllocator& getBreadcrumbAllocator() noexcept;

    /// @brief Returns a shared pointer to this command list's `BreadcrumbAllocator` (`nullptr` if none allocated yet).
    /// @return `gp::SharedPtr<diagnostics::BreadcrumbAllocator>`.
    [[nodiscard]] const gp::SharedPtr<diagnostics::BreadcrumbAllocator>& getBreadcrumbAllocatorShared() const noexcept
    {
        return m_breadcrumbAllocator;
    }

    /// @brief Exports a reference-counted handle to the currently active CPU breadcrumb node on this command list.
    /// @return `diagnostics::BreadcrumbNodeRef` keeping the node's allocator alive across threads.
    [[nodiscard]] diagnostics::BreadcrumbNodeRef getCurrentBreadcrumbRef() const noexcept;

    /// @brief Returns the currently active CPU breadcrumb node on this command list (`nullptr` at root).
    /// @return Active CPU `diagnostics::BreadcrumbNode*`.
    [[nodiscard]] GP_FORCEINLINE_HINT diagnostics::BreadcrumbNode* getCurrentBreadcrumbCPU() const noexcept
    {
        return m_persistentState.currentBreadcrumbCPU;
    }

    /// @brief Returns the currently active GPU breadcrumb node on `pipeline` (`nullptr` when none).
    /// @param[in] pipeline Single-bit `sync::PipelineStage`.
    /// @return Active GPU `diagnostics::BreadcrumbNode*` for `pipeline`.
    [[nodiscard]] diagnostics::BreadcrumbNode* getCurrentBreadcrumbGPU(sync::PipelineStage pipeline) const noexcept;

    /// @brief Pushes `node` onto this command list's CPU breadcrumb stack (linking parent/sentinel if needed).
    /// @param[in] node Non-null breadcrumb node to enter.
    /// @param[in] execute True when entering at the top of recording; false when replaying pre-linked subtrees.
    void beginBreadcrumbCPU(diagnostics::BreadcrumbNode* node, bool execute = true) noexcept;

    /// @brief Pops `node` from this command list's CPU breadcrumb stack.
    /// @param[in] node Breadcrumb node expected at the top of the CPU stack.
    /// @param[in] execute True when exiting at the top of recording.
    void endBreadcrumbCPU(diagnostics::BreadcrumbNode* node, bool execute = true) noexcept;

    /// @brief Records or executes a GPU entry marker for `node` on `pipeline` and links `node` into the GPU range.
    /// @param[in] node Non-null breadcrumb node.
    /// @param[in] pipeline Single-bit execution pipeline (`Graphics` or `AsyncCompute`).
    void beginBreadcrumbGPU(diagnostics::BreadcrumbNode* node, sync::PipelineStage pipeline) noexcept;

    /// @brief Records or executes a GPU exit marker for `node` on `pipeline`.
    /// @param[in] node Non-null breadcrumb node matching the top of `pipeline`'s GPU stack.
    /// @param[in] pipeline Single-bit execution pipeline (`Graphics` or `AsyncCompute`).
    void endBreadcrumbGPU(diagnostics::BreadcrumbNode* node, sync::PipelineStage pipeline) noexcept;

    /// @brief Attaches `Sentinel`-rooted breadcrumb nodes from a parallel `childAllocator` under this command list's
    ///        active CPU breadcrumb and retains `childAllocator`.
    /// @param[in,out] childAllocator Child command list's breadcrumb allocator.
    /// @param[in,out] subTreeRoots Unparented/`Sentinel`-rooted child breadcrumb list (unlinked during attachment).
    void attachBreadcrumbSubTree(
        diagnostics::BreadcrumbAllocator& childAllocator, diagnostics::BreadcrumbList& subTreeRoots
    ) noexcept;

    /// @brief Returns the `[first, last]` GPU breadcrumb range recorded on `pipeline` during the current batch.
    /// @param[in] pipeline Single-bit execution pipeline.
    /// @return `diagnostics::BreadcrumbRange` for `pipeline`.
    [[nodiscard]] diagnostics::BreadcrumbRange exportBreadcrumbRange(sync::PipelineStage pipeline) const noexcept;

    /// @brief Returns all `BreadcrumbAllocator` instances referenced by commands recorded on this command list.
    /// @return Const reference to `diagnostics::BreadcrumbAllocatorArray`.
    [[nodiscard]] const diagnostics::BreadcrumbAllocatorArray& getBreadcrumbAllocatorRefs() const noexcept
    {
        return m_breadcrumbAllocatorRefs;
    }
#endif

public:
    /// @brief Returns the scratch `BatchedShaderParameters` builder owned by this command list.
    /// @return Mutable reference to the scratch parameter batch.
    [[nodiscard]] shader::BatchedShaderParameters& getScratchShaderParameters() noexcept;

    /// @brief Returns the scratch `BatchedShaderUnbinds` builder owned by this command list.
    /// @return Mutable reference to the scratch unbind batch.
    [[nodiscard]] shader::BatchedShaderUnbinds& getScratchShaderUnbinds() noexcept;

    /// @brief Allocates a new `BatchedShaderParametersAllocator` whose lifetime is tied to this command list's
    /// `reset()`.
    /// @return Non-null pointer to an arena-owned `BatchedShaderParametersAllocator`.
    [[nodiscard]] shader::BatchedShaderParametersAllocator* createBatchedShaderParameterAllocator() noexcept;

public:
    /// @brief Creates a GPU buffer initializer through `DynamicRHI::createBufferInitializer()`.
    /// @param[in] createDesc Buffer creation descriptor.
    /// @return Move-only `resource::BufferInitializer`.
    [[nodiscard]] resource::BufferInitializer
        createBufferInitializer(const resource::BufferCreateDesc& createDesc) noexcept;

    /// @brief Creates and immediately finalizes a GPU buffer from `createDesc`.
    /// @param[in] createDesc Buffer creation descriptor.
    /// @return Owning `resource::BufferRef`.
    [[nodiscard]] resource::BufferRef createBuffer(const resource::BufferCreateDesc& createDesc) noexcept;

    /// @brief Creates a GPU texture initializer through `DynamicRHI::createTextureInitializer()`.
    /// @param[in] createDesc Texture creation descriptor.
    /// @return Move-only `resource::TextureInitializer`.
    [[nodiscard]] resource::TextureInitializer
        createTextureInitializer(const resource::TextureCreateDesc& createDesc) noexcept;

    /// @brief Creates and immediately finalizes a GPU texture from `createDesc`.
    /// @param[in] createDesc Texture creation descriptor.
    /// @return Owning `resource::TextureRef`.
    [[nodiscard]] resource::TextureRef createTexture(const resource::TextureCreateDesc& createDesc) noexcept;

    /// @brief Creates a `TextureReference` initially pointing to `referencedTexture`.
    /// @param[in] referencedTexture Optional initial target texture.
    /// @return Owning `resource::TextureReferenceRef`.
    [[nodiscard]] resource::TextureReferenceRef
        createTextureReference(resource::Texture* referencedTexture = nullptr) noexcept;

    /// @brief Updates `textureReference` to point at `newTexture` on the RHI timeline.
    /// @param[in] textureReference Target texture reference wrapper.
    /// @param[in] newTexture New underlying texture (or `nullptr` for default texture).
    void updateTextureReference(resource::TextureReference* textureReference, resource::Texture* newTexture) noexcept;

    /// @brief Creates a shader resource view for `buffer` using `desc`.
    /// @param[in] buffer Target buffer.
    /// @param[in] desc Buffer SRV descriptor.
    /// @return Owning `view::ShaderResourceViewRef`.
    [[nodiscard]] view::ShaderResourceViewRef
        createShaderResourceView(resource::Buffer* buffer, const view::ViewDesc& desc) noexcept;

    /// @brief Creates a shader resource view for `texture` using `desc`.
    /// @param[in] texture Target texture.
    /// @param[in] desc Texture SRV descriptor.
    /// @return Owning `view::ShaderResourceViewRef`.
    [[nodiscard]] view::ShaderResourceViewRef
        createShaderResourceView(resource::Texture* texture, const view::ViewDesc& desc) noexcept;

    /// @brief Creates an unordered access view for `buffer` using `desc`.
    /// @param[in] buffer Target buffer.
    /// @param[in] desc Buffer UAV descriptor.
    /// @return Owning `view::UnorderedAccessViewRef`.
    [[nodiscard]] view::UnorderedAccessViewRef
        createUnorderedAccessView(resource::Buffer* buffer, const view::ViewDesc& desc) noexcept;

    /// @brief Creates an unordered access view for `texture` using `desc`.
    /// @param[in] texture Target texture.
    /// @param[in] desc Texture UAV descriptor.
    /// @return Owning `view::UnorderedAccessViewRef`.
    [[nodiscard]] view::UnorderedAccessViewRef
        createUnorderedAccessView(resource::Texture* texture, const view::ViewDesc& desc) noexcept;

    /// @brief Updates the contents of an existing `UniformBuffer` on the RHI timeline.
    /// @param[in] uniformBuffer Target uniform buffer.
    /// @param[in] contents Pointer to new layout-matching struct bytes.
    void updateUniformBuffer(resource::UniformBuffer* uniformBuffer, const void* contents) noexcept;

    /// @brief Updates a `StreamSourceSlot`'s bound buffer and byte offset on the RHI timeline.
    /// @param[in] slot Target stream source slot.
    /// @param[in] buffer New vertex buffer pointer.
    /// @param[in] offset Byte offset into `buffer`.
    void updateStreamSourceSlot(
        resource::StreamSourceSlot* slot, resource::Buffer* buffer, gp::UInt32 offset = 0u
    ) noexcept;

    /// @brief Uploads CPU pixel rows into a 2D texture region on the RHI timeline.
    /// @param[in] texture Target 2D texture.
    /// @param[in] mipIndex Target mip level.
    /// @param[in] updateRegion Sub-rectangle in texels.
    /// @param[in] sourcePitch Row stride in bytes of `sourceData`.
    /// @param[in] sourceData Pointer to source pixel bytes.
    void updateTexture2D(
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion2D& updateRegion,
        gp::UInt32 sourcePitch,
        const gp::UInt8* sourceData
    ) noexcept;

    /// @brief Uploads CPU voxel slices into a 3D texture region on the RHI timeline.
    /// @param[in] texture Target 3D texture.
    /// @param[in] mipIndex Target mip level.
    /// @param[in] updateRegion Sub-box in texels.
    /// @param[in] sourceRowPitch Row stride in bytes of `sourceData`.
    /// @param[in] sourceDepthPitch Slice stride in bytes of `sourceData`.
    /// @param[in] sourceData Pointer to source voxel bytes.
    void updateTexture3D(
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion3D& updateRegion,
        gp::UInt32 sourceRowPitch,
        gp::UInt32 sourceDepthPitch,
        const gp::UInt8* sourceData
    ) noexcept;

    /// @brief Replaces backing GPU allocations for each `(dst, src)` pair in `infos`.
    /// @param[in] infos Span of resource replacement pairs.
    void replaceResources(gp::ConstVectorView<resource::ResourceReplaceInfo> infos) noexcept;

    /// @brief Attaches a backend debug label name to `texture`.
    /// @param[in] texture Target texture.
    /// @param[in] name Null-terminated debug name.
    void bindDebugLabelName(resource::Texture* texture, const char* name) noexcept;

    /// @brief Attaches a backend debug label name to `buffer`.
    /// @param[in] buffer Target buffer.
    /// @param[in] name Null-terminated debug name.
    void bindDebugLabelName(resource::Buffer* buffer, const char* name) noexcept;

    /// @brief Attaches a backend debug label name to `uav`.
    /// @param[in] uav Target unordered access view.
    /// @param[in] name Null-terminated debug name.
    void bindDebugLabelName(view::UnorderedAccessView* uav, const char* name) noexcept;

    /// @brief Maps `[offset, offset + size)` of `buffer` for CPU access.
    /// @param[in] buffer Target buffer.
    /// @param[in] offset Byte offset.
    /// @param[in] size Byte length.
    /// @param[in] lockMode CPU lock mode.
    /// @return Mapped CPU pointer, or `nullptr` on failure.
    [[nodiscard]] void* lockBuffer(
        resource::Buffer* buffer, gp::UInt32 offset, gp::UInt32 size, resource::ResourceLockMode lockMode
    ) noexcept;

    /// @brief Unmaps `buffer` previously locked by `lockBuffer()`.
    /// @param[in] buffer Target buffer.
    void unlockBuffer(resource::Buffer* buffer) noexcept;

protected:
    /// @brief Creates and executes or enqueues a single-pipeline or multi-pipeline transition batch.
    /// @param[in] infos Per-resource transition descriptors.
    /// @param[in] srcPipelines Source execution pipelines.
    /// @param[in] dstPipelines Destination execution pipelines.
    /// @param[in] flags Transition creation flags.
    void transitionInternal(
        gp::ConstVectorView<sync::TransitionInfo> infos,
        sync::PipelineStage srcPipelines,
        sync::PipelineStage dstPipelines,
        sync::TransitionCreateFlags flags
    ) noexcept;

    /// @brief Releases any chained `BatchedShaderParametersAllocator` objects owned by this command list.
    void releaseBatchedShaderParameterAllocators() noexcept;

protected:
    /// @brief State retained across command recording on this command list until `reset()`.
    struct PersistentState
    {
        /// @brief Multi-GPU node mask active at the start of the current recording batch.
        GPUMask initialGPUMask{ GPUMask::all() };

        /// @brief Active multi-GPU node mask.
        GPUMask currentGPUMask{ GPUMask::all() };

        /// @brief Active static global uniform buffer bindings.
        shader::UniformBufferStaticBindings staticUniformBuffers{};

        /// @brief Render target metadata cached from the active `beginRenderPass()`.
        pipeline::GraphicsPipelineRenderTargetsInfo cachedRenderTargetsInfo{};

        /// @brief True while a render pass is active on this command list.
        bool insideRenderPass{ false };

        /// @brief True while inside a `beginDrawingViewport()` / `endDrawingViewport()` scope.
        bool insideDrawingViewport{ false };

#if GP_WITH_RHI_BREADCRUMBS
        /// @brief Currently active CPU breadcrumb node on this command list (`nullptr` at root).
        diagnostics::BreadcrumbNode* currentBreadcrumbCPU{ nullptr };
#endif
    };

#if GP_WITH_RHI_BREADCRUMBS
    /// @brief Per-pipeline GPU breadcrumb stack and recorded node range.
    struct GPUBreadcrumbPipelineState
    {
        /// @brief Currently active GPU breadcrumb node on this pipeline.
        diagnostics::BreadcrumbNode* current{ nullptr };

        /// @brief Inclusive range of GPU breadcrumb nodes begun on this pipeline in the current batch.
        diagnostics::BreadcrumbList list{};
    };
#endif

    /// @brief Linear page-backed command packet arena (zero heap overhead when idle).
    gp::memory::PagedLinearAllocator m_memManager;

    /// @brief Head of the intrusive command packet list.
    CommandBase* m_root{ nullptr };

    /// @brief Tail link pointer (`&last->next` or `&m_root`) for O(1) append.
    CommandBase** m_commandLink{ nullptr };

    /// @brief Active backend contexts indexed by single-bit `sync::PipelineStage`.
    sync::PipelineArray<IComputeContext*> m_contexts{};

    /// @brief Cached pointer to the graphics `ICommandContext` (mirrors `m_contexts[Graphics]`).
    ICommandContext* m_graphicsContext{ nullptr };

    /// @brief Cached pointer to the compute `IComputeContext` for the active pipeline.
    IComputeContext* m_computeContext{ nullptr };

    /// @brief Head of the intrusive list of `BatchedShaderParametersAllocator` instances freed on `reset()`.
    shader::BatchedShaderParametersAllocator* m_parameterAllocatorHead{ nullptr };

    /// @brief Default scratch allocator backing `m_scratchShaderParameters`.
    shader::BatchedShaderParametersAllocator m_scratchParameterAllocator;

    /// @brief Reusable scratch shader parameter batch.
    shader::BatchedShaderParameters m_scratchShaderParameters;

    /// @brief Reusable scratch shader unbind batch.
    shader::BatchedShaderUnbinds m_scratchShaderUnbinds{};

    /// @brief Persistent recording state (GPU mask, render pass state, static UBs).
    PersistentState m_persistentState{};

#if GP_WITH_RHI_BREADCRUMBS
    /// @brief Lazily created linear arena owning breadcrumb nodes allocated on this command list.
    gp::SharedPtr<diagnostics::BreadcrumbAllocator> m_breadcrumbAllocator{};

    /// @brief Retained references to this and any child command lists' breadcrumb allocators until submission retires.
    diagnostics::BreadcrumbAllocatorArray m_breadcrumbAllocatorRefs{};

    /// @brief Unattached `Sentinel`-parented root nodes recorded on deferred command lists awaiting parent attachment.
    diagnostics::BreadcrumbList m_unattachedSubTreeRoots{};

    /// @brief Per-pipeline GPU breadcrumb stack and linked range.
    sync::PipelineArray<GPUBreadcrumbPipelineState> m_gpuBreadcrumbs{};
#endif

    /// @brief Number of queued command packets in `[m_root .. tail]`.
    gp::UInt32 m_numCommands{ 0u };

    /// @brief Pipeline active at the start of the current recorded packet batch.
    sync::PipelineStage m_initialPipeline{ sync::PipelineStage::Graphics };

    /// @brief Currently selected recording/execution pipeline (`Graphics` or `AsyncCompute`).
    sync::PipelineStage m_activePipeline{ sync::PipelineStage::Graphics };

    /// @brief Bitmask of all pipelines activated since the last `reset()`.
    sync::PipelineStage m_activePipelines{ sync::PipelineStage::None };

    /// @brief True while `execute()` is replaying command packets into backend contexts.
    bool m_isExecuting{ false };

    /// @brief True only for the singleton `CommandListImmediate`.
    bool m_isImmediate{ false };

    /// @brief True after `finishRecording()` has been called.
    bool m_isFinished{ false };
};

template <typename InLambdaType>
void LambdaCommandMultiPipe<InLambdaType>::execute(CommandListBase& cmdList) noexcept
{
    lambda(cmdList.m_contexts);
}

}   // namespace gp::rhi::command
