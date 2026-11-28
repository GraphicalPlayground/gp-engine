// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/DynamicRHI.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/ICommandContext.hpp"
#include "containers/arrays/Vector.hpp"
#include "device/DynamicRHIPSOFallback.hpp"
#include "device/GenericStagingBuffer.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/Memory.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/SamplerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "presentation/PresentArgs.hpp"
#include "presentation/Viewport.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/ResourceReplace.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/TextureReference.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"   // IWYU pragma: keep
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/VertexShader.hpp"
#include "sync/GPUFence.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/RenderQueryPool.hpp"
#include "sync/Transition.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::tests
{

namespace
{

class DeviceTestBuffer final : public resource::Buffer
{
public:
    explicit DeviceTestBuffer(const resource::BufferCreateDesc& desc) noexcept
        : Buffer(desc)
    {}

protected:
    virtual ~DeviceTestBuffer() noexcept override = default;
};

class DeviceTestTexture final : public resource::Texture
{
public:
    explicit DeviceTestTexture(const resource::TextureCreateDesc& desc) noexcept
        : Texture(desc)
    {}

protected:
    virtual ~DeviceTestTexture() noexcept override = default;
};

class DeviceTestUniformBuffer final : public resource::UniformBuffer
{
public:
    explicit DeviceTestUniformBuffer(const resource::UniformBufferLayout* const layout) noexcept
        : UniformBuffer(layout)
    {}

protected:
    virtual ~DeviceTestUniformBuffer() noexcept override = default;
};

class DeviceTestFence final : public sync::GPUFence
{
public:
    explicit DeviceTestFence(const gp::Name name) noexcept
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
    virtual ~DeviceTestFence() noexcept override = default;
};

struct BackendTransitionPrivateData
{
    gp::UInt64 magic{ 0u };
};

class DeviceMockRHI final
    : public DynamicRHIPSOFallback
    , public command::ICommandContextPSOFallback
{
public:
    bool initCalled{ false };
    bool postInitCalled{ false };
    bool shutdownCalled{ false };
    gp::UInt32 lockBottomCalls{ 0u };
    gp::UInt32 unlockBottomCalls{ 0u };
    gp::UInt32 updateTex3DCalls{ 0u };
    gp::UInt32 createTransitionCalls{ 0u };
    gp::UInt32 releaseTransitionCalls{ 0u };
    gp::UInt32 endViewportCalls{ 0u };
    gp::UInt32 replaceResourcesCalls{ 0u };
    gp::USize lastReplacedCount{ 0u };
    presentation::PresentArgsWithStats lastPresentArgs{};
    gp::Vector<gp::UInt8> backingBuffer{};

    virtual void init() noexcept override
    {
        initCalled = true;
        getRHIGlobals().gpuInfo.adapterName = "DeviceMock";
    }

    virtual void postInit() noexcept override
    {
        postInitCalled = true;
    }

    virtual void shutdown() noexcept override
    {
        shutdownCalled = true;
    }

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "DeviceMock";
    }

    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override
    {
        return InterfaceType::Null;
    }

    [[nodiscard]] virtual gp::USize getTransitionPrivateDataSizeInBytes() const noexcept override
    {
        return sizeof(BackendTransitionPrivateData);
    }

    [[nodiscard]] virtual gp::USize getTransitionPrivateDataAlignInBytes() const noexcept override
    {
        return alignof(BackendTransitionPrivateData);
    }

    virtual void
        createTransition(sync::Transition* const transition, const sync::TransitionCreateInfo&) noexcept override
    {
        ++createTransitionCalls;
        if (transition != nullptr)
        {
            BackendTransitionPrivateData* const priv = transition->getPrivateData<BackendTransitionPrivateData>();
            if (priv != nullptr)
            {
                priv->magic = 0xDEADBEEFCAFEBABEull;
            }
        }
    }

    virtual void releaseTransition(sync::Transition* const transition) noexcept override
    {
        ++releaseTransitionCalls;
        if (transition != nullptr)
        {
            BackendTransitionPrivateData* const priv = transition->getPrivateData<BackendTransitionPrivateData>();
            if (priv != nullptr)
            {
                priv->magic = 0u;
            }
        }
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
        return resource::UniformBufferRef(new DeviceTestUniformBuffer(layout));
    }

    virtual void updateUniformBuffer(command::CommandListBase&, resource::UniformBuffer*, const void*) noexcept override
    {}

    [[nodiscard]] virtual resource::BufferInitializer createBufferInitializer(
        command::CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
    ) noexcept override
    {
        backingBuffer.resize(createDesc.size, 0u);
        resource::BufferRef buf(new DeviceTestBuffer(createDesc));
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
        ++lockBottomCalls;
        const gp::USize needed = static_cast<gp::USize>(offset) + static_cast<gp::USize>(size);
        if (backingBuffer.size() < needed)
        {
            backingBuffer.resize(needed, 0u);
        }
        return backingBuffer.data() + offset;
    }

    virtual void unlockBufferBottomOfPipe(command::CommandListBase&, resource::Buffer*) noexcept override
    {
        ++unlockBottomCalls;
    }

    [[nodiscard]] virtual resource::TextureInitializer createTextureInitializer(
        command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
    ) noexcept override
    {
        resource::TextureRef tex(new DeviceTestTexture(createDesc));
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
    {
        ++updateTex3DCalls;
    }

    using DynamicRHI::readSurfaceData;

    virtual void readSurfaceData(
        resource::Texture*,
        const resource::IntRect& rect,
        gp::Vector<gp::math::Color>& outData,
        const resource::ReadSurfaceDataFlags&
    ) noexcept override
    {
        const gp::USize count = static_cast<gp::USize>(rect.width()) * static_cast<gp::USize>(rect.height());
        outData.resize(count, gp::math::Color(255u, 128u, 0u, 255u));
    }

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
        return sync::GPUFenceRef(new DeviceTestFence(name));
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
        return resource::TextureRef(new DeviceTestTexture(
            resource::TextureCreateDesc::create2D("BB").setExtent(1, 1).setFormat(PixelFormat::B8G8R8A8_UNorm)
        ));
    }

    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport*) noexcept override
    {}

    virtual void beginDrawingViewport(presentation::Viewport*, resource::Texture*) noexcept override
    {}

    using DynamicRHI::endDrawingViewport;

    virtual void endDrawingViewport(
        presentation::Viewport*, const presentation::PresentArgsWithStats& presentArgs
    ) noexcept override
    {
        ++endViewportCalls;
        lastPresentArgs = presentArgs;
    }

    virtual void replaceResources(
        command::CommandListBase&, const gp::ConstVectorView<resource::ResourceReplaceInfo> replaceInfos
    ) noexcept override
    {
        ++replaceResourcesCalls;
        lastReplacedCount = replaceInfos.size();
    }

    virtual void blockUntilGPUIdle() noexcept override
    {}

    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override
    {
        return this;
    }

    // Minimal no-op context overrides
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

    virtual void setShaderParameters(
        shader::GraphicsShader*,
        gp::ConstVectorView<gp::UInt8>,
        gp::ConstVectorView<shader::ShaderParameter>,
        gp::ConstVectorView<shader::ShaderParameterResource>,
        gp::ConstVectorView<shader::ShaderParameterResource>
    ) noexcept override
    {}

    virtual void
        setShaderUnbinds(shader::GraphicsShader*, gp::ConstVectorView<shader::ShaderParameterUnbind>) noexcept override
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

TEST(DynamicRHITest, LifecycleInitPostInitExitAndGlobalsReset)
{
    EXPECT_FALSE(hasValidDynamicRHI());
    EXPECT_EQ(getInterfaceType(), InterfaceType::NotSet);

    DeviceMockRHI* const mock = new DeviceMockRHI();
    rhiInit(mock);
    EXPECT_TRUE(hasValidDynamicRHI());
    EXPECT_EQ(getDynamicRHI(), mock);
    EXPECT_EQ(castDynamicRHI<DeviceMockRHI>(), mock);
    EXPECT_EQ(getInterfaceType(), InterfaceType::Null);
    EXPECT_TRUE(mock->initCalled);
    EXPECT_TRUE(getRHIGlobals().isRHIInitialized);
    EXPECT_EQ(getRHIGlobals().gpuInfo.adapterName, gp::String("DeviceMock"));

    rhiPostInit();
    EXPECT_TRUE(mock->postInitCalled);

    // Transition private data hooks wired by rhiInit() must be active.
    const sync::TransitionCreateInfo createInfo(
        sync::PipelineStage::Graphics, sync::PipelineStage::Graphics, sync::TransitionCreateFlags::None
    );
    sync::Transition* const transition = createTransition(createInfo);
    ASSERT_NE(transition, nullptr);
    EXPECT_EQ(mock->createTransitionCalls, 1u);
    const BackendTransitionPrivateData* const priv = transition->getPrivateData<BackendTransitionPrivateData>();
    ASSERT_NE(priv, nullptr);
    EXPECT_EQ(priv->magic, 0xDEADBEEFCAFEBABEull);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
    EXPECT_EQ(mock->releaseTransitionCalls, 1u);

    rhiExit();
    EXPECT_FALSE(hasValidDynamicRHI());
    EXPECT_EQ(getInterfaceType(), InterfaceType::NotSet);
    EXPECT_FALSE(getRHIGlobals().isRHIInitialized);
    EXPECT_TRUE(getRHIGlobals().gpuInfo.adapterName.isEmpty());
}

TEST(DynamicRHITest, LockBufferWriteStagingInQueuedModeAndDirectInBypassMode)
{
    DeviceMockRHI* const mock = new DeviceMockRHI();
    rhiInit(mock);

    command::CommandListImmediate& cmdList = command::getImmediateCommandList();
    const resource::BufferCreateDesc bufDesc =
        resource::BufferCreateDesc::create("LockBuf", 16u, 4u, resource::BufferUsageFlags::Dynamic);
    resource::BufferRef buf = cmdList.createBuffer(bufDesc);

    // Queued WriteOnly lock stages into CPU memory and commits on flush.
    command::getCommandListExecutor().setBypass(false);
    void* const stagedPtr = cmdList.lockBuffer(buf.get(), 4u, 8u, resource::ResourceLockMode::WriteOnly);
    ASSERT_NE(stagedPtr, nullptr);
    EXPECT_EQ(mock->lockBottomCalls, 0u);

    const gp::UInt32 payload[2] = { 0x12345678u, 0x9ABCDEF0u };
    gp::memory::copyMemory(stagedPtr, payload, sizeof(payload));
    cmdList.unlockBuffer(buf.get());
    EXPECT_EQ(mock->lockBottomCalls, 0u);

    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_EQ(mock->lockBottomCalls, 1u);
    EXPECT_EQ(mock->unlockBottomCalls, 1u);

    gp::UInt32 verified[2] = { 0u, 0u };
    gp::memory::copyMemory(verified, mock->backingBuffer.data() + 4u, sizeof(verified));
    EXPECT_EQ(verified[0], payload[0]);
    EXPECT_EQ(verified[1], payload[1]);

    // Bypass mode locks bottom-of-pipe immediately.
    command::getCommandListExecutor().setBypass(true);
    void* const directPtr = cmdList.lockBuffer(buf.get(), 0u, 4u, resource::ResourceLockMode::ReadOnly);
    EXPECT_EQ(directPtr, mock->backingBuffer.data());
    EXPECT_EQ(mock->lockBottomCalls, 2u);
    cmdList.unlockBuffer(buf.get());
    EXPECT_EQ(mock->unlockBottomCalls, 2u);

    command::getCommandListExecutor().setBypass(false);
    rhiExit();
}

TEST(DynamicRHITest, DefaultHelpersQueryPoolStagingTextureRefAndPrecacheHash)
{
    DeviceMockRHI* const mock = new DeviceMockRHI();
    rhiInit(mock);
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    // DefaultRenderQueryPool allocates and recycles queries.
    sync::RenderQueryPoolRef pool = createRenderQueryPool(sync::RenderQueryType::Occlusion, 2u);
    ASSERT_TRUE(pool.isValid());
    sync::RenderQuery* firstRaw = nullptr;
    {
        sync::PooledRenderQuery q1 = pool->allocateQuery();
        ASSERT_TRUE(q1.isValid());
        firstRaw = q1.getQuery();
    }
    {
        sync::PooledRenderQuery q2 = pool->allocateQuery();
        ASSERT_TRUE(q2.isValid());
        EXPECT_EQ(q2.getQuery(), firstRaw);
    }

    // GenericStagingBuffer writeShadowData / lock / unlock / getGPUSizeBytes.
    resource::StagingBufferRef staging = mock->createStagingBuffer();
    ASSERT_TRUE(staging.isValid());
    const gp::UInt32 data[2] = { 101u, 202u };
    static_cast<GenericStagingBuffer*>(staging.get())->writeShadowData(data, sizeof(data));
    EXPECT_EQ(staging->getGPUSizeBytes(), sizeof(data));
    const void* const mapped = mock->lockStagingBuffer(staging.get(), nullptr, 4u, 4u);
    ASSERT_NE(mapped, nullptr);
    gp::UInt32 secondWord = 0u;
    gp::memory::copyMemory(&secondWord, mapped, sizeof(secondWord));
    EXPECT_EQ(secondWord, 202u);
    mock->unlockStagingBuffer(staging.get());

    // TextureReference creation and update.
    const resource::TextureCreateDesc texDesc =
        resource::TextureCreateDesc::create2D("RefTarget").setExtent(8, 8).setFormat(PixelFormat::R8G8B8A8_UNorm);
    resource::TextureRef texA = cmdList.createTexture(texDesc);
    resource::TextureRef texB = cmdList.createTexture(texDesc);
    resource::TextureReferenceRef texRef = cmdList.createTextureReference(texA.get());
    ASSERT_TRUE(texRef.isValid());
    EXPECT_EQ(texRef->getReferencedTexture(), texA.get());

    cmdList.updateTextureReference(texRef.get(), texB.get());
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_EQ(texRef->getReferencedTexture(), texB.get());

    // Begin/End UpdateTexture3D default staging path.
    const resource::TextureCreateDesc volDesc =
        resource::TextureCreateDesc::create3D("VolTex").setExtent(4, 4).setDepth(2).setFormat(
            PixelFormat::R8G8B8A8_UNorm
        );
    resource::TextureRef volTex = cmdList.createTexture(volDesc);
    const resource::UpdateTextureRegion3D region(0u, 0u, 0u, 0u, 0u, 0u, 4u, 4u, 2u);
    resource::UpdateTexture3DData updateData = cmdList.beginUpdateTexture3D(volTex.get(), 0u, region);
    ASSERT_NE(updateData.data, nullptr);
    EXPECT_EQ(updateData.rowPitch, 16u);
    EXPECT_EQ(updateData.depthPitch, 64u);
    cmdList.endUpdateTexture3D(updateData);
    EXPECT_EQ(updateData.data, nullptr);
    EXPECT_EQ(mock->updateTex3DCalls, 1u);

    // LinearColor readSurfaceData fallback converts Color output.
    gp::Vector<gp::math::LinearColor> linearPixels{};
    cmdList.readSurfaceData(texA.get(), resource::IntRect(0, 0, 2, 2), linearPixels);
    ASSERT_EQ(linearPixels.size(), 4u);
    EXPECT_NEAR(linearPixels[0].r, 1.0f, 0.01f);
    EXPECT_NEAR(linearPixels[0].b, 0.0f, 0.01f);
    EXPECT_NEAR(linearPixels[0].a, 1.0f, 0.01f);

    // calcTexturePlatformSize.
    const resource::CalcTextureSizeResult sizeResult =
        mock->calcTexturePlatformSize(static_cast<const resource::TextureDesc&>(texDesc), 0u);
    EXPECT_EQ(sizeResult.size, 8u * 8u * 4u);
    EXPECT_GT(sizeResult.align, 0u);

    // PSO precache hashing & matching.
    pipeline::VertexDeclarationRef vdecl = mock->createVertexDeclaration({});
    shader::VertexShaderRef vs = mock->createVertexShader({}, shader::ShaderHash{ 1u, 2u });
    shader::PixelShaderRef ps = mock->createPixelShader({}, shader::ShaderHash{ 3u, 4u });
    pipeline::GraphicsPipelineStateInitializer psoA{};
    psoA.boundShaderState = pipeline::BoundShaderStateInput(vdecl.get(), vs.get(), ps.get());
    psoA.renderTargetsEnabled = 1u;
    psoA.renderTargetFormats[0] = PixelFormat::R8G8B8A8_UNorm;
    pipeline::GraphicsPipelineStateInitializer psoB = psoA;

    EXPECT_EQ(mock->computeStatePrecachePSOHash(psoA), mock->computeStatePrecachePSOHash(psoB));
    EXPECT_EQ(mock->computePrecachePSOHash(psoA), mock->computePrecachePSOHash(psoB));
    EXPECT_TRUE(mock->matchPrecachePSOInitializers(psoA, psoB));

    psoB.primitiveType = pipeline::PrimitiveType::LineList;
    EXPECT_NE(mock->computeStatePrecachePSOHash(psoA), mock->computeStatePrecachePSOHash(psoB));
    EXPECT_FALSE(mock->matchPrecachePSOInitializers(psoA, psoB));

    rhiExit();
}

TEST(DynamicRHITest, PresentArgsWithStatsAndResourceReplaceBatcher)
{
    DeviceMockRHI* const mock = new DeviceMockRHI();
    rhiInit(mock);
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    presentation::ViewportRef vp = mock->createViewport(nullptr, 128u, 128u, false, PixelFormat::B8G8R8A8_UNorm);
    const gp::UInt64 initialPresentCounter = getRHIGlobals().presentCounter;

    // Explicit PresentArgsWithStats propagates frameCounter and inputCycles and increments presentCounter on present.
    cmdList.beginDrawingViewport(vp.get(), nullptr);
    const presentation::PresentArgsWithStats explicitArgs(presentation::PresentArgs(true, true), 42u, 987654321ull);
    cmdList.endDrawingViewport(vp.get(), explicitArgs);
    EXPECT_EQ(mock->endViewportCalls, 1u);
    EXPECT_EQ(mock->lastPresentArgs, explicitArgs);
    EXPECT_EQ(getRHIGlobals().presentCounter, initialPresentCounter + 1u);

    // Skipping presentation (shouldPresent() == false) must not increment presentCounter.
    cmdList.beginDrawingViewport(vp.get(), nullptr);
    const presentation::PresentArgsWithStats noPresentArgs(presentation::PresentArgs(false, false), 43u, 12345ull);
    cmdList.endDrawingViewport(vp.get(), noPresentArgs);
    EXPECT_EQ(mock->endViewportCalls, 2u);
    EXPECT_EQ(mock->lastPresentArgs, noPresentArgs);
    EXPECT_EQ(getRHIGlobals().presentCounter, initialPresentCounter + 1u);

    // Advance backend frame counter and verify legacy (present, lockToVsync) overload populates m_frameCounter.
    cmdList.beginFrame();
    cmdList.endFrame();
    EXPECT_EQ(mock->getFrameCounter(), 1u);
    cmdList.beginDrawingViewport(vp.get(), nullptr);
    cmdList.endDrawingViewport(vp.get(), true, false);
    EXPECT_EQ(mock->endViewportCalls, 3u);
    EXPECT_TRUE(mock->lastPresentArgs.shouldPresent());
    EXPECT_FALSE(mock->lastPresentArgs.isLockedToVsync());
    EXPECT_EQ(mock->lastPresentArgs.frameCounter, 1u);
    EXPECT_EQ(mock->lastPresentArgs.inputCycles, 0u);
    EXPECT_EQ(getRHIGlobals().presentCounter, initialPresentCounter + 2u);

    // ResourceReplaceInfo typed pair contracts and ResourceReplaceBatcher flush semantics.
    const resource::BufferCreateDesc bufDesc =
        resource::BufferCreateDesc::create("ReplaceBuf", 32u, 4u, resource::BufferUsageFlags::Static);
    resource::BufferRef dstA = cmdList.createBuffer(bufDesc);
    resource::BufferRef srcA = cmdList.createBuffer(bufDesc);
    resource::BufferRef dstB = cmdList.createBuffer(bufDesc);

    const resource::ResourceReplaceInfo bufInfo(dstA.get(), srcA.get());
    EXPECT_EQ(bufInfo.getType(), resource::ResourceReplaceInfo::Type::Buffer);
    EXPECT_EQ(bufInfo.getBuffer().dst, dstA.get());
    EXPECT_EQ(bufInfo.getBuffer().src, srcA.get());

    resource::RayTracingGeometry* const dummyGeom = nullptr;
    const resource::ResourceReplaceInfo rtInfo(dummyGeom, dummyGeom);
    EXPECT_EQ(rtInfo.getType(), resource::ResourceReplaceInfo::Type::RTGeometry);
    EXPECT_EQ(rtInfo.getRTGeometry().dst, nullptr);
    EXPECT_NE(bufInfo, rtInfo);

    // Empty batcher must not dispatch replaceResources on destruction or manual flush.
    {
        resource::ResourceReplaceBatcher emptyBatcher(cmdList, 4u);
        EXPECT_TRUE(emptyBatcher.isEmpty());
        emptyBatcher.flush();
    }
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_EQ(mock->replaceResourcesCalls, 0u);

    // Queued entries flush automatically on scope exit in a single batch.
    {
        resource::ResourceReplaceBatcher batcher(cmdList, 2u);
        batcher.enqueueReplace(dstA.get(), srcA.get());
        batcher.enqueueReplace(dstB.get(), static_cast<resource::Buffer*>(nullptr));
        EXPECT_EQ(batcher.size(), 2u);
        EXPECT_FALSE(batcher.isEmpty());
    }
    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    EXPECT_EQ(mock->replaceResourcesCalls, 1u);
    EXPECT_EQ(mock->lastReplacedCount, 2u);

    rhiExit();
}

}   // namespace

}   // namespace gp::rhi::tests
