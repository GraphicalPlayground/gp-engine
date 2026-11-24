// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/ResourceState.hpp"
#include "commands/IComputeContext.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "device/RHIGlobals.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::sync::tests
{

namespace
{

class DummyComputeContext final : public command::IComputeContext
{
public:
    virtual void setComputePipelineState(pipeline::ComputePipelineState*) noexcept override
    {}

    virtual void dispatchComputeShader(gp::UInt32, gp::UInt32, gp::UInt32) noexcept override
    {}

    virtual void dispatchIndirectComputeShader(resource::Buffer*, gp::UInt32) noexcept override
    {}

    virtual void beginTransitions(gp::ConstVectorView<const Transition*>) noexcept override
    {}

    virtual void endTransitions(gp::ConstVectorView<const Transition*>) noexcept override
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

    virtual void writeGPUFence(GPUFence*) noexcept override
    {}
};

class DummyBuffer final : public resource::Buffer
{
public:
    explicit DummyBuffer(const resource::BufferCreateDesc& createDesc) noexcept
        : Buffer(createDesc)
    {}
};

class DummyTexture final : public resource::Texture
{
public:
    explicit DummyTexture(const resource::TextureCreateDesc& createDesc) noexcept
        : Texture(createDesc)
    {}
};

struct ScopedBypassDeleteCheck final
{
    bool previousBypass{ false };

    ScopedBypassDeleteCheck() noexcept
        : previousBypass(resource::ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit())
    {
        resource::ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(true);
    }

    ~ScopedBypassDeleteCheck() noexcept
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        resource::ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(previousBypass);
    }
};

struct ScopedDiscardTracking final
{
    bool previousState{ false };

    explicit ScopedDiscardTracking(const bool enabled) noexcept
        : previousState(getRHIGlobals().needsTransientDiscardStateTracking)
    {
        getRHIGlobals().needsTransientDiscardStateTracking = enabled;
    }

    ~ScopedDiscardTracking() noexcept
    {
        getRHIGlobals().needsTransientDiscardStateTracking = previousState;
    }
};

}   // namespace

TEST(ResourceStateTest, ExplicitAndTrackedUnknownResolution)
{
    const ScopedBypassDeleteCheck deleteGuard;
    DummyComputeContext context;

    const resource::BufferCreateDesc bufDesc = resource::BufferCreateDesc::createStructured("StateBuf", 64u, 16u)
                                                   .addUsage(resource::BufferUsageFlags::UnorderedAccess)
                                                   .setInitialState(ResourceAccess::UAVCompute);
    const resource::BufferRef buffer(new DummyBuffer(bufDesc));
    context.setTrackedAccess(buffer.get(), ResourceAccess::UAVCompute, PipelineStage::AsyncCompute);

    // 1. Explicit before and after states preserve declared values and pipelines.
    {
        const TransitionInfo info(buffer.get(), ResourceAccess::CopyDest, ResourceAccess::SRVGraphics);
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::Graphics, info);
        EXPECT_EQ(state.accessBefore, ResourceAccess::CopyDest);
        EXPECT_EQ(state.accessAfter, ResourceAccess::SRVGraphics);
        EXPECT_EQ(state.srcPipelines, PipelineStage::Graphics);
        EXPECT_EQ(state.dstPipelines, PipelineStage::Graphics);
    }

    // 2. Unknown accessBefore fetches tracked access and tracked pipelines.
    {
        const TransitionInfo info(buffer.get(), ResourceAccess::Unknown, ResourceAccess::SRVGraphics);
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::Graphics, info);
        EXPECT_EQ(state.accessBefore, ResourceAccess::UAVCompute);
        EXPECT_EQ(state.accessAfter, ResourceAccess::SRVGraphics);
        EXPECT_EQ(state.srcPipelines, PipelineStage::AsyncCompute);
        EXPECT_EQ(state.dstPipelines, PipelineStage::Graphics);
    }

    // 3. Unknown accessAfter fetches tracked access while preserving explicit accessBefore and srcPipelines.
    {
        const TransitionInfo info(buffer.get(), ResourceAccess::CopySrc, ResourceAccess::Unknown);
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, info);
        EXPECT_EQ(state.accessBefore, ResourceAccess::CopySrc);
        EXPECT_EQ(state.accessAfter, ResourceAccess::UAVCompute);
        EXPECT_EQ(state.srcPipelines, PipelineStage::Graphics);
        EXPECT_EQ(state.dstPipelines, PipelineStage::AsyncCompute);
    }

    // 4. Both Unknown resolves both from tracked state.
    {
        const TransitionInfo info(buffer.get(), ResourceAccess::Unknown, ResourceAccess::Unknown);
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::Graphics, info);
        EXPECT_EQ(state.accessBefore, ResourceAccess::UAVCompute);
        EXPECT_EQ(state.accessAfter, ResourceAccess::UAVCompute);
        EXPECT_EQ(state.srcPipelines, PipelineStage::AsyncCompute);
        EXPECT_EQ(state.dstPipelines, PipelineStage::Graphics);
    }
}

