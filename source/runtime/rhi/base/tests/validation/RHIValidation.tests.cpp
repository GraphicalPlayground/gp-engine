// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ExclusiveDepthStencil.hpp"
#include "core/GPUMask.hpp"
#include "core/ResourceDeletionQueue.hpp"   // IWYU pragma: keep
#include "device/DynamicRHI.hpp"
#include "device/DynamicRHIPSOFallback.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/vector/Vector4.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/SamplerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "presentation/PresentArgs.hpp"
#include "presentation/Viewport.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
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
#include "shaders/ShaderTypes.hpp"
#include "shaders/VertexShader.hpp"
#include "sync/GPUFence.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/Transition.hpp"
#include "sync/TransitionDesc.hpp"
#include "validation/ValidationRHI.hpp"
#include "validation/ValidationTypes.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::validation::tests
{

#if GP_ENABLE_RHI_VALIDATION

namespace
{

class ValTestBuffer final : public resource::Buffer
{
public:
    explicit ValTestBuffer(const resource::BufferCreateDesc& desc) noexcept
        : Buffer(desc)
    {}

protected:
    virtual ~ValTestBuffer() noexcept override = default;
};

class ValTestTexture final : public resource::Texture
{
public:
    explicit ValTestTexture(const resource::TextureCreateDesc& desc) noexcept
        : Texture(desc)
    {}

protected:
    virtual ~ValTestTexture() noexcept override = default;
};

class ValTestUniformBuffer final : public resource::UniformBuffer
{
public:
    explicit ValTestUniformBuffer(const resource::UniformBufferLayout* const layout) noexcept
        : UniformBuffer(layout)
    {}

protected:
    virtual ~ValTestUniformBuffer() noexcept override = default;
};

class ValTestFence final : public sync::GPUFence
{
public:
    explicit ValTestFence(const gp::Name name) noexcept
        : GPUFence(name)
    {}

    virtual void clear() noexcept override
    {}

    [[nodiscard]] virtual bool poll() const noexcept override
    {
        return true;
    }

    virtual void wait(command::CommandListImmediate&, GPUMask) const noexcept override
    {}

protected:
    virtual ~ValTestFence() noexcept override = default;
};

class ValAsyncComputeMockContext final : public command::IComputeContext
{
public:
    virtual void setComputePipelineState(pipeline::ComputePipelineState*) noexcept override
    {}

    virtual void dispatchComputeShader(gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void dispatchIndirectComputeShader(resource::Buffer*, gp::UInt32) noexcept override
    {}

    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {}

    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {}

    virtual void clearUAVFloat(view::UnorderedAccessView*, const gp::math::Vector4<gp::Float32>&) noexcept override
    {}

    virtual void clearUAVUint(view::UnorderedAccessView*, const resource::UIntVector4&) noexcept override
    {}

    virtual void setShaderParameters(
        shader::ComputeShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {}

    virtual void
        setShaderUnbinds(shader::ComputeShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
    {}

    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings&) noexcept override
    {}

    virtual void
        copyToStagingBuffer(resource::Buffer*, resource::StagingBuffer*, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void writeGPUFence(sync::GPUFence*) noexcept override
    {}
};

class ValMockBackendRHI final
    : public DynamicRHIPSOFallback
    , public command::ICommandContextPSOFallback
{
public:
    ValAsyncComputeMockContext asyncComputeContext{};
    gp::Vector<gp::UInt8> scratchMap{};

    virtual void init() noexcept override
    {
        getRHIGlobals().supportsAsyncCompute = true;
        getRHIGlobals().maxDispatchThreadGroupsPerDimension = 65535u;
    }

    virtual void shutdown() noexcept override
    {}

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "ValMockBackend";
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
        createVertexShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash& hash) noexcept override
    {
        shader::VertexShaderRef s(new shader::VertexShader());
        s->setHash(hash);
        return s;
    }

    [[nodiscard]] virtual shader::PixelShaderRef
        createPixelShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash& hash) noexcept override
    {
        shader::PixelShaderRef s(new shader::PixelShader());
        s->setHash(hash);
        return s;
    }

    [[nodiscard]] virtual shader::GeometryShaderRef
        createGeometryShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash& hash) noexcept override
    {
        shader::GeometryShaderRef s(new shader::GeometryShader());
        s->setHash(hash);
        return s;
    }

    [[nodiscard]] virtual shader::ComputeShaderRef
        createComputeShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash& hash) noexcept override
    {
        shader::ComputeShaderRef s(new shader::ComputeShader());
        s->setHash(hash);
        return s;
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
        return resource::UniformBufferRef(new ValTestUniformBuffer(layout));
    }

    virtual void updateUniformBuffer(command::CommandListBase&, resource::UniformBuffer*, const void*) noexcept override
    {}

    [[nodiscard]] virtual resource::BufferInitializer createBufferInitializer(
        command::CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
    ) noexcept override
    {
        resource::BufferRef buf(new ValTestBuffer(createDesc));
        return resource::BufferInitializer(
            cmdList, std::move(buf), nullptr, 0u, resource::BufferInitializer::FinalizeCallback{}
        );
    }

    [[nodiscard]] virtual void* lockBufferBottomOfPipe(
        command::CommandListBase&,
        resource::Buffer*,
        const gp::UInt32 offset,
        const gp::UInt32 size,
        resource::ResourceLockMode
    ) noexcept override
    {
        const gp::USize total = static_cast<gp::USize>(offset) + static_cast<gp::USize>(size);
        if (scratchMap.size() < total)
        {
            scratchMap.resize(total, 0u);
        }
        return scratchMap.data() + offset;
    }

    virtual void unlockBufferBottomOfPipe(command::CommandListBase&, resource::Buffer*) noexcept override
    {}

    [[nodiscard]] virtual resource::TextureInitializer createTextureInitializer(
        command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
    ) noexcept override
    {
        resource::TextureRef tex(new ValTestTexture(createDesc));
        return resource::TextureInitializer(
            cmdList,
            std::move(tex),
            resource::TextureInitializer::SubresourceCallback{},
            resource::TextureInitializer::FinalizeCallback{}
        );
    }

    [[nodiscard]] virtual resource::TextureLockResult
        lockTexture(command::CommandListImmediate&, const resource::TextureLockArgs&) noexcept override
    {
        return resource::TextureLockResult{};
    }

    virtual void unlockTexture(command::CommandListImmediate&, const resource::TextureLockArgs&) noexcept override
    {}

    virtual void updateTexture2D(
        command::CommandListBase&,
        resource::Texture*,
        gp::UInt32,
        const resource::UpdateTextureRegion2D&,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    virtual void updateTexture3D(
        command::CommandListBase&,
        resource::Texture*,
        gp::UInt32,
        const resource::UpdateTextureRegion3D&,
        gp::UInt32,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    using DynamicRHI::readSurfaceData;

    virtual void readSurfaceData(
        resource::Texture*,
        const resource::IntRect&,
        gp::Vector<gp::math::Color>&,
        const resource::ReadSurfaceDataFlags&
    ) noexcept override
    {}

    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase&, resource::Buffer* const buf, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::ShaderResourceViewRef(new view::ShaderResourceView(buf, desc));
    }

    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase&, resource::Texture* const tex, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::ShaderResourceViewRef(new view::ShaderResourceView(tex, desc));
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase&, resource::Buffer* const buf, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::UnorderedAccessViewRef(new view::UnorderedAccessView(buf, desc));
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase&, resource::Texture* const tex, const view::ViewDesc& desc
    ) noexcept override
    {
        return view::UnorderedAccessViewRef(new view::UnorderedAccessView(tex, desc));
    }

    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(const gp::Name name) noexcept override
    {
        return sync::GPUFenceRef(new ValTestFence(name));
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
        resource::TextureCreateDesc desc =
            resource::TextureCreateDesc::create2D("BackBuffer", 64, 64, PixelFormat::B8G8R8A8_UNorm);
        desc.flags = resource::TextureCreateFlags::RenderTargetable;
        desc.initialState = sync::ResourceAccess::RTV;
        return resource::TextureRef(new ValTestTexture(desc));
    }

    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport*) noexcept override
    {}

    virtual void beginDrawingViewport(presentation::Viewport*, resource::Texture*) noexcept override
    {}

    using DynamicRHI::endDrawingViewport;

    virtual void
        endDrawingViewport(presentation::Viewport*, const presentation::PresentArgsWithStats&) noexcept override
    {}

    virtual void blockUntilGPUIdle() noexcept override
    {}

    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override
    {
        return this;
    }

    [[nodiscard]] virtual command::IComputeContext* getDefaultAsyncComputeContext() noexcept override
    {
        return &asyncComputeContext;
    }

    virtual void dispatchComputeShader(gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void dispatchIndirectComputeShader(resource::Buffer*, gp::UInt32) noexcept override
    {}

    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {}

    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*>) noexcept override
    {}

    virtual void clearUAVFloat(view::UnorderedAccessView*, const gp::math::Vector4<gp::Float32>&) noexcept override
    {}

    virtual void clearUAVUint(view::UnorderedAccessView*, const resource::UIntVector4&) noexcept override
    {}

    virtual void setShaderParameters(
        shader::ComputeShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {}

    virtual void setShaderParameters(
        shader::GraphicsShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {}

    virtual void
        setShaderUnbinds(shader::ComputeShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
    {}

    virtual void
        setShaderUnbinds(shader::GraphicsShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
    {}

    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings&) noexcept override
    {}

    virtual void
        copyToStagingBuffer(resource::Buffer*, resource::StagingBuffer*, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void writeGPUFence(sync::GPUFence*) noexcept override
    {}

    virtual void beginRenderQuery(sync::RenderQuery*) noexcept override
    {}

    virtual void endRenderQuery(sync::RenderQuery*) noexcept override
    {}

    virtual void setStreamSource(gp::UInt32, resource::Buffer*, gp::UInt32) noexcept override
    {}

    virtual void
        setViewport(gp::Float32, gp::Float32, gp::Float32, gp::Float32, gp::Float32, gp::Float32) noexcept override
    {}

    virtual void setScissorRect(bool, gp::UInt32, gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void drawPrimitive(gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void drawPrimitiveIndirect(resource::Buffer*, gp::UInt32) noexcept override
    {}

    virtual void drawIndexedPrimitive(
        resource::Buffer*, gp::Int32, gp::UInt32, gp::UInt32, gp::UInt32, gp::UInt32, gp::UInt32
    ) noexcept override
    {}

    virtual void drawIndexedPrimitiveIndirect(resource::Buffer*, resource::Buffer*, gp::UInt32) noexcept override
    {}

    virtual void beginRenderPass(const command::RenderPassInfo&, const char*) noexcept override
    {}

    virtual void endRenderPass() noexcept override
    {}

    virtual void copyTexture(resource::Texture*, resource::Texture*, const command::CopyTextureInfo&) noexcept override
    {}

    virtual void
        copyBufferRegion(resource::Buffer*, gp::UInt64, resource::Buffer*, gp::UInt64, gp::UInt64) noexcept override
    {}

    virtual void setBoundShaderState(pipeline::BoundShaderState*) noexcept override
    {}

    virtual void setDepthStencilState(pipeline::DepthStencilState*, gp::UInt32) noexcept override
    {}

    virtual void setRasterizerState(pipeline::RasterizerState*) noexcept override
    {}

    virtual void setBlendState(pipeline::BlendState*, const gp::math::LinearColor&) noexcept override
    {}

    virtual void enableDepthBoundsTest(bool, gp::Float32, gp::Float32) noexcept override
    {}

    virtual void setComputeShader(shader::ComputeShader*) noexcept override
    {}
};

class RHIValidationFixture : public ::testing::Test
{
protected:
    ValMockBackendRHI* m_backend{ nullptr };

    virtual void SetUp() override
    {
        if (hasValidDynamicRHI())
        {
            rhiExit();
        }
        g_rhiValidationBreakOnError = false;
        g_rhiValidationDeduplicateErrors = false;
        clearValidationFailures();

        m_backend = new ValMockBackendRHI();
        rhiInit(new ValidationRHI(m_backend));
        rhiPostInit();
    }

    virtual void TearDown() override
    {
        clearValidationFailures();
        setValidationFailureCallback(nullptr, nullptr);
        if (hasValidDynamicRHI())
        {
            rhiExit();
        }
        m_backend = nullptr;
    }
};

}   // namespace

TEST_F(RHIValidationFixture, UnwrapsBackendRHIAndValidatesValidRenderPassAndTransitions)
{
    EXPECT_TRUE(g_rhiValidationEnabled);
    EXPECT_EQ(castDynamicRHI<ValMockBackendRHI>(), m_backend);

    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    resource::TextureCreateDesc rtDesc =
        resource::TextureCreateDesc::create2D("ColorTarget", 128, 128, PixelFormat::R8G8B8A8_UNorm);
    rtDesc.flags = resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource;
    rtDesc.initialState = sync::ResourceAccess::RTV;
    resource::TextureRef colorRT = cmdList.createTexture(rtDesc);

    const gp::UInt8 dummyCode[4] = { 1u, 2u, 3u, 4u };
    shader::VertexShaderRef vs = getDynamicRHI()->createVertexShader(dummyCode, shader::ShaderHash{});
    shader::PixelShaderRef ps = getDynamicRHI()->createPixelShader(dummyCode, shader::ShaderHash{});

    pipeline::GraphicsPipelineStateInitializer psoInit{};
    psoInit.boundShaderState.setVertexShader(vs.get());
    psoInit.boundShaderState.setPixelShader(ps.get());
    psoInit.renderTargetsEnabled = 1u;
    psoInit.renderTargetFormats[0] = PixelFormat::R8G8B8A8_UNorm;
    psoInit.renderTargetFlags[0] = rtDesc.flags;
    psoInit.numSamples = 1u;
    pipeline::GraphicsPipelineStateRef pso = getDynamicRHI()->createGraphicsPipelineState(psoInit);

    const command::RenderPassInfo rpInfo(colorRT.get(), command::RenderTargetActions::ClearStore);
    cmdList.beginRenderPass(rpInfo, "ValidPass");
    cmdList.setGraphicsPipelineState(pso.get());
    cmdList.drawPrimitive(0u, 1u, 1u);
    cmdList.endRenderPass();

    // Transition RTV -> SRVGraphicsPixel and sample in a second pass.
    cmdList.transition(
        sync::TransitionInfo(colorRT.get(), sync::ResourceAccess::RTV, sync::ResourceAccess::SRVGraphicsPixel)
    );
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);

    EXPECT_EQ(getValidationFailureCount(), 0u);
}

TEST_F(RHIValidationFixture, DetectsMissingBarrierAndSplitTransitionViolations)
{
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    resource::TextureCreateDesc texDesc =
        resource::TextureCreateDesc::create2D("SampledTex", 64, 64, PixelFormat::R8G8B8A8_UNorm);
    texDesc.flags = resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource;
    texDesc.initialState = sync::ResourceAccess::RTV;
    resource::TextureRef tex = cmdList.createTexture(texDesc);

    resource::TextureCreateDesc dstDesc =
        resource::TextureCreateDesc::create2D("DestTex", 64, 64, PixelFormat::R8G8B8A8_UNorm);
    dstDesc.flags = resource::TextureCreateFlags::RenderTargetable;
    dstDesc.initialState = sync::ResourceAccess::CopyDest;
    resource::TextureRef dstTex = cmdList.createTexture(dstDesc);

    // 1. `tex` is in RTV state, NOT CopySrc -> copyTexture must flag a missing barrier!
    cmdList.copyTexture(tex.get(), dstTex.get(), command::CopyTextureInfo{});
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_GE(getValidationFailureCount(), 1u);
    EXPECT_NE(getLastValidationFailureMessage().view().find("missing barrier"), gp::StringView::npos);

    clearValidationFailures();

    // 2. Begin a split transition on `tex` (RTV -> CopySrc) and access it BEFORE calling `endTransition`!
    const sync::TransitionInfo splitInfo(tex.get(), sync::ResourceAccess::RTV, sync::ResourceAccess::CopySrc);
    const sync::Transition* const splitTr = createTransition(
        sync::TransitionCreateInfo(
            sync::PipelineStage::Graphics,
            sync::PipelineStage::Graphics,
            sync::TransitionCreateFlags::None,
            gp::ConstVectorView<sync::TransitionInfo>(&splitInfo, 1u)
        )
    );
    ASSERT_NE(splitTr, nullptr);

    cmdList.beginTransition(splitTr);
    cmdList.copyTexture(tex.get(), dstTex.get(), command::CopyTextureInfo{});
    cmdList.endTransition(splitTr);
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);

    EXPECT_GE(getValidationFailureCount(), 1u);
    EXPECT_NE(getLastValidationFailureMessage().view().find("split transition"), gp::StringView::npos);
}

TEST_F(RHIValidationFixture, ValidatesSubresourceTransitionsAndCrossPipelineFences)
{
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    resource::TextureCreateDesc mipTexDesc =
        resource::TextureCreateDesc::create2D("MipChainTex", 64, 64, PixelFormat::R16G16B16A16_Float);
    mipTexDesc.numMips = 2u;
    mipTexDesc.flags = resource::TextureCreateFlags::UAV | resource::TextureCreateFlags::ShaderResource;
    mipTexDesc.initialState = sync::ResourceAccess::SRVCompute;
    resource::TextureRef mipTex = cmdList.createTexture(mipTexDesc);

    // Transition ONLY mip 1 to UAVCompute while mip 0 stays in SRVCompute.
    const sync::TransitionInfo mip1ToUAV(
        mipTex.get(),
        sync::ResourceAccess::SRVCompute,
        sync::ResourceAccess::UAVCompute,
        sync::ResourceTransitionFlags::None,
        1u
    );
    cmdList.transition(mip1ToUAV);

    view::UnorderedAccessViewRef uavMip1 = cmdList.createUnorderedAccessView(
        mipTex.get(),
        view::ViewDesc::createTextureUAV().setDimensionFromTexture(mipTex.get()).setMipLevel(1u).asViewDesc()
    );
    view::UnorderedAccessViewRef uavMip0 = cmdList.createUnorderedAccessView(
        mipTex.get(),
        view::ViewDesc::createTextureUAV().setDimensionFromTexture(mipTex.get()).setMipLevel(0u).asViewDesc()
    );

    // Clearing mip 1 succeeds; clearing mip 0 (still in SRVCompute) fails!
    cmdList.clearUAVUint(uavMip1.get(), resource::UIntVector4(0u));
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_EQ(getValidationFailureCount(), 0u);

    cmdList.clearUAVUint(uavMip0.get(), resource::UIntVector4(0u));
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_EQ(getValidationFailureCount(), 1u);
    clearValidationFailures();

    // Cross-pipeline Graphics -> AsyncCompute transition on a buffer.
    resource::BufferCreateDesc bufDesc = resource::BufferCreateDesc::createByteAddress("CrossPipeBuf", 256u, 4u);
    bufDesc.usage |= resource::BufferUsageFlags::UnorderedAccess;
    bufDesc.initialState = sync::ResourceAccess::UAVGraphics;
    resource::BufferRef crossBuf = cmdList.createBuffer(bufDesc);
    view::UnorderedAccessViewRef bufUAV = cmdList.createUnorderedAccessView(
        crossBuf.get(), view::ViewDesc::createBufferUAV().setTypeFromBuffer(crossBuf.get()).asViewDesc()
    );

    const sync::TransitionInfo crossInfo(
        crossBuf.get(), sync::ResourceAccess::UAVGraphics, sync::ResourceAccess::UAVCompute
    );
    cmdList.transition(
        gp::ConstVectorView<sync::TransitionInfo>(&crossInfo, 1u),
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute
    );

    {
        command::CommandListScopedPipeline asyncScope(cmdList, sync::PipelineStage::AsyncCompute);
        cmdList.clearUAVUint(bufUAV.get(), resource::UIntVector4(1u));
    }
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_EQ(getValidationFailureCount(), 0u);
}

TEST_F(RHIValidationFixture, ValidatesTransientAcquireDiscardAndAliasingOverlap)
{
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    resource::BufferCreateDesc descA = resource::BufferCreateDesc::createStructured("TransientA", 1024u, 16u);
    descA.usage |= resource::BufferUsageFlags::Transient | resource::BufferUsageFlags::UnorderedAccess;
    descA.initialState = sync::ResourceAccess::Discard;
    resource::BufferRef bufA = cmdList.createBuffer(descA);

    resource::BufferCreateDesc descB = resource::BufferCreateDesc::createStructured("TransientB", 1024u, 16u);
    descB.usage |= resource::BufferUsageFlags::Transient | resource::BufferUsageFlags::UnorderedAccess;
    descB.initialState = sync::ResourceAccess::Discard;
    resource::BufferRef bufB = cmdList.createBuffer(descB);

    // 1. Overlapping `bufA` BEFORE `bufA` was acquired+discarded must report a validation failure!
    {
        const sync::TransientAliasingOverlap badOverlap(bufA.get());
        const sync::TransientAliasingInfo badAcquire = sync::TransientAliasingInfo::acquire(
            bufB.get(), gp::ConstVectorView<sync::TransientAliasingOverlap>(&badOverlap, 1u)
        );
        const sync::TransitionInfo toUAV(bufB.get(), sync::ResourceAccess::Discard, sync::ResourceAccess::UAVCompute);
        const sync::Transition* const tr = createTransition(
            sync::TransitionCreateInfo(
                sync::PipelineStage::Graphics,
                sync::PipelineStage::Graphics,
                sync::TransitionCreateFlags::NoSplit,
                gp::ConstVectorView<sync::TransitionInfo>(&toUAV, 1u),
                gp::ConstVectorView<sync::TransientAliasingInfo>(&badAcquire, 1u)
            )
        );
        cmdList.beginTransition(tr);
        cmdList.endTransition(tr);
        cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
        EXPECT_GE(getValidationFailureCount(), 1u);
    }

    // Reset `bufB` by discarding it properly, then test valid `Acquire(A) -> Discard(A) -> Acquire(B, overlap=A)`.
    {
        const sync::TransientAliasingInfo discB = sync::TransientAliasingInfo::discard(bufB.get());
        const sync::Transition* const trDiscB = createTransition(
            sync::TransitionCreateInfo(
                sync::PipelineStage::Graphics,
                sync::PipelineStage::Graphics,
                sync::TransitionCreateFlags::NoSplit,
                {},
                gp::ConstVectorView<sync::TransientAliasingInfo>(&discB, 1u)
            )
        );
        cmdList.beginTransition(trDiscB);
        cmdList.endTransition(trDiscB);
        cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    }
    clearValidationFailures();

    // Valid lifecycle: Acquire(A) -> Discard(A) -> Acquire(B overlapping A).
    {
        const sync::TransientAliasingInfo acqA = sync::TransientAliasingInfo::acquire(bufA.get());
        const sync::TransitionInfo aToUAV(bufA.get(), sync::ResourceAccess::Discard, sync::ResourceAccess::UAVCompute);
        const sync::Transition* const tr1 = createTransition(
            sync::TransitionCreateInfo(
                sync::PipelineStage::Graphics,
                sync::PipelineStage::Graphics,
                sync::TransitionCreateFlags::NoSplit,
                gp::ConstVectorView<sync::TransitionInfo>(&aToUAV, 1u),
                gp::ConstVectorView<sync::TransientAliasingInfo>(&acqA, 1u)
            )
        );
        cmdList.beginTransition(tr1);
        cmdList.endTransition(tr1);

        const sync::TransientAliasingInfo discA = sync::TransientAliasingInfo::discard(bufA.get());
        const sync::TransitionInfo aToDiscard(
            bufA.get(), sync::ResourceAccess::UAVCompute, sync::ResourceAccess::Discard
        );
        const sync::Transition* const tr2 = createTransition(
            sync::TransitionCreateInfo(
                sync::PipelineStage::Graphics,
                sync::PipelineStage::Graphics,
                sync::TransitionCreateFlags::NoSplit,
                gp::ConstVectorView<sync::TransitionInfo>(&aToDiscard, 1u),
                gp::ConstVectorView<sync::TransientAliasingInfo>(&discA, 1u)
            )
        );
        cmdList.beginTransition(tr2);
        cmdList.endTransition(tr2);

        const sync::TransientAliasingOverlap overlapA(bufA.get());
        const sync::TransientAliasingInfo acqB = sync::TransientAliasingInfo::acquire(
            bufB.get(), gp::ConstVectorView<sync::TransientAliasingOverlap>(&overlapA, 1u)
        );
        const sync::TransitionInfo bToUAV(bufB.get(), sync::ResourceAccess::Discard, sync::ResourceAccess::UAVCompute);
        const sync::Transition* const tr3 = createTransition(
            sync::TransitionCreateInfo(
                sync::PipelineStage::Graphics,
                sync::PipelineStage::Graphics,
                sync::TransitionCreateFlags::NoSplit,
                gp::ConstVectorView<sync::TransitionInfo>(&bToUAV, 1u),
                gp::ConstVectorView<sync::TransientAliasingInfo>(&acqB, 1u)
            )
        );
        cmdList.beginTransition(tr3);
        cmdList.endTransition(tr3);
        cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    }

    EXPECT_EQ(getValidationFailureCount(), 0u);
}

TEST_F(RHIValidationFixture, DetectsPSODepthWriteMismatchUniformBufferLifetimeAndIndirectArgsErrors)
{
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    // 1. Read-only DSV in RenderPassInfo vs DepthWrite PSO -> must flag DSV incompatibility!
    resource::TextureCreateDesc dsDesc =
        resource::TextureCreateDesc::create2D("SceneDepth", 64, 64, PixelFormat::D32_Float_S8_UInt);
    dsDesc.flags = resource::TextureCreateFlags::DepthStencilTargetable;
    dsDesc.initialState = sync::ResourceAccess::DSVRead;
    resource::TextureRef depthTex = cmdList.createTexture(dsDesc);

    const gp::UInt8 dummyCode[4] = { 1u, 2u, 3u, 4u };
    shader::VertexShaderRef vs = getDynamicRHI()->createVertexShader(dummyCode, shader::ShaderHash{});
    pipeline::DepthStencilStateRef dsWriteState = getDynamicRHI()->createDepthStencilState(
        pipeline::DepthStencilStateDesc(true, pipeline::CompareFunction::Less)
    );

    pipeline::GraphicsPipelineStateInitializer psoInit{};
    psoInit.boundShaderState.setVertexShader(vs.get());
    psoInit.depthStencilState = dsWriteState.get();
    psoInit.depthStencilTargetFormat = PixelFormat::D32_Float_S8_UInt;
    psoInit.depthStencilTargetFlag = dsDesc.flags;
    psoInit.depthStencilAccess = resource::ExclusiveDepthStencil::kDepthWriteStencilNop;
    pipeline::GraphicsPipelineStateRef writePSO = getDynamicRHI()->createGraphicsPipelineState(psoInit);
    EXPECT_EQ(getValidationFailureCount(), 0u);

    const command::RenderPassInfo readOnlyDepthPass(
        depthTex.get(),
        command::DepthStencilTargetActions::LoadDepthNotStencilDontStore,
        resource::ExclusiveDepthStencil::kDepthReadStencilNop
    );
    cmdList.beginRenderPass(readOnlyDepthPass, "ReadOnlyDepthPass");
    cmdList.setGraphicsPipelineState(writePSO.get());
    cmdList.endRenderPass();
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_GE(getValidationFailureCount(), 1u);
    clearValidationFailures();

    // 2. SingleFrame UniformBuffer bound after advancing `endFrame()` -> must flag stale frame lifetime!
    resource::UniformBufferLayoutDesc ubLayoutDesc("ViewUB");
    ubLayoutDesc.constantBufferSize = 16u;
    resource::UniformBufferLayoutRef ubLayout = createUniformBufferLayout(ubLayoutDesc);
    const gp::UInt32 ubInitData[4] = { 1u, 2u, 3u, 4u };
    resource::UniformBufferRef singleFrameUB =
        getDynamicRHI()->createUniformBuffer(ubInitData, ubLayout.get(), resource::UniformBufferUsage::SingleFrame);

    shader::ComputeShaderRef cs = getDynamicRHI()->createComputeShader(dummyCode, shader::ShaderHash{});
    shader::ShaderResourceTable srt{};
    srt.resourceTableLayoutHashes.emplaceBack(ubLayout->getHash());
    cs->setShaderResourceTable(std::move(srt));
    pipeline::ComputePipelineStateRef csPSO = getDynamicRHI()->createComputePipelineState(cs.get());

    // Advance frame!
    cmdList.endFrame();

    cmdList.setComputePipelineState(csPSO.get());
    shader::BatchedShaderParameters& params = cmdList.getScratchShaderParameters();
    params.setShaderUniformBuffer(0u, singleFrameUB.get());
    cmdList.setBatchedShaderParameters(cs.get(), params);
    cmdList.dispatchComputeShader(1u, 1u, 1u);
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);

    EXPECT_GE(getValidationFailureCount(), 1u);
    EXPECT_NE(getLastValidationFailureMessage().view().find("SingleFrame"), gp::StringView::npos);
    clearValidationFailures();

    // 3. Indirect dispatch with a buffer lacking `DrawIndirect` flag and unaligned offset -> must flag!
    resource::BufferCreateDesc nonIndirectDesc =
        resource::BufferCreateDesc::createByteAddress("NonIndirectBuf", 64u, 4u);
    nonIndirectDesc.initialState = sync::ResourceAccess::IndirectArgs;
    resource::BufferRef nonIndirectBuf = cmdList.createBuffer(nonIndirectDesc);

    cmdList.setComputePipelineState(csPSO.get());
    cmdList.dispatchIndirectComputeShader(nonIndirectBuf.get(), 2u);
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_GE(getValidationFailureCount(), 2u);
}

#endif

}   // namespace gp::rhi::validation::tests
