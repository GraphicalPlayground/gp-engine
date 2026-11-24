// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/NullDynamicRHI.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "device/DynamicRHI.hpp"
#include "device/NullDynamicRHIModule.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/color/Color.hpp"
#include "maths/vector/Vector4.hpp"   // IWYU pragma: keep
#include "memory/Memory.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "shaders/ShaderParameters.hpp"   // IWYU pragma: keep
#include "sync/RenderQueryPool.hpp"
#include "threading/roles/ScopedThreadRole.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "views/ViewDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::null::tests
{

namespace
{

class TrackingResourceArray final : public resource::IResourceArrayUpload
{
public:
    gp::UInt32 data[4] = { 1u, 2u, 3u, 4u };
    bool discarded{ false };

    [[nodiscard]] virtual const void* getResourceData() const noexcept override
    {
        return data;
    }

    [[nodiscard]] virtual gp::UInt32 getResourceDataSize() const noexcept override
    {
        return static_cast<gp::UInt32>(sizeof(data));
    }

    virtual void discard() noexcept override
    {
        discarded = true;
    }
};

class TrackingBulkData final : public resource::IResourceBulkData
{
public:
    gp::UInt8 bytes[64]{};
    bool discarded{ false };

    [[nodiscard]] virtual const void* getResourceBulkData() const noexcept override
    {
        return bytes;
    }

    [[nodiscard]] virtual gp::UInt32 getResourceBulkDataSize() const noexcept override
    {
        return static_cast<gp::UInt32>(sizeof(bytes));
    }

    virtual void discard() noexcept override
    {
        discarded = true;
    }
};

class NullDynamicRHIFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        NullDynamicRHIModule module{};
        ASSERT_TRUE(module.isSupported());
        ASSERT_TRUE(module.isSupported(FeatureLevel::SM5));
        ASSERT_FALSE(module.supportsDynamicReloading());

        DynamicRHI* const rhi = module.createRHI(FeatureLevel::SM5);
        ASSERT_NE(rhi, nullptr);
        rhiInit(rhi);
        rhiPostInit();
    }

    void TearDown() override
    {
        command::getCommandListExecutor().setBypass(false);
        rhiExit();
    }
};

TEST_F(NullDynamicRHIFixture, IdentityCapabilitiesAndContextContract)
{
    DynamicRHI* const rhi = getDynamicRHI();
    ASSERT_NE(rhi, nullptr);
    EXPECT_STREQ(rhi->getName(), "Null");
    EXPECT_EQ(rhi->getInterfaceType(), InterfaceType::Null);
    EXPECT_EQ(getInterfaceType(), InterfaceType::Null);

    NullDynamicRHI* const nullRHI = castDynamicRHI<NullDynamicRHI>();
    ASSERT_NE(nullRHI, nullptr);
    EXPECT_EQ(rhi->getDefaultContext(), static_cast<command::ICommandContext*>(nullRHI));
    EXPECT_EQ(rhi->getDefaultAsyncComputeContext(), static_cast<command::IComputeContext*>(nullRHI));
    EXPECT_EQ(rhi->getCommandContext(sync::PipelineStage::Graphics), nullptr);
    EXPECT_EQ(rhi->acquireUploadContext(), nullptr);

    const RHIGlobals& globals = getRHIGlobals();
    EXPECT_TRUE(globals.isRHIInitialized);
    EXPECT_TRUE(globals.usingNullRHI);
    EXPECT_EQ(globals.maxTextureDimensions, 16384u);
    EXPECT_EQ(globals.maxVolumeTextureDimensions, 2048u);
    EXPECT_EQ(globals.maxCubeTextureDimensions, 16384u);
    EXPECT_EQ(globals.maxTextureArrayLayers, 2048u);
    EXPECT_EQ(globals.gpuInfo.adapterName, gp::String("Null"));
    EXPECT_EQ(globals.gpuInfo.vendorId, GpuVendorId::NotQueried);
    EXPECT_TRUE(hasPixelFormatCapabilities(
        globals.getPixelFormatCapabilities(PixelFormat::R8G8B8A8_UNorm),
        PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::RenderTarget | PixelFormatCapabilities::UAV
    ));
    EXPECT_TRUE(globals.vertexElementTypeSupport.isSupported(pipeline::VertexElementType::Float3));
}