TEST(ResourceStateTest, TransientDiscardTrackingUAVUnwrappingAndNullResource)
{
    const ScopedBypassDeleteCheck deleteGuard;
    DummyComputeContext context;

    const resource::TextureCreateDesc texDesc =
        resource::TextureCreateDesc::create2D("DiscardTex", 32, 32, PixelFormat::R8G8B8A8_UNorm)
            .setFlags(resource::TextureCreateFlags::UAV | resource::TextureCreateFlags::ShaderResource)
            .setInitialState(ResourceAccess::RTV);
    const resource::TextureRef texture(new DummyTexture(texDesc));
    context.setTrackedAccess(texture.get(), ResourceAccess::RTV, PipelineStage::All);

    // Discard with tracking disabled keeps Discard and declared srcPipelines.
    {
        const ScopedDiscardTracking guard(false);
        const TransitionInfo info(texture.get(), ResourceAccess::Discard, ResourceAccess::UAVCompute);
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, info);
        EXPECT_EQ(state.accessBefore, ResourceAccess::Discard);
        EXPECT_EQ(state.accessAfter, ResourceAccess::UAVCompute);
        EXPECT_EQ(state.srcPipelines, PipelineStage::Graphics);
        EXPECT_EQ(state.dstPipelines, PipelineStage::AsyncCompute);
    }

    // Discard with tracking enabled replaces accessBefore and srcPipelines from tracked state.
    {
        const ScopedDiscardTracking guard(true);
        const TransitionInfo info(texture.get(), ResourceAccess::Discard, ResourceAccess::UAVCompute);
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, info);
        EXPECT_EQ(state.accessBefore, ResourceAccess::RTV);
        EXPECT_EQ(state.accessAfter, ResourceAccess::UAVCompute);
        EXPECT_EQ(state.srcPipelines, PipelineStage::All);
        EXPECT_EQ(state.dstPipelines, PipelineStage::AsyncCompute);
    }

    // Discard combined with Unknown accessAfter (both tracking off and on).
    {
        const ScopedDiscardTracking guardOff(false);
        const TransitionInfo info(texture.get(), ResourceAccess::Discard, ResourceAccess::Unknown);
        const ResourceState stateOff(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, info);
        EXPECT_EQ(stateOff.accessBefore, ResourceAccess::Discard);
        EXPECT_EQ(stateOff.accessAfter, ResourceAccess::RTV);
        EXPECT_EQ(stateOff.srcPipelines, PipelineStage::Graphics);
        EXPECT_EQ(stateOff.dstPipelines, PipelineStage::AsyncCompute);

        const ScopedDiscardTracking guardOn(true);
        const ResourceState stateOn(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, info);
        EXPECT_EQ(stateOn.accessBefore, ResourceAccess::RTV);
        EXPECT_EQ(stateOn.accessAfter, ResourceAccess::RTV);
        EXPECT_EQ(stateOn.srcPipelines, PipelineStage::All);
        EXPECT_EQ(stateOn.dstPipelines, PipelineStage::AsyncCompute);
    }

    // UAV TransitionInfo unwraps to underlying texture's tracked state.
    {
        const view::ViewDesc uavDesc =
            view::ViewDesc::createTextureUAV().setDimensionFromTexture(texture.get()).setMipLevel(0u).asViewDesc();
        const view::UnorderedAccessViewRef uav(new view::UnorderedAccessView(texture.get(), uavDesc));

        const TransitionInfo info(uav.get(), ResourceAccess::Unknown, ResourceAccess::SRVCompute);
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, info);
        EXPECT_EQ(state.accessBefore, ResourceAccess::RTV);
        EXPECT_EQ(state.accessAfter, ResourceAccess::SRVCompute);
        EXPECT_EQ(state.srcPipelines, PipelineStage::All);
        EXPECT_EQ(state.dstPipelines, PipelineStage::AsyncCompute);
    }

    // Non-viewable resource (BVH): getViewableResource() returns nullptr, leaving states untouched.
    {
        TransitionInfo bvhInfo{};
        bvhInfo.resource = texture.get();
        bvhInfo.type = TransitionResourceType::BVH;
        bvhInfo.accessBefore = ResourceAccess::Unknown;
        bvhInfo.accessAfter = ResourceAccess::BVHRead;
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, bvhInfo);
        EXPECT_EQ(state.accessBefore, ResourceAccess::Unknown);
        EXPECT_EQ(state.accessAfter, ResourceAccess::BVHRead);
        EXPECT_EQ(state.srcPipelines, PipelineStage::Graphics);
        EXPECT_EQ(state.dstPipelines, PipelineStage::AsyncCompute);
    }

    // Null/empty TransitionInfo leaves fields untouched without asserting.
    {
        const TransitionInfo emptyInfo{};
        const ResourceState state(context, PipelineStage::Graphics, PipelineStage::AsyncCompute, emptyInfo);
        EXPECT_EQ(state.accessBefore, ResourceAccess::Unknown);
        EXPECT_EQ(state.accessAfter, ResourceAccess::Unknown);
        EXPECT_EQ(state.srcPipelines, PipelineStage::Graphics);
        EXPECT_EQ(state.dstPipelines, PipelineStage::AsyncCompute);
    }
}

}   // namespace gp::rhi::sync::tests
