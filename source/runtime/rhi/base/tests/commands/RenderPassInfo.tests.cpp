// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/RenderPassInfo.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/CommandTypes.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::command::tests
{

namespace
{

class TestTexture final : public resource::Texture
{
public:
    explicit TestTexture(const resource::TextureCreateDesc& desc) noexcept
        : Texture(desc)
    {}

protected:
    virtual ~TestTexture() noexcept override = default;
};

TEST(RenderPassInfoTest, ActionPackingAndUnpackingRoundTrip)
{
    const RenderTargetActions clearStore =
        makeRenderTargetActions(RenderTargetLoadAction::Clear, RenderTargetStoreAction::Store);
    EXPECT_EQ(clearStore, RenderTargetActions::ClearStore);
    EXPECT_EQ(getLoadAction(clearStore), RenderTargetLoadAction::Clear);
    EXPECT_EQ(getStoreAction(clearStore), RenderTargetStoreAction::Store);

    const DepthStencilTargetActions depthLoadStencilLoad =
        makeDepthStencilTargetActions(RenderTargetActions::LoadStore, RenderTargetActions::LoadDontStore);
    EXPECT_EQ(depthLoadStencilLoad, DepthStencilTargetActions::LoadDepthStencilStoreDepthNotStencil);
    EXPECT_EQ(getDepthActions(depthLoadStencilLoad), RenderTargetActions::LoadStore);
    EXPECT_EQ(getStencilActions(depthLoadStencilLoad), RenderTargetActions::LoadDontStore);

    RenderTargetLoadAction colorLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction colorStore = RenderTargetStoreAction::NoAction;
    RenderTargetLoadAction depthLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction depthStore = RenderTargetStoreAction::NoAction;
    RenderTargetLoadAction stencilLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction stencilStore = RenderTargetStoreAction::NoAction;
    decodeRenderTargetMode(
        SimpleRenderTargetMode::ExistingColorAndDepthAndClearStencil,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        resource::ExclusiveDepthStencil::kDepthWriteStencilWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Clear);
}

TEST(RenderPassInfoTest, CommandTypesPODContracts)
{
    const ViewportBounds vp{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    const ViewportBounds vpCopy = vp;
    EXPECT_EQ(vp, vpCopy);

    const DispatchIndirectParameters dispatchParams{ 8u, 4u, 2u };
    EXPECT_EQ(dispatchParams.threadGroupCountX, 8u);
    EXPECT_EQ(dispatchParams.threadGroupCountY, 4u);
    EXPECT_EQ(dispatchParams.threadGroupCountZ, 2u);

    const BufferRange range{ nullptr, 16u, 64u };
    const BufferRange rangeCopy{ nullptr, 16u, 64u };
    EXPECT_EQ(range, rangeCopy);
    EXPECT_EQ(range.offset, 16u);
    EXPECT_EQ(range.size, 64u);

    const ResolveRect defaultRect{};
    EXPECT_FALSE(defaultRect.isValid());
    const ResolveRect validRect{ 0, 0, 64, 64 };
    EXPECT_TRUE(validRect.isValid());
}

TEST(RenderPassInfoTest, ColorAndDepthExtractionAndConversion)
{
    const resource::TextureCreateDesc colorDesc =
        resource::TextureCreateDesc::create2D("ColorRT")
            .setExtent(256, 256)
            .setFormat(PixelFormat::R8G8B8A8_UNorm)
            .setNumSamples(4u)
            .addFlags(resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::SRGB);
    const resource::TextureCreateDesc resolveDesc = resource::TextureCreateDesc::create2D("ResolveRT")
                                                        .setExtent(256, 256)
                                                        .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                                        .addFlags(resource::TextureCreateFlags::ResolveTargetable);
    const resource::TextureCreateDesc depthDesc = resource::TextureCreateDesc::create2D("DepthRT")
                                                      .setExtent(256, 256)
                                                      .setFormat(PixelFormat::D32_Float_S8_UInt)
                                                      .setNumSamples(4u)
                                                      .addFlags(resource::TextureCreateFlags::DepthStencilTargetable);

    resource::TextureRef colorTex(new TestTexture(colorDesc));
    resource::TextureRef resolveTex(new TestTexture(resolveDesc));
    resource::TextureRef depthTex(new TestTexture(depthDesc));

    RenderPassInfo rpInfo(
        colorTex.get(),
        RenderTargetActions::ClearStore,
        depthTex.get(),
        DepthStencilTargetActions::ClearDepthStencilStoreDepthStencil,
        resource::ExclusiveDepthStencil::kDepthWriteStencilWrite,
        resolveTex.get(),
        nullptr
    );
    rpInfo.accessRect = resource::IntRect(0, 0, 128, 128);
    rpInfo.multiViewCount = 2u;

    EXPECT_TRUE(rpInfo.validate());
    EXPECT_TRUE(rpInfo.hasAccessRect());
    EXPECT_EQ(rpInfo.getNumColorRenderTargets(), 1u);

    const pipeline::GraphicsPipelineRenderTargetsInfo psoRT = rpInfo.extractRenderTargetsInfo();
    EXPECT_EQ(psoRT.renderTargetsEnabled, 1u);
    EXPECT_EQ(psoRT.renderTargetFormats[0], PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(psoRT.depthStencilTargetFormat, PixelFormat::D32_Float_S8_UInt);
    EXPECT_EQ(psoRT.depthTargetLoadAction, RenderTargetLoadAction::Clear);
    EXPECT_EQ(psoRT.depthTargetStoreAction, RenderTargetStoreAction::Store);
    EXPECT_EQ(psoRT.stencilTargetLoadAction, RenderTargetLoadAction::Clear);
    EXPECT_EQ(psoRT.stencilTargetStoreAction, RenderTargetStoreAction::Store);
    EXPECT_EQ(psoRT.numSamples, 4u);
    EXPECT_EQ(psoRT.multiViewCount, 2u);

    SetRenderTargetsInfo rtInfo{};
    rpInfo.convertToRenderTargetsInfo(rtInfo);
    EXPECT_EQ(rtInfo.numColorRenderTargets, 1);
    EXPECT_TRUE(rtInfo.clearColor);
    EXPECT_TRUE(rtInfo.clearDepth);
    EXPECT_TRUE(rtInfo.clearStencil);
    EXPECT_TRUE(rtInfo.hasResolveAttachments);
    EXPECT_EQ(rtInfo.colorRenderTarget[0].texture, colorTex.get());
    EXPECT_EQ(rtInfo.colorResolveRenderTarget[0].texture, resolveTex.get());
    EXPECT_EQ(rtInfo.depthStencilRenderTarget.texture, depthTex.get());
    EXPECT_EQ(rtInfo.depthStencilRenderTarget.getStencilStoreAction(), RenderTargetStoreAction::Store);
    EXPECT_EQ(
        rtInfo.depthStencilRenderTarget.getDepthStencilAccess(),
        resource::ExclusiveDepthStencil::kDepthWriteStencilWrite
    );

    const gp::UInt64 hash1 = rtInfo.calculateHash();
    rtInfo.setClearDepthStencil(false, false);
    const gp::UInt64 hash2 = rtInfo.calculateHash();
    EXPECT_NE(hash1, hash2);
}

TEST(RenderPassInfoTest, ValidateDetectsGapsAndSampleMismatch)
{
    const resource::TextureCreateDesc desc1x =
        resource::TextureCreateDesc::create2D("RT1x").setExtent(64, 64).setFormat(PixelFormat::R8G8B8A8_UNorm);
    const resource::TextureCreateDesc desc4x = resource::TextureCreateDesc::create2D("RT4x")
                                                   .setExtent(64, 64)
                                                   .setFormat(PixelFormat::R8G8B8A8_UNorm)
                                                   .setNumSamples(4u);

    resource::TextureRef tex1x(new TestTexture(desc1x));
    resource::TextureRef tex4x(new TestTexture(desc4x));

    // Gap in MRT slots (slot 0 null, slot 1 non-null) fails validation.
    RenderPassInfo gapped{};
    gapped.colorRenderTargets[1].renderTarget = tex1x.get();
    EXPECT_FALSE(gapped.validate());

    // Mismatched sample counts between color targets fail validation.
    resource::Texture* const mismatchColors[2] = { tex1x.get(), tex4x.get() };
    const RenderPassInfo colorMismatch(2u, mismatchColors, RenderTargetActions::LoadStore);
    EXPECT_FALSE(colorMismatch.validate());

    // Mismatched sample count between color and depth fails validation.
    const RenderPassInfo depthMismatch(
        tex1x.get(),
        RenderTargetActions::LoadStore,
        tex4x.get(),
        DepthStencilTargetActions::LoadDepthStencilStoreDepthStencil
    );
    EXPECT_FALSE(depthMismatch.validate());

    // Depth-only pass with matching sample count succeeds.
    const RenderPassInfo depthOnly(tex4x.get(), DepthStencilTargetActions::ClearDepthStencilStoreDepthStencil);
    EXPECT_TRUE(depthOnly.validate());
    EXPECT_EQ(depthOnly.getNumColorRenderTargets(), 0u);
    EXPECT_EQ(depthOnly.extractRenderTargetsInfo().numSamples, 4u);
}

}   // namespace

}   // namespace gp::rhi::command::tests