TEST_F(NullDynamicRHIFixture, BufferAndTextureInitializersLocksAndReadbacks)
{
    DynamicRHI* const rhi = getDynamicRHI();
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    // BufferInitializer in Initializer mode provides writable scratch memory.
    const resource::BufferCreateDesc initBufDesc =
        resource::BufferCreateDesc::create("NullInitBuf", 64u, 4u, resource::BufferUsageFlags::VertexBuffer)
            .setInitActionInitializer();
    resource::BufferInitializer bufInit = cmdList.createBufferInitializer(initBufDesc);
    ASSERT_TRUE(bufInit.isValid());
    ASSERT_NE(bufInit.getWritableData(), nullptr);
    EXPECT_EQ(bufInit.getWritableDataSize(), 64u);
    const gp::UInt32 word = 0x55AA55AAu;
    bufInit.writeBytes(0u, &word, sizeof(word));
    resource::BufferRef bufA = bufInit.finalize();
    ASSERT_TRUE(bufA.isValid());
    EXPECT_EQ(bufA->getSize(), 64u);

    // BufferInitializer in ResourceArray mode calls discard() on upload array.
    TrackingResourceArray uploadArray{};
    const resource::BufferCreateDesc arrayBufDesc =
        resource::BufferCreateDesc::create("NullArrayBuf", 16u, 4u, resource::BufferUsageFlags::Static)
            .setInitActionResourceArray(&uploadArray);
    resource::BufferRef bufB = cmdList.createBuffer(arrayBufDesc);
    ASSERT_TRUE(bufB.isValid());
    EXPECT_TRUE(uploadArray.discarded);

    // Lock and unlock buffer returns valid memory.
    void* const lockedBuf = cmdList.lockBuffer(bufA.get(), 0u, 32u, resource::ResourceLockMode::WriteOnly);
    ASSERT_NE(lockedBuf, nullptr);
    gp::memory::zeroMemory(lockedBuf, 32u);
    cmdList.unlockBuffer(bufA.get());

    // TextureInitializer in Initializer mode supplies subresource pointers and pitches.
    const resource::TextureCreateDesc initTexDesc = resource::TextureCreateDesc::create2D("NullInitTex")
                                                        .setExtent(16, 16)
                                                        .setNumMips(3u)
                                                        .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                                        .setInitActionInitializer();
    resource::TextureInitializer texInit = cmdList.createTextureInitializer(initTexDesc);
    ASSERT_TRUE(texInit.isValid());
    const resource::TextureSubresourceInitializer mip0 = texInit.getWritableSubresource(0u, 0u);
    const resource::TextureSubresourceInitializer mip1 = texInit.getWritableSubresource(1u, 0u);
    ASSERT_TRUE(mip0.isValid());
    ASSERT_TRUE(mip1.isValid());
    EXPECT_EQ(mip0.stride, 16u * 4u);
    EXPECT_EQ(mip0.size, 16u * 16u * 4u);
    EXPECT_EQ(mip1.stride, 8u * 4u);
    EXPECT_EQ(mip1.size, 8u * 8u * 4u);
    resource::TextureRef texA = texInit.finalize();
    ASSERT_TRUE(texA.isValid());

    // Null backend reports zero GPU footprint for textures.
    const resource::CalcTextureSizeResult nullFootprint =
        rhi->calcTexturePlatformSize(static_cast<const resource::TextureDesc&>(initTexDesc), 0u);
    EXPECT_EQ(nullFootprint.size, 0u);
    EXPECT_EQ(nullFootprint.align, 0u);
    EXPECT_EQ(rhi->computeMemorySize(texA.get()), 0u);

    // TextureInitializer in BulkData mode calls discard().
    TrackingBulkData bulkData{};
    const resource::TextureCreateDesc bulkTexDesc = resource::TextureCreateDesc::create2D("NullBulkTex")
                                                        .setExtent(4, 4)
                                                        .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                                        .setInitActionBulkData(&bulkData);
    resource::TextureRef texB = cmdList.createTexture(bulkTexDesc);
    ASSERT_TRUE(texB.isValid());
    EXPECT_TRUE(bulkData.discarded);

    // lockTexture computes mip-scaled stride and size.
    const resource::TextureLockArgs lockMip1 =
        resource::TextureLockArgs::lock2D(texA.get(), 1u, resource::ResourceLockMode::WriteOnly);
    const resource::TextureLockResult lockRes = cmdList.lockTexture(lockMip1);
    ASSERT_TRUE(lockRes.isValid());
    EXPECT_EQ(lockRes.stride, 8u * 4u);
    EXPECT_EQ(lockRes.byteCount, 8u * 8u * 4u);
    cmdList.unlockTexture(lockMip1);

    // readSurfaceData appends width * height zeroed pixels.
    gp::Vector<gp::math::Color> pixels{};
    cmdList.readSurfaceData(texA.get(), resource::IntRect(0, 0, 4, 3), pixels);
    ASSERT_EQ(pixels.size(), 12u);
    EXPECT_EQ(pixels[0], gp::math::Color(0u, 0u, 0u, 0u));
    EXPECT_EQ(pixels[11], gp::math::Color(0u, 0u, 0u, 0u));
}

