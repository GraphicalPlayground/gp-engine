// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/ResolveRenderPassTargets.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::command::tests
{

namespace
{

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

}   // namespace

TEST(ResolveRenderPassTargetsTest, ResolveTextureInfoConstructionAndEquality)
{
    const ResolveTextureInfo defaultInfo{};
    EXPECT_EQ(defaultInfo.sourceTexture, nullptr);
    EXPECT_EQ(defaultInfo.destTexture, nullptr);
    EXPECT_EQ(defaultInfo.mipLevel, 0u);
    EXPECT_EQ(defaultInfo.arraySlice, -1);
    EXPECT_EQ(defaultInfo.resolveRect, ResolveRect{});

    const ResolveRect rect(10, 20, 110, 220);
    const ResolveTextureInfo infoA(nullptr, nullptr, 2u, 3, rect);
    const ResolveTextureInfo infoB(nullptr, nullptr, 2u, 3, rect);
    const ResolveTextureInfo infoDiffMip(nullptr, nullptr, 1u, 3, rect);
    const ResolveTextureInfo infoDiffSlice(nullptr, nullptr, 2u, 4, rect);
    const ResolveTextureInfo infoDiffRect(nullptr, nullptr, 2u, 3, ResolveRect(0, 0, 10, 10));
    EXPECT_EQ(infoA, infoB);
    EXPECT_NE(infoA, infoDiffMip);
    EXPECT_NE(infoA, infoDiffSlice);
    EXPECT_NE(infoA, infoDiffRect);
    EXPECT_EQ(infoA.mipLevel, 2u);
    EXPECT_EQ(infoA.arraySlice, 3);
    EXPECT_EQ(infoA.resolveRect, rect);
}

TEST(ResolveRenderPassTargetsTest, ResolvesValidColorAndDepthAttachmentsAndSkipsNoOps)
{
    const ScopedBypassDeleteCheck deleteGuard;

    const resource::TextureCreateDesc colorMsaaDesc =
        resource::TextureCreateDesc::create2D("ColorMSAA", 128, 64, PixelFormat::R8G8B8A8_UNorm)
            .setNumSamples(4u)
            .setFlags(resource::TextureCreateFlags::RenderTargetable)
            .determineInitialState();
    const resource::TextureCreateDesc colorResolvedDesc =
        resource::TextureCreateDesc::create2D("ColorResolved", 128, 64, PixelFormat::R8G8B8A8_UNorm)
            .setNumSamples(1u)
            .setFlags(resource::TextureCreateFlags::ResolveTargetable)
            .determineInitialState();

    const resource::TextureCreateDesc arrayMsaaDesc =
        resource::TextureCreateDesc::create2DArray("ArrayMSAA", 128, 64, 2u, PixelFormat::R16G16B16A16_Float)
            .setNumSamples(4u)
            .setFlags(resource::TextureCreateFlags::RenderTargetable)
            .determineInitialState();
    const resource::TextureCreateDesc arrayResolvedDesc =
        resource::TextureCreateDesc::create2DArray("ArrayResolved", 128, 64, 2u, PixelFormat::R16G16B16A16_Float)
            .setNumSamples(1u)
            .setFlags(resource::TextureCreateFlags::ResolveTargetable)
            .determineInitialState();

    const resource::TextureCreateDesc depthMsaaDesc =
        resource::TextureCreateDesc::create2D("DepthMSAA", 128, 64, PixelFormat::D32_Float_S8_UInt)
            .setNumSamples(4u)
            .setFlags(resource::TextureCreateFlags::DepthStencilTargetable)
            .determineInitialState();
    const resource::TextureCreateDesc depthResolvedDesc =
        resource::TextureCreateDesc::create2D("DepthResolved", 128, 64, PixelFormat::D32_Float_S8_UInt)
            .setNumSamples(1u)
            .setFlags(resource::TextureCreateFlags::DepthStencilResolveTarget)
            .determineInitialState();

    const resource::TextureRef colorMsaa(new DummyTexture(colorMsaaDesc));
    const resource::TextureRef colorResolved(new DummyTexture(colorResolvedDesc));
    const resource::TextureRef arrayMsaa(new DummyTexture(arrayMsaaDesc));
    const resource::TextureRef arrayResolved(new DummyTexture(arrayResolvedDesc));
    const resource::TextureRef depthMsaa(new DummyTexture(depthMsaaDesc));
    const resource::TextureRef depthResolved(new DummyTexture(depthResolvedDesc));

    RenderPassInfo rpInfo{};
    rpInfo.resolveRect = ResolveRect(0, 0, 128, 64);

    // Slot 0: valid color MSAA resolve.
    rpInfo.colorRenderTargets[0].renderTarget = colorMsaa.get();
    rpInfo.colorRenderTargets[0].resolveTarget = colorResolved.get();
    rpInfo.colorRenderTargets[0].mipIndex = 1u;
    rpInfo.colorRenderTargets[0].arraySlice = 0;

    // Slot 1: self-resolve (source == dest) -> skipped.
    rpInfo.colorRenderTargets[1].renderTarget = colorResolved.get();
    rpInfo.colorRenderTargets[1].resolveTarget = colorResolved.get();

    // Slot 2: no resolve target (dest == nullptr) -> skipped.
    rpInfo.colorRenderTargets[2].renderTarget = colorMsaa.get();
    rpInfo.colorRenderTargets[2].resolveTarget = nullptr;

    // Slot 3: valid 2D array color MSAA resolve.
    rpInfo.colorRenderTargets[3].renderTarget = arrayMsaa.get();
    rpInfo.colorRenderTargets[3].resolveTarget = arrayResolved.get();
    rpInfo.colorRenderTargets[3].mipIndex = 0u;
    rpInfo.colorRenderTargets[3].arraySlice = 1;

    // Slot 4: null source with non-null destination -> skipped.
    rpInfo.colorRenderTargets[4].renderTarget = nullptr;
    rpInfo.colorRenderTargets[4].resolveTarget = colorResolved.get();

    // Depth-stencil: valid MSAA resolve.
    rpInfo.depthStencilRenderTarget.depthStencilTarget = depthMsaa.get();
    rpInfo.depthStencilRenderTarget.resolveTarget = depthResolved.get();

    gp::Vector<ResolveTextureInfo> resolved;
    resolveRenderPassTargets(
        rpInfo,
        [&resolved](const ResolveTextureInfo info) noexcept -> void
    {
        resolved.pushBack(info);
    }
    );

    ASSERT_EQ(resolved.size(), 3u);
    EXPECT_EQ(resolved[0], ResolveTextureInfo(colorMsaa.get(), colorResolved.get(), 1u, 0, rpInfo.resolveRect));
    EXPECT_EQ(resolved[1], ResolveTextureInfo(arrayMsaa.get(), arrayResolved.get(), 0u, 1, rpInfo.resolveRect));
    EXPECT_EQ(resolved[2], ResolveTextureInfo(depthMsaa.get(), depthResolved.get(), 0u, 0, rpInfo.resolveRect));
    EXPECT_NE(resolved[0], resolved[1]);

    // Empty RenderPassInfo and depth-stencil no-op cases (null resolve, null source, self-resolve) invoke 0 callbacks.
    {
        gp::UInt32 count = 0u;
        const auto counter = [&count](const ResolveTextureInfo) noexcept -> void
        {
            ++count;
        };

        const RenderPassInfo emptyPass{};
        resolveRenderPassTargets(emptyPass, counter);
        EXPECT_EQ(count, 0u);

        RenderPassInfo depthNullResolve{};
        depthNullResolve.depthStencilRenderTarget.depthStencilTarget = depthMsaa.get();
        depthNullResolve.depthStencilRenderTarget.resolveTarget = nullptr;
        resolveRenderPassTargets(depthNullResolve, counter);
        EXPECT_EQ(count, 0u);

        RenderPassInfo depthNullSource{};
        depthNullSource.depthStencilRenderTarget.depthStencilTarget = nullptr;
        depthNullSource.depthStencilRenderTarget.resolveTarget = depthResolved.get();
        resolveRenderPassTargets(depthNullSource, counter);
        EXPECT_EQ(count, 0u);

        RenderPassInfo depthSelfResolve{};
        depthSelfResolve.depthStencilRenderTarget.depthStencilTarget = depthResolved.get();
        depthSelfResolve.depthStencilRenderTarget.resolveTarget = depthResolved.get();
        resolveRenderPassTargets(depthSelfResolve, counter);
        EXPECT_EQ(count, 0u);
    }
}

}   // namespace gp::rhi::command::tests
