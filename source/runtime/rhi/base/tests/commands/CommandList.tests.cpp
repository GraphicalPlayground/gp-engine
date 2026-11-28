// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandList.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/CommandTypes.hpp"
#include "commands/ComputeCommandList.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/IComputeContext.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "device/DynamicRHI.hpp"
#include "device/DynamicRHIPSOFallback.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/vector/Vector4.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineStateFallbacks.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerState.hpp"
#include "pipeline/SamplerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "presentation/Viewport.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/StreamSourceSlot.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/ShaderParameters.hpp"
#include "shaders/VertexShader.hpp"
#include "sync/GPUFence.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/Transition.hpp"
#include "threading/roles/ScopedThreadRole.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::command::tests
{

namespace
{

class MockBuffer final : public resource::Buffer
{
public:
    explicit MockBuffer(const resource::BufferCreateDesc& desc) noexcept
        : Buffer(desc)
    {}

protected:
    virtual ~MockBuffer() noexcept override = default;
};

class MockTexture final : public resource::Texture
{
public:
    explicit MockTexture(const resource::TextureCreateDesc& desc) noexcept
        : Texture(desc)
    {}

protected:
    virtual ~MockTexture() noexcept override = default;
};

class MockUniformBuffer final : public resource::UniformBuffer
{
public:
    explicit MockUniformBuffer(const resource::UniformBufferLayout* const layout) noexcept
        : UniformBuffer(layout)
    {}

protected:
    virtual ~MockUniformBuffer() noexcept override = default;
};

class MockGPUFence final : public sync::GPUFence
{
public:
    explicit MockGPUFence(const gp::Name name) noexcept
        : GPUFence(name)
    {}

    virtual void clear() noexcept override
    {
        m_signaled = false;
    }

    [[nodiscard]] virtual bool poll() const noexcept override
    {
        return m_signaled;
    }

    virtual void wait(CommandListImmediate&, GPUMask) const noexcept override
    {}

    void signal() noexcept
    {
        m_signaled = true;
    }

protected:
    virtual ~MockGPUFence() noexcept override = default;

private:
    bool m_signaled{ false };
};

struct RecordedCalls
{
    gp::UInt32 computePSOBinds{ 0u };
    gp::UInt32 dispatches{ 0u };
    gp::UInt32 indirectDispatches{ 0u };
    gp::UInt32 beginTransitionsCalls{ 0u };
    gp::UInt32 endTransitionsCalls{ 0u };
    gp::UInt32 clearUAVFloatCalls{ 0u };
    gp::UInt32 clearUAVUintCalls{ 0u };
    gp::UInt32 rootConstantCalls{ 0u };
    gp::UInt32 computeParamCalls{ 0u };
    gp::UInt32 computeUnbindCalls{ 0u };
    gp::UInt32 staticUBCalls{ 0u };
    gp::UInt32 uniformOffsetCalls{ 0u };
    gp::UInt32 copyToStagingCalls{ 0u };
    gp::UInt32 writeGPUFenceCalls{ 0u };
    gp::UInt32 postExternalResets{ 0u };

    gp::UInt32 graphicsPSOBinds{ 0u };
    gp::UInt32 boundShaderStateBinds{ 0u };
    gp::UInt32 depthStencilBinds{ 0u };
    gp::UInt32 rasterizerBinds{ 0u };
    gp::UInt32 blendBinds{ 0u };
    gp::UInt32 depthBoundsEnableCalls{ 0u };
    gp::UInt32 computeShaderFallbackBinds{ 0u };
    gp::UInt32 graphicsParamCalls{ 0u };
    gp::UInt32 graphicsUnbindCalls{ 0u };
    gp::UInt32 stencilRefCalls{ 0u };
    gp::UInt32 blendFactorCalls{ 0u };
    gp::UInt32 streamSourceCalls{ 0u };
    gp::UInt32 viewportCalls{ 0u };
    gp::UInt32 stereoViewportCalls{ 0u };
    gp::UInt32 multiViewportCalls{ 0u };
    gp::UInt32 scissorCalls{ 0u };
    gp::UInt32 depthBoundsCalls{ 0u };
    gp::UInt32 shadingRateCalls{ 0u };
    gp::UInt32 drawPrimitiveCalls{ 0u };
    gp::UInt32 drawPrimitiveIndirectCalls{ 0u };
    gp::UInt32 drawIndexedCalls{ 0u };
    gp::UInt32 drawIndexedIndirectCalls{ 0u };
    gp::UInt32 multiDrawIndexedIndirectCalls{ 0u };
    gp::UInt32 dispatchMeshCalls{ 0u };
    gp::UInt32 dispatchIndirectMeshCalls{ 0u };
    gp::UInt32 beginRenderPassCalls{ 0u };
    gp::UInt32 endRenderPassCalls{ 0u };
    gp::UInt32 nextSubpassCalls{ 0u };
    gp::UInt32 copyTextureCalls{ 0u };
    gp::UInt32 copyBufferRegionCalls{ 0u };
    gp::UInt32 resummarizeHTileCalls{ 0u };
    gp::UInt32 beginRenderQueryCalls{ 0u };
    gp::UInt32 endRenderQueryCalls{ 0u };

    gp::UInt32 lastDispatchX{ 0u };
    gp::UInt32 lastStreamOffset{ 0u };
    gp::UInt32 lastStencilRef{ 0u };
    gp::UInt32 lastDrawPrimitives{ 0u };
    gp::UInt32 lastMultiViewportCount{ 0u };
    resource::UIntVector4 lastRootConstants{};
    AsyncComputeBudget lastAsyncBudget{ AsyncComputeBudget::All };
    SyncComputeBudget lastSyncBudget{ SyncComputeBudget::Balanced };
    bool lastLowLatencyHint{ false };
};

class RecordingAsyncComputeContext final : public IComputeContext
{
public:
    explicit RecordingAsyncComputeContext(RecordedCalls& inLog) noexcept
        : m_log(inLog)
    {}

    virtual void setComputePipelineState(pipeline::ComputePipelineState*) noexcept override
    {
        ++m_log.computePSOBinds;
    }

    virtual void dispatchComputeShader(const gp::UInt32 x, const gp::UInt32 y, const gp::UInt32 z) noexcept override
    {
        static_cast<void>(y);
        static_cast<void>(z);
        ++m_log.dispatches;
        m_log.lastDispatchX = x;
    }

    virtual void dispatchIndirectComputeShader(resource::Buffer*, gp::UInt32) noexcept override
    {
        ++m_log.indirectDispatches;
    }

    virtual void setAsyncComputeBudget(const AsyncComputeBudget budget) noexcept override
    {
        m_log.lastAsyncBudget = budget;
    }

    virtual void setComputeBudget(const SyncComputeBudget budget) noexcept override
    {
        m_log.lastSyncBudget = budget;
    }

    virtual void setLowLatencyHint(const bool lowLatency) noexcept override
    {
        m_log.lastLowLatencyHint = lowLatency;
    }

    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {
        ++m_log.beginTransitionsCalls;
    }

    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {
        ++m_log.endTransitionsCalls;
    }

    virtual void clearUAVFloat(view::UnorderedAccessView*, const gp::math::Vector4<gp::Float32>&) noexcept override
    {
        ++m_log.clearUAVFloatCalls;
    }

    virtual void clearUAVUint(view::UnorderedAccessView*, const resource::UIntVector4&) noexcept override
    {
        ++m_log.clearUAVUintCalls;
    }

    virtual void setShaderRootConstants(const resource::UIntVector4& constants) noexcept override
    {
        ++m_log.rootConstantCalls;
        m_log.lastRootConstants = constants;
    }

    virtual void setShaderParameters(
        shader::ComputeShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {
        ++m_log.computeParamCalls;
    }

    virtual void
        setShaderUnbinds(shader::ComputeShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
    {
        ++m_log.computeUnbindCalls;
    }

    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings&) noexcept override
    {
        ++m_log.staticUBCalls;
    }

    virtual void setUniformBlockOffset(view::ShaderResourceView*, gp::UInt32) noexcept override
    {
        ++m_log.uniformOffsetCalls;
    }

    virtual void
        copyToStagingBuffer(resource::Buffer*, resource::StagingBuffer*, gp::UInt32, gp::UInt32) noexcept override
    {
        ++m_log.copyToStagingCalls;
    }

    virtual void writeGPUFence(sync::GPUFence* const fence) noexcept override
    {
        ++m_log.writeGPUFenceCalls;
        if (fence != nullptr)
        {
            static_cast<MockGPUFence*>(fence)->signal();
        }
    }

    virtual void postExternalCommandsReset() noexcept override
    {
        ++m_log.postExternalResets;
    }

private:
    RecordedCalls& m_log;
};

class RecordingMockRHI final
    : public DynamicRHIPSOFallback
    , public ICommandContextPSOFallback
{
public:
    RecordedCalls graphicsLog{};
    RecordedCalls asyncComputeLog{};
    RecordingAsyncComputeContext asyncComputeContext{ asyncComputeLog };
    gp::UInt32 finalizeCalls{ 0u };
    gp::UInt32 submitCalls{ 0u };
    gp::UInt32 beginFrameCalls{ 0u };
    gp::UInt32 endFrameCalls{ 0u };
    gp::UInt32 beginViewportCalls{ 0u };
    gp::UInt32 endViewportCalls{ 0u };
    gp::UInt32 blockIdleCalls{ 0u };
    gp::Vector<gp::UInt8> scratchBuffer{};

    virtual void init() noexcept override
    {
        getRHIGlobals().supportsAsyncCompute = true;
    }

    virtual void shutdown() noexcept override
    {}

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "RecordingMock";
    }

    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override
    {
        return InterfaceType::Null;
    }

    [[nodiscard]] virtual pipeline::SamplerStateRef
        createSamplerState(const pipeline::SamplerStateDesc&) noexcept override
    {
        return pipeline::SamplerStateRef(new pipeline::SamplerState());
    }

    [[nodiscard]] virtual pipeline::RasterizerStateRef
        createRasterizerState(const pipeline::RasterizerStateDesc&) noexcept override
    {
        return pipeline::RasterizerStateRef(new pipeline::RasterizerState());
    }

    [[nodiscard]] virtual pipeline::DepthStencilStateRef
        createDepthStencilState(const pipeline::DepthStencilStateDesc&) noexcept override
    {
        return pipeline::DepthStencilStateRef(new pipeline::DepthStencilState());
    }

    [[nodiscard]] virtual pipeline::BlendStateRef createBlendState(const pipeline::BlendStateDesc&) noexcept override
    {
        return pipeline::BlendStateRef(new pipeline::BlendState());
    }

    [[nodiscard]] virtual pipeline::VertexDeclarationRef
        createVertexDeclaration(const pipeline::VertexDeclarationElementList&) noexcept override
    {
        return pipeline::VertexDeclarationRef(new pipeline::VertexDeclaration());
    }

    [[nodiscard]] virtual shader::VertexShaderRef
        createVertexShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return shader::VertexShaderRef(new shader::VertexShader());
    }

    [[nodiscard]] virtual shader::PixelShaderRef
        createPixelShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return shader::PixelShaderRef(new shader::PixelShader());
    }

    [[nodiscard]] virtual shader::GeometryShaderRef
        createGeometryShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return shader::GeometryShaderRef(new shader::GeometryShader());
    }

    [[nodiscard]] virtual shader::ComputeShaderRef
        createComputeShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return shader::ComputeShaderRef(new shader::ComputeShader());
    }

    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        pipeline::VertexDeclaration*, shader::VertexShader*, shader::PixelShader*, shader::GeometryShader*
    ) noexcept override
    {
        return pipeline::BoundShaderStateRef(new pipeline::BoundShaderState());
    }

    [[nodiscard]] virtual resource::UniformBufferRef createUniformBuffer(
        const void*,
        const resource::UniformBufferLayout* const layout,
        resource::UniformBufferUsage,
        resource::UniformBufferValidation
    ) noexcept override
    {
        return resource::UniformBufferRef(new MockUniformBuffer(layout));
    }

    virtual void updateUniformBuffer(CommandListBase&, resource::UniformBuffer*, const void*) noexcept override
    {}

    [[nodiscard]] virtual resource::BufferInitializer createBufferInitializer(
        CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
    ) noexcept override
    {
        resource::BufferRef buf(new MockBuffer(createDesc));
        return resource::BufferInitializer(
            cmdList, std::move(buf), nullptr, 0u, resource::BufferInitializer::FinalizeCallback{}
        );
    }

    [[nodiscard]] virtual void* lockBufferBottomOfPipe(
        CommandListBase&, resource::Buffer*, gp::UInt32, const gp::UInt32 size, resource::ResourceLockMode
    ) noexcept override
    {
        const gp::USize needed = (size > 0u) ? static_cast<gp::USize>(size) : 1u;
        if (scratchBuffer.size() < needed)
        {
            scratchBuffer.resize(needed, 0u);
        }
        return scratchBuffer.data();
    }

    virtual void unlockBufferBottomOfPipe(CommandListBase&, resource::Buffer*) noexcept override
    {}

    [[nodiscard]] virtual resource::TextureInitializer createTextureInitializer(
        CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
    ) noexcept override
    {
        resource::TextureRef tex(new MockTexture(createDesc));
        return resource::TextureInitializer(
            cmdList,
            std::move(tex),
            resource::TextureInitializer::SubresourceCallback{},
            resource::TextureInitializer::FinalizeCallback{}
        );
    }

    [[nodiscard]] virtual resource::TextureLockResult
        lockTexture(CommandListImmediate&, const resource::TextureLockArgs&) noexcept override
    {
        return resource::TextureLockResult{};
    }

    virtual void unlockTexture(CommandListImmediate&, const resource::TextureLockArgs&) noexcept override
    {}

    virtual void updateTexture2D(
        CommandListBase&,
        resource::Texture*,
        gp::UInt32,
        const resource::UpdateTextureRegion2D&,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    virtual void updateTexture3D(
        CommandListBase&,
        resource::Texture*,
        gp::UInt32,
        const resource::UpdateTextureRegion3D&,
        gp::UInt32,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    virtual void readSurfaceData(
        resource::Texture*,
        const resource::IntRect&,
        gp::Vector<gp::math::Color>&,
        const resource::ReadSurfaceDataFlags&
    ) noexcept override
    {}

    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        CommandListBase&, resource::Buffer* const buf, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::ShaderResourceViewRef(new view::ShaderResourceView(buf, desc));
    }

    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        CommandListBase&, resource::Texture* const tex, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::ShaderResourceViewRef(new view::ShaderResourceView(tex, desc));
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        CommandListBase&, resource::Buffer* const buf, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::UnorderedAccessViewRef(new view::UnorderedAccessView(buf, desc));
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        CommandListBase&, resource::Texture* const tex, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::UnorderedAccessViewRef(new view::UnorderedAccessView(tex, desc));
    }

    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(const gp::Name name) noexcept override
    {
        return sync::GPUFenceRef(new MockGPUFence(name));
    }

    [[nodiscard]] virtual sync::RenderQueryRef createRenderQuery(sync::RenderQueryType) noexcept override
    {
        return sync::RenderQueryRef(new sync::RenderQuery());
    }

    [[nodiscard]] virtual bool getRenderQueryResult(sync::RenderQuery*, gp::UInt64&, bool, gp::UInt32) noexcept override
    {
        return true;
    }

    [[nodiscard]] virtual presentation::ViewportRef
        createViewport(void*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {
        return presentation::ViewportRef(new presentation::Viewport());
    }

    virtual void resizeViewport(presentation::Viewport*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {}

    [[nodiscard]] virtual resource::TextureRef getViewportBackBuffer(presentation::Viewport*) noexcept override
    {
        const resource::TextureCreateDesc desc =
            resource::TextureCreateDesc::create2D("BackBuffer").setExtent(1, 1).setFormat(PixelFormat::B8G8R8A8_UNorm);
        return resource::TextureRef(new MockTexture(desc));
    }

    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport*) noexcept override
    {}

    virtual void beginDrawingViewport(presentation::Viewport*, resource::Texture*) noexcept override
    {
        ++beginViewportCalls;
    }

    using DynamicRHI::endDrawingViewport;

    virtual void
        endDrawingViewport(presentation::Viewport*, const presentation::PresentArgsWithStats&) noexcept override
    {
        ++endViewportCalls;
    }

    virtual void beginFrame() noexcept override
    {
        ++beginFrameCalls;
    }

    virtual void endFrame(const EndFrameArgs&) noexcept override
    {
        ++endFrameCalls;
    }

    virtual void blockUntilGPUIdle() noexcept override
    {
        ++blockIdleCalls;
    }

    [[nodiscard]] virtual ICommandContext* getDefaultContext() noexcept override
    {
        return this;
    }

    [[nodiscard]] virtual IComputeContext* getDefaultAsyncComputeContext() noexcept override
    {
        return &asyncComputeContext;
    }

    struct MockPlatformCommandList final : public IPlatformCommandList
    {};

    MockPlatformCommandList dummyPlatformList{};

    virtual void finalizeContexts(FinalizeContextsArgs&&, FinalizeContextsResult& outResult) noexcept override
    {
        ++finalizeCalls;
        outResult.commandLists.clear();
        outResult.commandLists.emplaceBack(&dummyPlatformList);
    }

    virtual void submitCommandLists(SubmitCommandListsArgs&&) noexcept override
    {
        ++submitCalls;
    }

    // IComputeContext overrides
    virtual void dispatchComputeShader(const gp::UInt32 x, const gp::UInt32 y, const gp::UInt32 z) noexcept override
    {
        static_cast<void>(y);
        static_cast<void>(z);
        ++graphicsLog.dispatches;
        graphicsLog.lastDispatchX = x;
    }

    virtual void dispatchIndirectComputeShader(resource::Buffer*, gp::UInt32) noexcept override
    {
        ++graphicsLog.indirectDispatches;
    }

    virtual void setAsyncComputeBudget(const AsyncComputeBudget budget) noexcept override
    {
        graphicsLog.lastAsyncBudget = budget;
    }

    virtual void setComputeBudget(const SyncComputeBudget budget) noexcept override
    {
        graphicsLog.lastSyncBudget = budget;
    }

    virtual void setLowLatencyHint(const bool lowLatency) noexcept override
    {
        graphicsLog.lastLowLatencyHint = lowLatency;
    }

    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {
        ++graphicsLog.beginTransitionsCalls;
    }

    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {
        ++graphicsLog.endTransitionsCalls;
    }

    virtual void clearUAVFloat(view::UnorderedAccessView*, const gp::math::Vector4<gp::Float32>&) noexcept override
    {
        ++graphicsLog.clearUAVFloatCalls;
    }

    virtual void clearUAVUint(view::UnorderedAccessView*, const resource::UIntVector4&) noexcept override
    {
        ++graphicsLog.clearUAVUintCalls;
    }

    virtual void setShaderRootConstants(const resource::UIntVector4& constants) noexcept override
    {
        ++graphicsLog.rootConstantCalls;
        graphicsLog.lastRootConstants = constants;
    }

    virtual void setShaderParameters(
        shader::ComputeShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {
        ++graphicsLog.computeParamCalls;
    }

    virtual void
        setShaderUnbinds(shader::ComputeShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
    {
        ++graphicsLog.computeUnbindCalls;
    }

    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings&) noexcept override
    {
        ++graphicsLog.staticUBCalls;
    }

    virtual void setUniformBlockOffset(view::ShaderResourceView*, gp::UInt32) noexcept override
    {
        ++graphicsLog.uniformOffsetCalls;
    }

    virtual void
        copyToStagingBuffer(resource::Buffer*, resource::StagingBuffer*, gp::UInt32, gp::UInt32) noexcept override
    {
        ++graphicsLog.copyToStagingCalls;
    }

    virtual void writeGPUFence(sync::GPUFence* const fence) noexcept override
    {
        ++graphicsLog.writeGPUFenceCalls;
        if (fence != nullptr)
        {
            static_cast<MockGPUFence*>(fence)->signal();
        }
    }

    virtual void postExternalCommandsReset() noexcept override
    {
        ++graphicsLog.postExternalResets;
    }

    // ICommandContext overrides
    virtual void setMultipleViewports(const gp::UInt32 count, const ViewportBounds*) noexcept override
    {
        ++graphicsLog.multiViewportCalls;
        graphicsLog.lastMultiViewportCount = count;
    }

    virtual void resummarizeHTile(resource::Texture*) noexcept override
    {
        ++graphicsLog.resummarizeHTileCalls;
    }

    virtual void beginRenderQuery(sync::RenderQuery*) noexcept override
    {
        ++graphicsLog.beginRenderQueryCalls;
    }

    virtual void endRenderQuery(sync::RenderQuery*) noexcept override
    {
        ++graphicsLog.endRenderQueryCalls;
    }

    virtual void setStreamSource(gp::UInt32, resource::Buffer*, const gp::UInt32 offset) noexcept override
    {
        ++graphicsLog.streamSourceCalls;
        graphicsLog.lastStreamOffset = offset;
    }

    virtual void
        setViewport(gp::Float32, gp::Float32, gp::Float32, gp::Float32, gp::Float32, gp::Float32) noexcept override
    {
        ++graphicsLog.viewportCalls;
    }

    virtual void setStereoViewport(
        gp::Float32,
        gp::Float32,
        gp::Float32,
        gp::Float32,
        gp::Float32,
        gp::Float32,
        gp::Float32,
        gp::Float32,
        gp::Float32,
        gp::Float32
    ) noexcept override
    {
        ++graphicsLog.stereoViewportCalls;
    }

    virtual void setScissorRect(bool, gp::UInt32, gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {
        ++graphicsLog.scissorCalls;
    }

    virtual void setGraphicsPipelineState(
        pipeline::GraphicsPipelineState* const graphicsState,
        const gp::UInt32 stencilRef,
        const bool applyAdditionalState
    ) noexcept override
    {
        ++graphicsLog.graphicsPSOBinds;
        ICommandContextPSOFallback::setGraphicsPipelineState(graphicsState, stencilRef, applyAdditionalState);
    }

    virtual void setComputePipelineState(pipeline::ComputePipelineState* const computePSO) noexcept override
    {
        ++graphicsLog.computePSOBinds;
        ICommandContextPSOFallback::setComputePipelineState(computePSO);
    }

    virtual void setShaderParameters(
        shader::GraphicsShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {
        ++graphicsLog.graphicsParamCalls;
    }

    virtual void
        setShaderUnbinds(shader::GraphicsShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
    {
        ++graphicsLog.graphicsUnbindCalls;
    }

    virtual void setStencilRef(const gp::UInt32 stencilRef) noexcept override
    {
        ++graphicsLog.stencilRefCalls;
        graphicsLog.lastStencilRef = stencilRef;
    }

    virtual void setBlendFactor(const gp::math::LinearColor&) noexcept override
    {
        ++graphicsLog.blendFactorCalls;
    }

    virtual void drawPrimitive(gp::UInt32, const gp::UInt32 numPrimitives, gp::UInt32) noexcept override
    {
        ++graphicsLog.drawPrimitiveCalls;
        graphicsLog.lastDrawPrimitives = numPrimitives;
    }

    virtual void drawPrimitiveIndirect(resource::Buffer*, gp::UInt32) noexcept override
    {
        ++graphicsLog.drawPrimitiveIndirectCalls;
    }

    virtual void drawIndexedPrimitive(
        resource::Buffer*, gp::Int32, gp::UInt32, gp::UInt32, gp::UInt32, const gp::UInt32 numPrimitives, gp::UInt32
    ) noexcept override
    {
        ++graphicsLog.drawIndexedCalls;
        graphicsLog.lastDrawPrimitives = numPrimitives;
    }

    virtual void drawIndexedPrimitiveIndirect(resource::Buffer*, resource::Buffer*, gp::UInt32) noexcept override
    {
        ++graphicsLog.drawIndexedIndirectCalls;
    }

    virtual void multiDrawIndexedPrimitiveIndirect(
        resource::Buffer*,
        resource::Buffer*,
        gp::UInt32,
        resource::Buffer*,
        gp::UInt32,
        gp::UInt32,
        gp::UInt32,
        gp::UInt32
    ) noexcept override
    {
        ++graphicsLog.multiDrawIndexedIndirectCalls;
    }

    virtual void dispatchMeshShader(gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {
        ++graphicsLog.dispatchMeshCalls;
    }

    virtual void dispatchIndirectMeshShader(resource::Buffer*, gp::UInt32) noexcept override
    {
        ++graphicsLog.dispatchIndirectMeshCalls;
    }

    virtual void setDepthBounds(gp::Float32, gp::Float32) noexcept override
    {
        ++graphicsLog.depthBoundsCalls;
    }

    virtual void setShadingRate(pipeline::VRSShadingRate, pipeline::VRSRateCombiner) noexcept override
    {
        ++graphicsLog.shadingRateCalls;
    }

    virtual void beginRenderPass(const RenderPassInfo& inInfo, const char*) noexcept override
    {
        ++graphicsLog.beginRenderPassCalls;
        m_renderPassInfo = inInfo;
    }

    virtual void endRenderPass() noexcept override
    {
        ++graphicsLog.endRenderPassCalls;
        m_renderPassInfo = RenderPassInfo{};
    }

    virtual void nextSubpass() noexcept override
    {
        ++graphicsLog.nextSubpassCalls;
    }

    virtual void copyTexture(resource::Texture*, resource::Texture*, const CopyTextureInfo&) noexcept override
    {
        ++graphicsLog.copyTextureCalls;
    }

    virtual void
        copyBufferRegion(resource::Buffer*, gp::UInt64, resource::Buffer*, gp::UInt64, gp::UInt64) noexcept override
    {
        ++graphicsLog.copyBufferRegionCalls;
    }

    // ICommandContextPSOFallback overrides
    virtual void setBoundShaderState(pipeline::BoundShaderState*) noexcept override
    {
        ++graphicsLog.boundShaderStateBinds;
    }

    virtual void setDepthStencilState(pipeline::DepthStencilState*, const gp::UInt32 stencilRef) noexcept override
    {
        ++graphicsLog.depthStencilBinds;
        graphicsLog.lastStencilRef = stencilRef;
    }

    virtual void setRasterizerState(pipeline::RasterizerState*) noexcept override
    {
        ++graphicsLog.rasterizerBinds;
    }

    virtual void setBlendState(pipeline::BlendState*, const gp::math::LinearColor&) noexcept override
    {
        ++graphicsLog.blendBinds;
    }

    virtual void enableDepthBoundsTest(bool, gp::Float32, gp::Float32) noexcept override
    {
        ++graphicsLog.depthBoundsEnableCalls;
    }

    virtual void setComputeShader(shader::ComputeShader*) noexcept override
    {
        ++graphicsLog.computeShaderFallbackBinds;
    }
};

class CommandListFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_rhi = new RecordingMockRHI();
        rhiInit(m_rhi);
        getCommandListExecutor().setBypass(false);
    }

    void TearDown() override
    {
        getCommandListExecutor().setBypass(false);
        rhiExit();
        m_rhi = nullptr;
    }

    void exerciseAllCommands(const bool enableBypass)
    {
        CommandListExecutor& executor = getCommandListExecutor();
        executor.setBypass(enableBypass);
        CommandListImmediate& cmdList = getImmediateCommandList();
        EXPECT_EQ(cmdList.bypass(), enableBypass);
        EXPECT_TRUE(cmdList.isImmediate());

        const resource::BufferCreateDesc bufDesc = resource::BufferCreateDesc::create(
            "CmdBuf",
            256u,
            16u,
            resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::IndexBuffer |
                resource::BufferUsageFlags::ShaderResource | resource::BufferUsageFlags::UnorderedAccess |
                resource::BufferUsageFlags::DrawIndirect
        );
        const resource::TextureCreateDesc texDesc =
            resource::TextureCreateDesc::create2D("CmdRT")
                .setExtent(64, 64)
                .setFormat(PixelFormat::R8G8B8A8_UNorm)
                .addFlags(
                    resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource
                );

        resource::BufferRef bufA = cmdList.createBuffer(bufDesc);
        resource::BufferRef bufB = cmdList.createBuffer(bufDesc);
        resource::TextureRef texA = cmdList.createTexture(texDesc);
        resource::TextureRef texB = cmdList.createTexture(texDesc);

        view::ShaderResourceViewRef srv = cmdList.createShaderResourceView(
            bufA.get(), view::ViewDesc::createBufferSRV().setType(view::BufferViewType::Raw).asViewDesc()
        );
        view::UnorderedAccessViewRef uav = cmdList.createUnorderedAccessView(
            bufA.get(), view::ViewDesc::createBufferUAV().setType(view::BufferViewType::Raw).asViewDesc()
        );
        shader::ComputeShaderRef cs = m_rhi->createComputeShader({}, {});
        shader::VertexShaderRef vs = m_rhi->createVertexShader({}, {});
        shader::PixelShaderRef ps = m_rhi->createPixelShader({}, {});
        pipeline::VertexDeclarationRef vdecl = m_rhi->createVertexDeclaration({});
        pipeline::BlendStateRef blend = m_rhi->createBlendState({});
        pipeline::RasterizerStateRef raster = m_rhi->createRasterizerState({});
        pipeline::DepthStencilStateRef depth = m_rhi->createDepthStencilState({});
        pipeline::ComputePipelineStateRef computePSO = m_rhi->createComputePipelineState(cs.get());

        resource::UniformBufferLayoutDesc ubLayoutDesc("CmdUB");
        ubLayoutDesc.constantBufferSize = 16u;
        ubLayoutDesc.staticSlot = 0u;
        resource::UniformBufferLayoutRef ubLayout = createUniformBufferLayout(ubLayoutDesc);
        resource::UniformBufferRef ub = m_rhi->createUniformBuffer(
            nullptr,
            ubLayout.get(),
            resource::UniformBufferUsage::SingleFrame,
            resource::UniformBufferValidation::ValidateResources
        );

        sync::GPUFenceRef fence = m_rhi->createGPUFence(gp::Name("CmdFence"));
        resource::StagingBufferRef staging = m_rhi->createStagingBuffer();
        sync::RenderQueryRef query = m_rhi->createRenderQuery(sync::RenderQueryType::Occlusion);
        resource::StreamSourceSlotRef streamSlot = resource::StreamSourceSlot::create(bufA.get());
        streamSlot->update(bufA.get(), 32u);

        // Compute commands
        cmdList.setComputePipelineState(computePSO.get());
        cmdList.setAsyncComputeBudget(AsyncComputeBudget::Balanced);
        cmdList.setComputeBudget(SyncComputeBudget::ComputeHeavy);
        cmdList.setLowLatencyHint(true);
        cmdList.dispatchComputeShader(4u, 2u, 1u);
        cmdList.dispatchIndirectComputeShader(bufA.get(), 16u);
        cmdList.clearUAVFloat(uav.get(), gp::math::Vector4<gp::Float32>(1.0f, 0.0f, 0.0f, 1.0f));
        cmdList.clearUAVUint(uav.get(), resource::UIntVector4(1u, 2u, 3u, 4u));
        cmdList.setShaderRootConstants(resource::UIntVector4(9u, 8u, 7u, 6u));

        shader::BatchedShaderParameters& computeParams = cmdList.getScratchShaderParameters();
        const gp::UInt32 val = 42u;
        computeParams.setShaderParameter(0u, 0u, sizeof(val), &val);
        cmdList.setBatchedShaderParameters(cs.get(), computeParams);
        EXPECT_FALSE(computeParams.hasParameters());

        shader::BatchedShaderUnbinds& computeUnbinds = cmdList.getScratchShaderUnbinds();
        computeUnbinds.unsetUAV(0u);
        cmdList.setBatchedShaderUnbinds(cs.get(), computeUnbinds);
        EXPECT_FALSE(computeUnbinds.hasParameters());

        cmdList.setStaticUniformBuffer(0u, ub.get());
        cmdList.setUniformBlockOffset(srv.get(), 16u);
        cmdList.copyToStagingBuffer(bufA.get(), staging.get(), 0u, 64u);
        cmdList.writeGPUFence(fence.get());
        cmdList.postExternalCommandsReset();

        // Transitions & tracked access
        const sync::TransitionInfo tInfo(bufA.get(), sync::ResourceAccess::Unknown, sync::ResourceAccess::UAVCompute);
        cmdList.transition(tInfo);
        const sync::TrackedAccessInfo tracked(
            bufB.get(), sync::ResourceAccess::SRVGraphics, sync::PipelineStage::Graphics
        );
        cmdList.setTrackedAccess(gp::ConstVectorView<sync::TrackedAccessInfo>(&tracked, 1u));

        // Render pass & graphics commands
        const RenderPassInfo rpInfo(texA.get(), RenderTargetActions::ClearStore);
        cmdList.beginRenderPass(rpInfo, "TestPass");
        EXPECT_TRUE(cmdList.isInsideRenderPass());

        pipeline::GraphicsPipelineStateInitializer graphicsInit{};
        graphicsInit.boundShaderState = pipeline::BoundShaderStateInput(vdecl.get(), vs.get(), ps.get());
        graphicsInit.blendState = blend.get();
        graphicsInit.rasterizerState = raster.get();
        graphicsInit.depthStencilState = depth.get();
        graphicsInit.depthBounds = true;
        cmdList.applyCachedRenderTargets(graphicsInit);
        EXPECT_EQ(graphicsInit.renderTargetsEnabled, 1u);
        EXPECT_EQ(graphicsInit.renderTargetFormats[0], PixelFormat::R8G8B8A8_UNorm);

        pipeline::GraphicsPipelineStateRef graphicsPSO = m_rhi->createGraphicsPipelineState(graphicsInit);
        cmdList.setGraphicsPipelineState(graphicsPSO.get(), 7u, true);

        shader::BatchedShaderParameters& gfxParams = cmdList.getScratchShaderParameters();
        gfxParams.setShaderParameter(0u, 0u, sizeof(val), &val);
        cmdList.setBatchedShaderParameters(ps.get(), gfxParams);

        shader::BatchedShaderUnbinds& gfxUnbinds = cmdList.getScratchShaderUnbinds();
        gfxUnbinds.unsetSRV(0u);
        cmdList.setBatchedShaderUnbinds(ps.get(), gfxUnbinds);

        cmdList.setStencilRef(11u);
        cmdList.setBlendFactor(gp::math::LinearColor{ 0.5f, 0.5f, 0.5f, 1.0f });
        cmdList.setStreamSource(0u, bufA.get(), 8u);
        cmdList.setStreamSourceSlot(1u, streamSlot.get(), 16u);
        cmdList.setViewport(0.0f, 0.0f, 0.0f, 64.0f, 64.0f, 1.0f);
        cmdList.setStereoViewport(0.0f, 32.0f, 0.0f, 0.0f, 0.0f, 32.0f, 64.0f, 64.0f, 64.0f, 1.0f);
        const ViewportBounds multiVP[2] = {
            ViewportBounds{  0.0f, 0.0f, 32.0f, 32.0f, 0.0f, 1.0f },
            ViewportBounds{ 32.0f, 0.0f, 32.0f, 32.0f, 0.0f, 1.0f }
        };
        cmdList.setMultipleViewports(2u, multiVP);
        cmdList.setScissorRect(true, 0u, 0u, 64u, 64u);
        cmdList.setDepthBounds(0.1f, 0.9f);
        cmdList.setShadingRate(pipeline::VRSShadingRate::Rate2x2, pipeline::VRSRateCombiner::Override);

        cmdList.beginRenderQuery(query.get());
        cmdList.drawPrimitive(0u, 12u, 1u);
        cmdList.drawPrimitiveIndirect(bufA.get(), 0u);
        cmdList.drawIndexedPrimitive(bufB.get(), 0, 0u, 36u, 0u, 12u, 1u);
        cmdList.drawIndexedPrimitiveIndirect(bufB.get(), bufA.get(), 0u);
        cmdList.multiDrawIndexedPrimitiveIndirect(bufB.get(), bufA.get(), 0u, bufA.get(), 64u, 4u);
        cmdList.dispatchMeshShader(2u, 1u, 1u);
        cmdList.dispatchIndirectMeshShader(bufA.get(), 0u);
        cmdList.endRenderQuery(query.get());
        cmdList.nextSubpass();
        cmdList.endRenderPass();
        EXPECT_FALSE(cmdList.isInsideRenderPass());

        CopyTextureInfo copyTexInfo{};
        copyTexInfo.size = resource::IntVector(64, 64, 1);
        cmdList.copyTexture(texA.get(), texB.get(), copyTexInfo);
        cmdList.copyBufferRegion(bufB.get(), 0u, bufA.get(), 0u, 64u);
        cmdList.resummarizeHTile(texA.get());

        if (!enableBypass)
        {
            EXPECT_TRUE(cmdList.hasCommands());
            EXPECT_GT(cmdList.getUsedMemory(), 0u);
            EXPECT_EQ(fence->getNumPendingWriteCommands(), 1);
            EXPECT_EQ(m_rhi->graphicsLog.drawPrimitiveCalls, 0u);

            cmdList.immediateFlush(ImmediateFlushType::FlushRHIThread);
            EXPECT_FALSE(cmdList.hasCommands());
            EXPECT_EQ(cmdList.getUsedMemory(), 0u);
        }

        EXPECT_EQ(fence->getNumPendingWriteCommands(), 0);
        EXPECT_TRUE(fence->poll());

        const RecordedCalls& log = m_rhi->graphicsLog;
        EXPECT_EQ(log.computePSOBinds, 1u);
        EXPECT_EQ(log.computeShaderFallbackBinds, 1u);
        EXPECT_EQ(log.dispatches, 1u);
        EXPECT_EQ(log.lastDispatchX, 4u);
        EXPECT_EQ(log.indirectDispatches, 1u);
        EXPECT_EQ(log.lastAsyncBudget, AsyncComputeBudget::Balanced);
        EXPECT_EQ(log.lastSyncBudget, SyncComputeBudget::ComputeHeavy);
        EXPECT_TRUE(log.lastLowLatencyHint);
        EXPECT_EQ(log.beginTransitionsCalls, 1u);
        EXPECT_EQ(log.endTransitionsCalls, 1u);
        EXPECT_EQ(log.clearUAVFloatCalls, 1u);
        EXPECT_EQ(log.clearUAVUintCalls, 1u);
        EXPECT_EQ(log.rootConstantCalls, 1u);
        EXPECT_EQ(log.lastRootConstants.x, 9u);
        EXPECT_EQ(log.computeParamCalls, 1u);
        EXPECT_EQ(log.computeUnbindCalls, 1u);
        EXPECT_EQ(log.staticUBCalls, 1u);
        EXPECT_EQ(log.uniformOffsetCalls, 1u);
        EXPECT_EQ(log.copyToStagingCalls, 1u);
        EXPECT_EQ(log.writeGPUFenceCalls, 1u);
        EXPECT_EQ(log.postExternalResets, 1u);
        EXPECT_EQ(log.graphicsPSOBinds, 1u);
        EXPECT_EQ(log.boundShaderStateBinds, 1u);
        EXPECT_EQ(log.depthStencilBinds, 1u);
        EXPECT_EQ(log.rasterizerBinds, 1u);
        EXPECT_EQ(log.blendBinds, 1u);
        EXPECT_EQ(log.depthBoundsEnableCalls, 1u);
        EXPECT_EQ(log.graphicsParamCalls, 1u);
        EXPECT_EQ(log.graphicsUnbindCalls, 1u);
        EXPECT_EQ(log.stencilRefCalls, 1u);
        EXPECT_EQ(log.lastStencilRef, 11u);
        EXPECT_EQ(log.blendFactorCalls, 1u);
        EXPECT_EQ(log.streamSourceCalls, 2u);
        EXPECT_EQ(log.lastStreamOffset, 48u);
        EXPECT_EQ(log.viewportCalls, 1u);
        EXPECT_EQ(log.stereoViewportCalls, 1u);
        EXPECT_EQ(log.multiViewportCalls, 1u);
        EXPECT_EQ(log.lastMultiViewportCount, 2u);
        EXPECT_EQ(log.scissorCalls, 1u);
        EXPECT_EQ(log.depthBoundsCalls, 1u);
        EXPECT_EQ(log.shadingRateCalls, 1u);
        EXPECT_EQ(log.drawPrimitiveCalls, 1u);
        EXPECT_EQ(log.drawPrimitiveIndirectCalls, 1u);
        EXPECT_EQ(log.drawIndexedCalls, 1u);
        EXPECT_EQ(log.lastDrawPrimitives, 12u);
        EXPECT_EQ(log.drawIndexedIndirectCalls, 1u);
        EXPECT_EQ(log.multiDrawIndexedIndirectCalls, 1u);
        EXPECT_EQ(log.dispatchMeshCalls, 1u);
        EXPECT_EQ(log.dispatchIndirectMeshCalls, 1u);
        EXPECT_EQ(log.beginRenderPassCalls, 1u);
        EXPECT_EQ(log.endRenderPassCalls, 1u);
        EXPECT_EQ(log.nextSubpassCalls, 1u);
        EXPECT_EQ(log.copyTextureCalls, 1u);
        EXPECT_EQ(log.copyBufferRegionCalls, 1u);
        EXPECT_EQ(log.resummarizeHTileCalls, 1u);
        EXPECT_EQ(log.beginRenderQueryCalls, 1u);
        EXPECT_EQ(log.endRenderQueryCalls, 1u);
        EXPECT_EQ(bufB->getTrackedAccessUnsafe(), sync::ResourceAccess::SRVGraphics);
    }

    RecordingMockRHI* m_rhi{ nullptr };
};

TEST_F(CommandListFixture, ZeroOverheadHierarchySizes)
{
    static_assert(sizeof(ComputeCommandList) == sizeof(CommandListBase));
    static_assert(sizeof(CommandList) == sizeof(CommandListBase));
    static_assert(sizeof(CommandListImmediate) == sizeof(CommandListBase));
    SUCCEED();
}

TEST_F(CommandListFixture, AllCommandsReplayIdenticallyInQueuedMode)
{
    exerciseAllCommands(false);
}

TEST_F(CommandListFixture, AllCommandsExecuteDirectlyInBypassMode)
{
    exerciseAllCommands(true);
}

TEST_F(CommandListFixture, ArenaHelpersAndResetDestroysUnexecutedLambdaCaptures)
{
    CommandList deferredList{};
    const gp::UInt32 srcValue = 0xCAFEBABEu;
    const gp::UInt32* const copiedValue =
        static_cast<const gp::UInt32*>(deferredList.allocCopy(&srcValue, sizeof(srcValue), alignof(gp::UInt32)));
    ASSERT_NE(copiedValue, nullptr);
    EXPECT_EQ(*copiedValue, 0xCAFEBABEu);

    const char* const copiedString = deferredList.allocString("RenderPassA");
    ASSERT_NE(copiedString, nullptr);
    EXPECT_STREQ(copiedString, "RenderPassA");

    const gp::UInt32 srcArr[4] = { 10u, 20u, 30u, 99u };
    const gp::ConstVectorView<gp::UInt32> copiedArr =
        deferredList.allocArray(gp::ConstVectorView<gp::UInt32>(srcArr, 4u));
    ASSERT_EQ(copiedArr.size(), 4u);
    EXPECT_EQ(copiedArr[3], 99u);

    const resource::BufferCreateDesc bufDesc =
        resource::BufferCreateDesc::create("LambdaCapBuf", 16u, 4u, resource::BufferUsageFlags::Static);
    resource::BufferRef buffer(new MockBuffer(bufDesc));
    EXPECT_EQ(buffer->getRefCount(), 1u);

    deferredList.enqueueLambda(
        [captured = buffer](CommandList&) noexcept -> void
    {
        static_cast<void>(captured);
    }
    );
    EXPECT_EQ(buffer->getRefCount(), 2u);

    sync::GPUFenceRef discardedFence = m_rhi->createGPUFence(gp::Name("DiscardedFence"));
    deferredList.writeGPUFence(discardedFence.get());
    EXPECT_EQ(discardedFence->getNumPendingWriteCommands(), 1);

    const sync::TransitionInfo discardedTrans(
        buffer.get(), sync::ResourceAccess::Unknown, sync::ResourceAccess::SRVGraphics
    );
    deferredList.transition(discardedTrans);

    // Discarding an unexecuted command list must invoke packet destructors, release fences, and free transitions.
    deferredList.reset();
    EXPECT_EQ(buffer->getRefCount(), 1u);
    EXPECT_EQ(discardedFence->getNumPendingWriteCommands(), 0);
    EXPECT_FALSE(deferredList.hasCommands());
}

TEST_F(CommandListFixture, PipelineSwitchingMultiPipeTransitionsAndScopes)
{
    CommandListImmediate& cmdList = getImmediateCommandList();
    const resource::BufferCreateDesc bufDesc =
        resource::BufferCreateDesc::create("MultiPipeBuf", 64u, 4u, resource::BufferUsageFlags::UnorderedAccess);
    resource::BufferRef buf = cmdList.createBuffer(bufDesc);

    {
        CommandListScopedPipeline asyncScope(cmdList, sync::PipelineStage::AsyncCompute);
        EXPECT_EQ(cmdList.getPipeline(), sync::PipelineStage::AsyncCompute);
        cmdList.dispatchComputeShader(8u, 1u, 1u);
    }
    EXPECT_EQ(cmdList.getPipeline(), sync::PipelineStage::Graphics);

    // Multi-pipe transition from Graphics to AsyncCompute hits both contexts on replay.
    const sync::TransitionInfo tInfo(buf.get(), sync::ResourceAccess::UAVGraphics, sync::ResourceAccess::UAVCompute);
    cmdList.transition(
        gp::ConstVectorView<sync::TransitionInfo>(&tInfo, 1u),
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute
    );

    bool multiPipeLambdaRan = false;
    cmdList.enqueueLambdaMultiPipe(
        [&multiPipeLambdaRan](sync::PipelineArray<IComputeContext*>& contexts) noexcept -> void
    {
        multiPipeLambdaRan = (contexts[sync::PipelineStage::Graphics] != nullptr) &&
                             (contexts[sync::PipelineStage::AsyncCompute] != nullptr);
    }
    );

    // Scoped GPU mask and static uniform buffers.
    {
        ScopedGPUMask maskScope(cmdList, GPUMask::gpu0());
        EXPECT_EQ(cmdList.getGPUMask(), GPUMask::gpu0());
    }
    EXPECT_EQ(cmdList.getGPUMask(), GPUMask::all());

    {
        shader::UniformBufferStaticBindings tempBindings{};
        tempBindings.addUniformBufferAtSlot(2u, nullptr);
        ScopedUniformBufferStaticBindings ubScope(cmdList, tempBindings);
        EXPECT_EQ(cmdList.getStaticUniformBuffers().getUniformBufferCount(), 1u);
        EXPECT_EQ(cmdList.getStaticUniformBuffers().getSlotCount(), 3u);
    }
    EXPECT_EQ(cmdList.getStaticUniformBuffers().getUniformBufferCount(), 0u);
    EXPECT_EQ(cmdList.getStaticUniformBuffers().getSlotCount(), 0u);

    {
        CommandListScopedFlushAndExecute flushGuard(cmdList, ImmediateFlushType::FlushRHIThread);
        EXPECT_TRUE(cmdList.isExecuting());
        EXPECT_TRUE(multiPipeLambdaRan);
    }
    EXPECT_FALSE(cmdList.isExecuting());
    EXPECT_EQ(m_rhi->asyncComputeLog.dispatches, 1u);
    EXPECT_EQ(m_rhi->asyncComputeLog.lastDispatchX, 8u);
    EXPECT_EQ(m_rhi->graphicsLog.beginTransitionsCalls, 1u);
    EXPECT_EQ(m_rhi->asyncComputeLog.endTransitionsCalls, 1u);
}

TEST_F(CommandListFixture, DeferredCommandListSubmitAndLifetimeExtension)
{
    gp::thread::ScopedNamedThread renderRole(gp::thread::NamedThread::Render);
    ASSERT_TRUE(renderRole.isAttached());

    CommandList deferredList{};
    deferredList.dispatchComputeShader(3u, 1u, 1u);
    deferredList.finishRecording();
    EXPECT_TRUE(deferredList.isFinished());

    CommandListImmediate& immediate = getImmediateCommandList();
    CommandListBase* const listPtr = &deferredList;
    immediate.queueAsyncCommandListSubmit(gp::ConstVectorView<CommandListBase*>(&listPtr, 1u));
    EXPECT_EQ(m_rhi->graphicsLog.dispatches, 1u);
    EXPECT_EQ(m_rhi->graphicsLog.lastDispatchX, 3u);
    EXPECT_EQ(m_rhi->finalizeCalls, 1u);
    EXPECT_EQ(m_rhi->submitCalls, 1u);

    // Verify lifetime extension defers deletion until the outermost scope exits.
    const resource::BufferCreateDesc bufDesc =
        resource::BufferCreateDesc::create("DeferredDelBuf", 16u, 4u, resource::BufferUsageFlags::Static);
    {
        CommandListScopedExtendResourceLifetime extendScope(immediate);
        EXPECT_GT(resource::ResourceDeletionQueue::getLifetimeRefCount(), 0u);
        resource::BufferRef transientBuf = immediate.createBuffer(bufDesc);
        transientBuf.reset();
        gp::Vector<resource::Resource*> pending{};
        resource::ResourceDeletionQueue::gatherResourcesToDelete(pending, false);
        EXPECT_TRUE(pending.isEmpty());
    }
    EXPECT_EQ(resource::ResourceDeletionQueue::getLifetimeRefCount(), 0u);
    gp::Vector<resource::Resource*> remaining{};
    resource::ResourceDeletionQueue::gatherResourcesToDelete(remaining, true);
    EXPECT_TRUE(remaining.isEmpty());
}

TEST_F(CommandListFixture, LatchBypassAndFrameLifecycle)
{
    CommandListExecutor& executor = getCommandListExecutor();
    executor.setBypassRequested(true);
    EXPECT_TRUE(executor.isBypassRequested());
    EXPECT_FALSE(executor.bypass());

    executor.latchBypass();
    EXPECT_TRUE(executor.bypass());

    CommandListImmediate& immediate = getImmediateCommandList();
    presentation::ViewportRef vp = m_rhi->createViewport(nullptr, 64u, 64u, false, PixelFormat::B8G8R8A8_UNorm);

    immediate.beginFrame();
    immediate.beginDrawingViewport(vp.get(), nullptr);
    EXPECT_TRUE(immediate.isInsideDrawingViewport());
    immediate.endDrawingViewport(vp.get(), true, false);
    EXPECT_FALSE(immediate.isInsideDrawingViewport());
    immediate.endFrame();
    immediate.blockUntilGPUIdle(true);

    EXPECT_EQ(m_rhi->beginFrameCalls, 1u);
    EXPECT_EQ(m_rhi->endFrameCalls, 1u);
    EXPECT_EQ(m_rhi->beginViewportCalls, 1u);
    EXPECT_EQ(m_rhi->endViewportCalls, 1u);
    EXPECT_EQ(m_rhi->blockIdleCalls, 1u);
}

}   // namespace

}   // namespace gp::rhi::command::tests