TEST_F(NullDynamicRHIFixture, FullEndToEndRenderAndComputeContract)
{
    gp::thread::ScopedNamedThread renderRole(gp::thread::NamedThread::Render);
    ASSERT_TRUE(renderRole.isAttached());

    DynamicRHI* const rhi = getDynamicRHI();
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    for (gp::UInt32 pass = 0u; pass < 2u; ++pass)
    {
        const bool bypassMode = (pass == 1u);
        command::getCommandListExecutor().setBypass(bypassMode);

        pipeline::SamplerStateRef sampler = rhi->createSamplerState({});
        pipeline::RasterizerStateRef raster = rhi->createRasterizerState({});
        pipeline::DepthStencilStateRef depth = rhi->createDepthStencilState({});
        pipeline::BlendStateRef blend = rhi->createBlendState({});
        pipeline::VertexDeclarationRef vdecl = rhi->createVertexDeclaration({});

        const shader::ShaderHash vsHash{ 10u, 20u };
        shader::VertexShaderRef vs = rhi->createVertexShader({}, vsHash);
        shader::PixelShaderRef ps = rhi->createPixelShader({}, {});
        shader::GeometryShaderRef gs = rhi->createGeometryShader({}, {});
        shader::ComputeShaderRef cs = rhi->createComputeShader({}, {});
        shader::MeshShaderRef ms = rhi->createMeshShader({}, {});
        shader::AmplificationShaderRef as = rhi->createAmplificationShader({}, {});
        ASSERT_TRUE(sampler.isValid() && raster.isValid() && depth.isValid() && blend.isValid() && vdecl.isValid());
        ASSERT_TRUE(vs.isValid() && ps.isValid() && gs.isValid() && cs.isValid() && ms.isValid() && as.isValid());
        EXPECT_EQ(vs->getHash(), vsHash);

        pipeline::ComputePipelineStateRef computePSO = rhi->createComputePipelineState(cs.get());
        ASSERT_TRUE(computePSO.isValid());

        resource::UniformBufferLayoutDesc ubDesc("NullFrameUB");
        ubDesc.constantBufferSize = 32u;
        ubDesc.staticSlot = 0u;
        resource::UniformBufferLayoutRef ubLayout = createUniformBufferLayout(ubDesc);
        resource::UniformBufferRef ub =
            rhi->createUniformBuffer(nullptr, ubLayout.get(), resource::UniformBufferUsage::SingleFrame);
        ASSERT_TRUE(ub.isValid());

        const resource::BufferCreateDesc bufDesc = resource::BufferCreateDesc::create(
            "NullGeoBuf",
            128u,
            16u,
            resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::IndexBuffer |
                resource::BufferUsageFlags::ShaderResource | resource::BufferUsageFlags::UnorderedAccess
        );
        resource::BufferRef buf = cmdList.createBuffer(bufDesc);
        view::ShaderResourceViewRef bufSRV = cmdList.createShaderResourceView(
            buf.get(), view::ViewDesc::createBufferSRV().setType(view::BufferViewType::Raw).asViewDesc()
        );
        view::UnorderedAccessViewRef bufUAV = cmdList.createUnorderedAccessView(
            buf.get(), view::ViewDesc::createBufferUAV().setType(view::BufferViewType::Raw).asViewDesc()
        );
        ASSERT_TRUE(bufSRV.isValid() && bufUAV.isValid());

        presentation::ViewportRef viewport =
            rhi->createViewport(nullptr, 1280u, 720u, false, PixelFormat::B8G8R8A8_UNorm);
        ASSERT_TRUE(viewport.isValid());
        rhi->resizeViewport(viewport.get(), 1920u, 1080u, false, PixelFormat::B8G8R8A8_UNorm);

        resource::TextureRef backBuffer = rhi->getViewportBackBuffer(viewport.get());
        ASSERT_TRUE(backBuffer.isValid());
        EXPECT_EQ(backBuffer->getDesc().extent.x, 1);
        EXPECT_EQ(backBuffer->getDesc().extent.y, 1);
        EXPECT_EQ(backBuffer->getFormat(), PixelFormat::B8G8R8A8_UNorm);

        view::ShaderResourceViewRef texSRV = cmdList.createShaderResourceView(
            backBuffer.get(), view::ViewDesc::createTextureSRV().setDimensionFromTexture(backBuffer.get()).asViewDesc()
        );
        view::UnorderedAccessViewRef texUAV = cmdList.createUnorderedAccessView(
            backBuffer.get(), view::ViewDesc::createTextureUAV().setDimensionFromTexture(backBuffer.get()).asViewDesc()
        );
        ASSERT_TRUE(texSRV.isValid() && texUAV.isValid());

        sync::GPUFenceRef fence = rhi->createGPUFence(gp::Name("NullFence"));
        ASSERT_TRUE(fence.isValid());
        EXPECT_TRUE(fence->poll());

        sync::RenderQueryPoolRef queryPool = createRenderQueryPool(sync::RenderQueryType::Occlusion, 1u);
        sync::PooledRenderQuery query = queryPool->allocateQuery();
        ASSERT_TRUE(query.isValid());

        resource::StagingBufferRef staging = rhi->createStagingBuffer();
        ASSERT_TRUE(staging.isValid());

        cmdList.beginFrame();
        cmdList.beginDrawingViewport(viewport.get(), backBuffer.get());

        // Compute pass
        cmdList.setComputePipelineState(computePSO.get());
        cmdList.clearUAVUint(bufUAV.get(), resource::UIntVector4(0u, 0u, 0u, 0u));
        cmdList.dispatchComputeShader(4u, 4u, 1u);
        cmdList.copyToStagingBuffer(buf.get(), staging.get(), 0u, 32u);
        cmdList.writeGPUFence(fence.get());

        // Graphics pass
        const command::RenderPassInfo rpInfo(backBuffer.get(), command::RenderTargetActions::ClearStore);
        cmdList.beginRenderPass(rpInfo, "NullMainPass");

        pipeline::GraphicsPipelineStateInitializer psoInit{};
        psoInit.boundShaderState = pipeline::BoundShaderStateInput(vdecl.get(), vs.get(), ps.get(), gs.get());
        psoInit.blendState = blend.get();
        psoInit.rasterizerState = raster.get();
        psoInit.depthStencilState = depth.get();
        cmdList.applyCachedRenderTargets(psoInit);
        pipeline::GraphicsPipelineStateRef graphicsPSO = rhi->createGraphicsPipelineState(psoInit);
        ASSERT_TRUE(graphicsPSO.isValid());

        cmdList.setGraphicsPipelineState(graphicsPSO.get(), 0u, true);
        cmdList.setViewport(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        cmdList.setScissorRect(false, 0u, 0u, 1u, 1u);
        cmdList.setStreamSource(0u, buf.get(), 0u);
        cmdList.beginRenderQuery(query.getQuery());
        cmdList.drawPrimitive(0u, 1u, 1u);
        cmdList.endRenderQuery(query.getQuery());
        cmdList.endRenderPass();

        cmdList.endDrawingViewport(viewport.get(), true, false);
        cmdList.endFrame();

        // getRenderQueryResult returns true and preserves outResult unchanged.
        gp::UInt64 queryValue = 0x12345678ull;
        EXPECT_TRUE(cmdList.getRenderQueryResult(query.getQuery(), queryValue, true));
        EXPECT_EQ(queryValue, 0x12345678ull);

        // Staging readback reflects the copied size.
        EXPECT_EQ(staging->getGPUSizeBytes(), 32u);
        void* const stagingPtr = rhi->lockStagingBuffer(staging.get(), fence.get(), 0u, 32u);
        EXPECT_NE(stagingPtr, nullptr);
        rhi->unlockStagingBuffer(staging.get());
    }

    // Drain any deferred resources queued during end-to-end tests.
    command::getImmediateCommandList().immediateFlush(
        command::ImmediateFlushType::FlushRHIThreadFlushResources, command::SubmitFlags::DeleteResources
    );
    gp::Vector<resource::Resource*> remaining{};
    resource::ResourceDeletionQueue::gatherResourcesToDelete(remaining, true);
    EXPECT_TRUE(remaining.isEmpty());
}

}   // namespace

}   // namespace gp::rhi::null::tests
