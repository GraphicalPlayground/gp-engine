// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/TextureInitializerHelpers.hpp"
#include "commands/CommandListImmediate.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "device/DynamicRHI.hpp"
#include "memory/Memory.hpp"
#include "resources/TextureSubresourceHelpers.hpp"
#include "resources/TextureUtils.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::resource::tests
{

namespace
{

struct MockTexture final : public Texture
{
    explicit MockTexture(const TextureCreateDesc& createDesc) noexcept
        : Texture(createDesc)
    {
        backing.resize(static_cast<gp::USize>(calculateTextureSize(createDesc)), 0u);
    }

    gp::Vector<gp::UInt8> backing{};
};

struct MockLockTextureDynamicRHI final : public DynamicRHI
{
    gp::UInt32 lockCalls{ 0u };
    gp::UInt32 unlockCalls{ 0u };

    virtual void init() noexcept override
    {}

    virtual void shutdown() noexcept override
    {}

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "MockLockTexture";
    }

    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override
    {
        return InterfaceType::Null;
    }

    [[nodiscard]] virtual pipeline::SamplerStateRef
        createSamplerState(const pipeline::SamplerStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::RasterizerStateRef
        createRasterizerState(const pipeline::RasterizerStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::DepthStencilStateRef
        createDepthStencilState(const pipeline::DepthStencilStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::BlendStateRef createBlendState(const pipeline::BlendStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::VertexDeclarationRef
        createVertexDeclaration(const pipeline::VertexDeclarationElementList&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::GraphicsPipelineStateRef
        createGraphicsPipelineState(const pipeline::GraphicsPipelineStateInitializer&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::ComputePipelineStateRef
        createComputePipelineState(shader::ComputeShader*) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual sync::RenderQueryRef createRenderQuery(sync::RenderQueryType) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual bool getRenderQueryResult(sync::RenderQuery*, gp::UInt64&, bool, gp::UInt32) noexcept override
    {
        return false;
    }

    [[nodiscard]] virtual shader::VertexShaderRef
        createVertexShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::PixelShaderRef
        createPixelShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::GeometryShaderRef
        createGeometryShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::ComputeShaderRef
        createComputeShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        pipeline::VertexDeclaration*, shader::VertexShader*, shader::PixelShader*, shader::GeometryShader*
    ) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual UniformBufferRef createUniformBuffer(
        const void*, const UniformBufferLayout*, UniformBufferUsage, UniformBufferValidation
    ) noexcept override
    {
        return {};
    }

    virtual void updateUniformBuffer(command::CommandListBase&, UniformBuffer*, const void*) noexcept override
    {}

    [[nodiscard]] virtual BufferInitializer
        createBufferInitializer(command::CommandListBase& cmdList, const BufferCreateDesc&) noexcept override
    {
        return BufferInitializer(cmdList, BufferRef{}, nullptr, 0u, BufferInitializer::FinalizeCallback{});
    }

    [[nodiscard]] virtual void* lockBufferBottomOfPipe(
        command::CommandListBase&, Buffer*, gp::UInt32, gp::UInt32, ResourceLockMode
    ) noexcept override
    {
        return nullptr;
    }

    virtual void unlockBufferBottomOfPipe(command::CommandListBase&, Buffer*) noexcept override
    {}

    [[nodiscard]] virtual TextureInitializer createTextureInitializer(
        command::CommandListBase& cmdList, const TextureCreateDesc& createDesc
    ) noexcept override
    {
        return DefaultTextureInitializer(cmdList, new MockTexture(createDesc));
    }

    [[nodiscard]] virtual TextureLockResult
        lockTexture(command::CommandListImmediate& cmdList, const TextureLockArgs& args) noexcept override
    {
        static_cast<void>(cmdList);
        ++lockCalls;

        MockTexture* const mockTexture = static_cast<MockTexture*>(args.texture);
        const TextureDesc& desc = mockTexture->getDesc();

        gp::UInt64 rowStride = 0u;
        gp::UInt64 subresourceSize = 0u;
        const gp::UInt64 offset = calculateSubresourceOffset(
            desc, static_cast<gp::UInt32>(args.faceIndex), args.arrayIndex, args.mipIndex, rowStride, subresourceSize
        );

        return TextureLockResult{ mockTexture->backing.data() + offset,
                                  subresourceSize,
                                  static_cast<gp::UInt32>(rowStride) };
    }

    virtual void unlockTexture(command::CommandListImmediate& cmdList, const TextureLockArgs& args) noexcept override
    {
        static_cast<void>(cmdList);
        static_cast<void>(args);
        ++unlockCalls;
    }

    virtual void updateTexture2D(
        command::CommandListBase&, Texture*, gp::UInt32, const UpdateTextureRegion2D&, gp::UInt32, const gp::UInt8*
    ) noexcept override
    {}

    virtual void updateTexture3D(
        command::CommandListBase&,
        Texture*,
        gp::UInt32,
        const UpdateTextureRegion3D&,
        gp::UInt32,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    virtual void readSurfaceData(
        Texture*, const IntRect&, gp::Vector<gp::math::Color>&, const ReadSurfaceDataFlags&
    ) noexcept override
    {}

    [[nodiscard]] virtual view::ShaderResourceViewRef
        createShaderResourceView(command::CommandListBase&, Buffer*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::ShaderResourceViewRef
        createShaderResourceView(command::CommandListBase&, Texture*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef
        createUnorderedAccessView(command::CommandListBase&, Buffer*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef
        createUnorderedAccessView(command::CommandListBase&, Texture*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(gp::Name) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual presentation::ViewportRef
        createViewport(void*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {
        return {};
    }

    virtual void resizeViewport(presentation::Viewport*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {}

    virtual void beginDrawingViewport(presentation::Viewport*, Texture*) noexcept override
    {}

    using DynamicRHI::endDrawingViewport;

    virtual void
        endDrawingViewport(presentation::Viewport*, const presentation::PresentArgsWithStats&) noexcept override
    {}

    [[nodiscard]] virtual TextureRef getViewportBackBuffer(presentation::Viewport*) noexcept override
    {
        return {};
    }

    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport*) noexcept override
    {}

    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] virtual command::IComputeContext* getDefaultAsyncComputeContext() noexcept override
    {
        return nullptr;
    }

    virtual void blockUntilGPUIdle() noexcept override
    {}
};

struct ScopedBypassDeleteCheck final
{
    bool previousBypass{ false };

    ScopedBypassDeleteCheck() noexcept
        : previousBypass(ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit())
    {
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(true);
    }

    ~ScopedBypassDeleteCheck() noexcept
    {
        ResourceDeletionQueue::flushPendingDeletes(true);
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(previousBypass);
    }
};

struct ScopedDynamicRHI final
{
    DynamicRHI* previousRHI{ nullptr };

    explicit ScopedDynamicRHI(DynamicRHI* const rhi) noexcept
        : previousRHI(getDynamicRHI())
    {
        setDynamicRHI(rhi);
    }

    ~ScopedDynamicRHI() noexcept
    {
        setDynamicRHI(previousRHI);
    }
};

}   // namespace

TEST(TextureSubresourceHelpersTest, CombinedArrayIndexLockArrayIndexAndSubresourceCount)
{
    TextureDesc tex2D(TextureDimension::Texture2D);
    tex2D.numMips = 4u;
    EXPECT_EQ(getCombinedArrayIndex(tex2D, 3u, 2u), 0u);
    EXPECT_EQ(getSubresourceCount(tex2D), 4u);
    EXPECT_EQ(getSubresourceCount(tex2D, 2), 2u);
    EXPECT_EQ(getSubresourceCount(tex2D, 4), 0u);

    TextureDesc tex2DArray(TextureDimension::Texture2DArray);
    tex2DArray.arraySize = 3u;
    tex2DArray.numMips = 2u;
    EXPECT_EQ(getCombinedArrayIndex(tex2DArray, 2u, 1u), 1u);
    EXPECT_EQ(
        getLockArrayIndex(tex2DArray, TextureLockArgs::lock2DArray(nullptr, 2u, 0u, ResourceLockMode::WriteOnly)), 2u
    );
    EXPECT_EQ(getSubresourceCount(tex2DArray), 6u);
    EXPECT_EQ(getSubresourceCount(tex2DArray, 1), 3u);

    TextureDesc texCube(TextureDimension::TextureCube);
    texCube.arraySize = 1u;
    texCube.numMips = 3u;
    EXPECT_EQ(getCombinedArrayIndex(texCube, 4u, 0u), 4u);
    EXPECT_EQ(
        getLockArrayIndex(
            texCube, TextureLockArgs::lockCubeFace(nullptr, CubeFace::PosZ, 0u, 0u, ResourceLockMode::WriteOnly)
        ),
        4u
    );
    EXPECT_EQ(getSubresourceCount(texCube), 18u);

    TextureDesc texCubeArray(TextureDimension::TextureCubeArray);
    texCubeArray.arraySize = 2u;
    texCubeArray.numMips = 3u;
    EXPECT_EQ(getCombinedArrayIndex(texCubeArray, 5u, 1u), 11u);
    EXPECT_EQ(getSubresourceCount(texCubeArray, 1), 24u);
}

TEST(TextureSubresourceHelpersTest, EnumerateSubresourcesFollowsArrayFaceMipOrder)
{
    TextureDesc desc(TextureDimension::TextureCubeArray);
    desc.arraySize = 2u;
    desc.numMips = 3u;

    gp::Vector<TextureSubresourceIndex> visitedIndices;
    gp::Vector<gp::UInt32> visitedCombined;

    enumerateSubresources(
        desc,
        1,
        [&visitedIndices,
         &visitedCombined](const TextureSubresourceIndex index, const gp::UInt32 combinedArrayIndex) noexcept -> void
    {
        visitedIndices.pushBack(index);
        visitedCombined.pushBack(combinedArrayIndex);
    }
    );

    // 2 arrays * 6 faces * 2 mips (mips 1 and 2) = 24 subresources.
    ASSERT_EQ(visitedIndices.size(), 24u);
    const TextureSubresourceIndex expected0{ 0, 0, 1 };
    const TextureSubresourceIndex expected1{ 0, 0, 2 };
    const TextureSubresourceIndex expected2{ 1, 0, 1 };
    const TextureSubresourceIndex expected12{ 0, 1, 1 };
    const TextureSubresourceIndex expected23{ 5, 1, 2 };
    EXPECT_EQ(visitedIndices[0], expected0);
    EXPECT_EQ(visitedCombined[0], 0u);
    EXPECT_EQ(visitedIndices[1], expected1);
    EXPECT_EQ(visitedIndices[2], expected2);
    EXPECT_EQ(visitedCombined[2], 1u);
    EXPECT_EQ(visitedIndices[12], expected12);
    EXPECT_EQ(visitedCombined[12], 6u);
    EXPECT_EQ(visitedIndices[23], expected23);
    EXPECT_EQ(visitedCombined[23], 11u);
}

TEST(TextureSubresourceHelpersTest, CopyTextureData2DAndCopyDataProviderSubresource)
{
    // Uncompressed RGBA8 repitch: 2x2 image, source stride 12 (8 bytes data + 4 pad), dest stride 8.
    {
        const gp::Array<gp::UInt8, 24u> src{
            1,  2,  3,  4,  5,  6,  7,  8,  99, 99, 99, 99,   // row 0
            11, 12, 13, 14, 15, 16, 17, 18, 88, 88, 88, 88    // row 1
        };
        gp::Array<gp::UInt8, 16u> dst{};
        dst.fill(0xCCu);

        TextureDataProvider::SubresourceData subData{ src.data(), src.size(), 12u };
        copyDataProviderSubresource(subData, dst.data(), dst.size(), 8u, 2u, 1u, PixelFormat::R8G8B8A8_UNorm);

        const gp::Array<gp::UInt8, 16u> expected{ 1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 18 };
        EXPECT_EQ(dst, expected);
    }

    // Block-compressed BC7 (4x4 blocks, 16 bytes/block): 8x8 texels -> 2 block rows of 32 bytes each.
    // Source has padded rowPitch 48; dest has tight stride 32.
    {
        gp::Vector<gp::UInt8> src;
        src.resize(96u, 0xEEu);
        for (gp::UInt32 i = 0u; i < 32u; ++i)
        {
            src[i] = static_cast<gp::UInt8>(i + 1u);
            src[48u + i] = static_cast<gp::UInt8>(i + 65u);
        }

        gp::Vector<gp::UInt8> dst;
        dst.resize(64u, 0u);

        TextureDataProvider::SubresourceData subData{ src.data(), src.size(), 48u };
        copyDataProviderSubresource(subData, dst.data(), dst.size(), 32u, 8u, 1u, PixelFormat::BC7_UNorm);

        for (gp::UInt32 i = 0u; i < 32u; ++i)
        {
            EXPECT_EQ(dst[i], static_cast<gp::UInt8>(i + 1u));
            EXPECT_EQ(dst[32u + i], static_cast<gp::UInt8>(i + 65u));
        }
    }

    // 3D volume repitch: 2x2x2 R8_UNorm, source stride 4, dest stride 2.
    {
        const gp::Array<gp::UInt8, 16u> src{
            1, 2, 0, 0,   // slice 0 row 0
            3, 4, 0, 0,   // slice 0 row 1
            5, 6, 0, 0,   // slice 1 row 0
            7, 8, 0, 0    // slice 1 row 1
        };
        gp::Array<gp::UInt8, 8u> dst{};

        TextureDataProvider::SubresourceData subData{ src.data(), src.size(), 4u };
        copyDataProviderSubresource(subData, dst.data(), dst.size(), 2u, 2u, 2u, PixelFormat::R8_UNorm);

        const gp::Array<gp::UInt8, 8u> expected{ 1, 2, 3, 4, 5, 6, 7, 8 };
        EXPECT_EQ(dst, expected);
    }

    // Matching pitch fast path clamps to min(source.size, destSize).
    {
        const gp::Array<gp::UInt8, 8u> src{ 10, 20, 30, 40, 50, 60, 70, 80 };
        gp::Array<gp::UInt8, 4u> dst{};
        TextureDataProvider::SubresourceData subData{ src.data(), src.size(), 4u };
        copyDataProviderSubresource(subData, dst.data(), dst.size(), 4u, 1u, 1u, PixelFormat::R8G8B8A8_UNorm);
        EXPECT_EQ(dst[0], 10u);
        EXPECT_EQ(dst[3], 40u);
    }

    // Fast path when source.size < destSize, source.rowPitch == 0, or destStride == 0.
    {
        const gp::Array<gp::UInt8, 4u> src{ 7, 8, 9, 10 };
        gp::Array<gp::UInt8, 8u> dst{};
        dst.fill(0xAAu);

        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ src.data(), src.size(), 0u },
            dst.data(),
            dst.size(),
            8u,
            1u,
            1u,
            PixelFormat::R8_UNorm
        );
        EXPECT_EQ(dst[0], 7u);
        EXPECT_EQ(dst[3], 10u);
        EXPECT_EQ(dst[4], 0xAAu);

        dst.fill(0xBBu);
        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ src.data(), src.size(), 4u },
            dst.data(),
            dst.size(),
            0u,
            1u,
            1u,
            PixelFormat::R8_UNorm
        );
        EXPECT_EQ(dst[0], 7u);
        EXPECT_EQ(dst[3], 10u);
        EXPECT_EQ(dst[4], 0xBBu);
    }

    // Null / zero guards and Unknown pixel format are safe no-ops.
    {
        gp::Array<gp::UInt8, 8u> dst{};
        dst.fill(0x55u);
        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ nullptr, 8u, 4u },
            dst.data(),
            dst.size(),
            4u,
            2u,
            1u,
            PixelFormat::R8_UNorm
        );
        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ dst.data(), 8u, 4u }, nullptr, 8u, 4u, 2u, 1u, PixelFormat::R8_UNorm
        );
        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ dst.data(), 0u, 4u },
            dst.data(),
            dst.size(),
            8u,
            2u,
            1u,
            PixelFormat::R8_UNorm
        );
        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ dst.data(), 8u, 4u },
            dst.data(),
            dst.size(),
            8u,
            2u,
            1u,
            PixelFormat::Unknown
        );
        EXPECT_EQ(dst[0], 0x55u);
    }

    // Expanding repitch (source.rowPitch < destStride), destDepth == 0 treated as 1, and slice bounds truncation.
    {
        const gp::Array<gp::UInt8, 4u> src{ 1, 2, 3, 4 };
        gp::Array<gp::UInt8, 8u> dst{};
        dst.fill(0xCCu);

        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ src.data(), src.size(), 2u },
            dst.data(),
            dst.size(),
            4u,
            2u,
            0u,
            PixelFormat::R8_UNorm
        );
        const gp::Array<gp::UInt8, 8u> expected{ 1, 2, 0xCCu, 0xCCu, 3, 4, 0xCCu, 0xCCu };
        EXPECT_EQ(dst, expected);

        // Truncated destination (only fits 1 row out of 2 slices * 2 rows): stops safely without OOB write.
        gp::Array<gp::UInt8, 4u> shortDst{};
        shortDst.fill(0xDDu);
        copyDataProviderSubresource(
            TextureDataProvider::SubresourceData{ src.data(), src.size(), 2u },
            shortDst.data(),
            shortDst.size(),
            4u,
            2u,
            2u,
            PixelFormat::R8_UNorm
        );
        EXPECT_EQ(shortDst[0], 1u);
        EXPECT_EQ(shortDst[1], 2u);
        EXPECT_EQ(shortDst[2], 0xDDu);
    }

    // Direct copyTextureData2D coverage: null/zero/invalid guards, matching/zero stride, expanding pitch, non-aligned
    // BC7 height (sizeY = 5 -> 2 block rows).
    {
        const gp::Array<gp::UInt8, 8u> src{ 1, 2, 3, 4, 5, 6, 7, 8 };
        gp::Array<gp::UInt8, 8u> dst{};
        dst.fill(0x99u);

        copyTextureData2D(nullptr, dst.data(), 2u, PixelFormat::R8_UNorm, 4u, 4u);
        copyTextureData2D(src.data(), nullptr, 2u, PixelFormat::R8_UNorm, 4u, 4u);
        copyTextureData2D(src.data(), dst.data(), 0u, PixelFormat::R8_UNorm, 4u, 4u);
        copyTextureData2D(src.data(), dst.data(), 2u, PixelFormat::Unknown, 4u, 4u);
        EXPECT_EQ(dst[0], 0x99u);

        // Matching stride fast copy.
        copyTextureData2D(src.data(), dst.data(), 2u, PixelFormat::R8_UNorm, 4u, 4u);
        EXPECT_EQ(dst, src);

        // Zero source stride falls back to destStride.
        dst.fill(0u);
        copyTextureData2D(src.data(), dst.data(), 2u, PixelFormat::R8_UNorm, 0u, 4u);
        EXPECT_EQ(dst, src);

        // Non-block-aligned BC7 height: sizeY = 5 -> ceil(5/4) = 2 block rows.
        gp::Vector<gp::UInt8> bcSrc;
        bcSrc.resize(32u, 0x11u);
        for (gp::UInt32 i = 0u; i < 16u; ++i)
        {
            bcSrc[16u + i] = 0x22u;
        }
        gp::Vector<gp::UInt8> bcDst;
        bcDst.resize(64u, 0xFFu);
        copyTextureData2D(bcSrc.data(), bcDst.data(), 5u, PixelFormat::BC7_UNorm, 16u, 32u);
        EXPECT_EQ(bcDst[0], 0x11u);
        EXPECT_EQ(bcDst[15], 0x11u);
        EXPECT_EQ(bcDst[16], 0xFFu);
        EXPECT_EQ(bcDst[32], 0x22u);
        EXPECT_EQ(bcDst[47], 0x22u);
        EXPECT_EQ(bcDst[48], 0xFFu);
    }
}

TEST(TextureSubresourceHelpersTest, EdgeCasesFor3DNegativeAndOutOfRangeMips)
{
    TextureDesc tex3D(TextureDimension::Texture3D);
    tex3D.extent = IntPoint(8, 8);
    tex3D.depth = 4u;
    tex3D.format = PixelFormat::R8_UNorm;
    tex3D.numMips = 3u;
    EXPECT_EQ(getCombinedArrayIndex(tex3D, 0u, 0u), 0u);
    EXPECT_EQ(getSubresourceCount(tex3D, 0), 3u);
    EXPECT_EQ(getSubresourceCount(tex3D, -1), 3u);
    EXPECT_EQ(getSubresourceCount(tex3D, 5), 0u);

    TextureDesc texCubeArr(TextureDimension::TextureCubeArray);
    texCubeArr.extent = IntPoint(8, 8);
    texCubeArr.arraySize = 2u;
    texCubeArr.format = PixelFormat::R8_UNorm;
    texCubeArr.numMips = 2u;
    const TextureLockArgs lockArgs =
        TextureLockArgs::lockCubeFace(nullptr, CubeFace::NegY, 1u, 0u, ResourceLockMode::WriteOnly, false);
    EXPECT_EQ(getLockArrayIndex(texCubeArr, lockArgs), 9u);

    gp::UInt32 visitCount = 0u;
    enumerateSubresources(
        tex3D,
        3,
        [&visitCount](const TextureSubresourceIndex, const gp::UInt32) noexcept -> void
    {
        ++visitCount;
    }
    );
    EXPECT_EQ(visitCount, 0u);
}

TEST(TextureInitializerHelpersTest, DefaultAndDefaultLayoutTextureInitializers)
{
    const ScopedBypassDeleteCheck deleteGuard;
    command::CommandListImmediate cmdList;

    const TextureCreateDesc desc = TextureCreateDesc::create2D("LayoutTex", 4, 4, PixelFormat::R8G8B8A8_UNorm)
                                       .setNumMips(3u)
                                       .setFlags(TextureCreateFlags::ShaderResource)
                                       .determineInitialState();

    // DefaultTextureInitializer returns texture directly on finalize.
    {
        MockTexture* const rawTex = new MockTexture(desc);
        DefaultTextureInitializer init(cmdList, rawTex);
        EXPECT_EQ(init.getWritableData(), nullptr);
        EXPECT_EQ(init.getWritableSize(), 0u);
        EXPECT_EQ(init.finalize().get(), rawTex);
    }

    // DefaultLayoutTextureInitializer maps each mip to its tightly packed offset.
    {
        MockTexture* const rawTex = new MockTexture(desc);
        const gp::UInt64 totalSize = calculateTextureSize(desc);
        gp::Vector<gp::UInt8> staging;
        staging.resize(static_cast<gp::USize>(totalSize), 0u);

        bool finalizeCalled = false;
        DefaultLayoutTextureInitializer init(
            cmdList,
            rawTex,
            staging.data(),
            totalSize,
            [heldTex = TextureRef(rawTex), &finalizeCalled](command::CommandListBase&) mutable noexcept -> TextureRef
        {
            finalizeCalled = true;
            return std::move(heldTex);
        }
        );

        EXPECT_EQ(init.getWritableData(), staging.data());
        EXPECT_EQ(init.getWritableSize(), totalSize);

        // Mip 0: 4x4*4 = 64 bytes @0, stride 16; Mip 1: 2x2*4 = 16 bytes @64, stride 8; Mip 2: 1x1*4 = 4 bytes @80,
        // stride 4.
        const TextureSubresourceInitializer mip0 = init.getTexture2DSubresource(0u);
        const TextureSubresourceInitializer mip1 = init.getTexture2DSubresource(1u);
        const TextureSubresourceInitializer mip2 = init.getTexture2DSubresource(2u);

        EXPECT_EQ(mip0.writableData, staging.data());
        EXPECT_EQ(mip0.stride, 16u);
        EXPECT_EQ(mip0.size, 64u);

        EXPECT_EQ(mip1.writableData, staging.data() + 64u);
        EXPECT_EQ(mip1.stride, 8u);
        EXPECT_EQ(mip1.size, 16u);

        EXPECT_EQ(mip2.writableData, staging.data() + 80u);
        EXPECT_EQ(mip2.stride, 4u);
        EXPECT_EQ(mip2.size, 4u);

        const TextureRef finalized = init.finalize();
        EXPECT_TRUE(finalizeCalled);
        EXPECT_EQ(finalized.get(), rawTex);
    }
}

TEST(TextureInitializerHelpersTest, CreateLockTextureInitializerUploadsAllSubresourcesOnFinalize)
{
    const ScopedBypassDeleteCheck deleteGuard;
    MockLockTextureDynamicRHI mockRHI;
    const ScopedDynamicRHI rhiGuard(&mockRHI);

    command::CommandListImmediate cmdList;
    const TextureCreateDesc desc =
        TextureCreateDesc::create2DArray("LockArrayTex", 4, 4, 2u, PixelFormat::R8G8B8A8_UNorm)
            .setNumMips(2u)
            .setFlags(TextureCreateFlags::ShaderResource)
            .determineInitialState();
    MockTexture* const rawTex = new MockTexture(desc);

    {
        LockTextureInitializer init = createLockTextureInitializer(cmdList, rawTex, desc);
        ASSERT_NE(init.getWritableData(), nullptr);
        EXPECT_EQ(init.getWritableSize(), calculateTextureSize(desc));

        // Fill each (array, mip) subresource with a unique byte pattern.
        for (gp::UInt32 arr = 0u; arr < 2u; ++arr)
        {
            for (gp::UInt32 mip = 0u; mip < 2u; ++mip)
            {
                const TextureSubresourceInitializer sub = init.getTexture2DArraySubresource(arr, mip);
                const gp::UInt8 tag = static_cast<gp::UInt8>((arr * 16u) + mip + 1u);
                gp::memory::setMemory(sub.writableData, static_cast<gp::Int32>(tag), static_cast<gp::USize>(sub.size));
            }
        }

        const TextureRef finalized = init.finalize();
        EXPECT_EQ(finalized.get(), rawTex);
    }

    // 2 array slices * 2 mips = 4 lock/unlock pairs.
    EXPECT_EQ(mockRHI.lockCalls, 4u);
    EXPECT_EQ(mockRHI.unlockCalls, 4u);

    // Verify uploaded bytes in MockTexture::backing match each subresource's tag.
    for (gp::UInt32 arr = 0u; arr < 2u; ++arr)
    {
        for (gp::UInt32 mip = 0u; mip < 2u; ++mip)
        {
            gp::UInt64 stride = 0u;
            gp::UInt64 size = 0u;
            const gp::UInt64 offset = calculateSubresourceOffset(desc, 0u, arr, mip, stride, size);
            const gp::UInt8 expectedTag = static_cast<gp::UInt8>((arr * 16u) + mip + 1u);
            EXPECT_EQ(rawTex->backing[static_cast<gp::USize>(offset)], expectedTag);
            EXPECT_EQ(rawTex->backing[static_cast<gp::USize>(offset + size - 1u)], expectedTag);
        }
    }
}

TEST(TextureInitializerHelpersTest, FillTextureInitializerFromDataProviderHandlesDataNullZeroingAndDiscard)
{
    const ScopedBypassDeleteCheck deleteGuard;
    command::CommandListImmediate cmdList;

    const TextureCreateDesc desc = TextureCreateDesc::create2D("ProviderTex", 4, 4, PixelFormat::R8_UNorm)
                                       .setNumMips(3u)
                                       .setFlags(TextureCreateFlags::ShaderResource)
                                       .determineInitialState();
    MockTexture* const rawTex = new MockTexture(desc);

    const gp::UInt64 totalSize = calculateTextureSize(desc);
    gp::Vector<gp::UInt8> staging;
    staging.resize(static_cast<gp::USize>(totalSize), 0xDDu);

    DefaultLayoutTextureInitializer init(
        cmdList,
        rawTex,
        staging.data(),
        totalSize,
        [heldTex = TextureRef(rawTex)](command::CommandListBase&) mutable noexcept -> TextureRef
    {
        return std::move(heldTex);
    }
    );

    // Mip 1 (2x2 R8): supply padded source (rowPitch = 4 -> dest stride = 2).
    // Mip 2 (1x1 R8): supply nullptr -> must zero-fill destination byte.
    const gp::Array<gp::UInt8, 8u> mip1Source{ 11, 12, 99, 99, 21, 22, 99, 99 };
    gp::UInt32 discardCount = 0u;

    TextureDataProvider provider(
        [&mip1Source](const TextureSubresourceIndex index) noexcept -> TextureDataProvider::SubresourceData
    {
        if (index.mipIndex == 1)
        {
            return TextureDataProvider::SubresourceData{ mip1Source.data(), mip1Source.size(), 4u };
        }
        return TextureDataProvider::SubresourceData{ nullptr, 0u, 0u };
    },
        [&discardCount](const TextureSubresourceIndex) noexcept -> void
    {
        ++discardCount;
    }
    );

    // Populate starting from firstMipIndex = 1 (mip 0 remains untouched 0xDD).
    fillTextureInitializerFromDataProvider(init, desc, provider, 1);

    EXPECT_EQ(discardCount, 2u);
    EXPECT_EQ(staging[0], 0xDDu);

    // Mip 1 starts at offset 16 (after 4x4 mip 0).
    EXPECT_EQ(staging[16], 11u);
    EXPECT_EQ(staging[17], 12u);
    EXPECT_EQ(staging[18], 21u);
    EXPECT_EQ(staging[19], 22u);

    // Mip 2 starts at offset 20 and was zero-filled because requester returned nullptr.
    EXPECT_EQ(staging[20], 0u);

    static_cast<void>(init.finalize());
}

TEST(TextureInitializerHelpersTest, LockTextureCubeAutoFinalizeZeroSizeAndProviderVolumeAndUnbound)
{
    const ScopedBypassDeleteCheck deleteGuard;
    MockLockTextureDynamicRHI mockRHI;
    const ScopedDynamicRHI rhiGuard(&mockRHI);
    command::CommandListImmediate cmdList;

    // 1. Cubemap upload via createLockTextureInitializer with destructor auto-finalize (6 faces * 2 mips = 12 locks).
    {
        const TextureCreateDesc cubeDesc = TextureCreateDesc::createCube("LockCube", 4, PixelFormat::R8_UNorm)
                                               .setNumMips(2u)
                                               .setFlags(TextureCreateFlags::ShaderResource)
                                               .determineInitialState();
        MockTexture* const rawCube = new MockTexture(cubeDesc);

        {
            LockTextureInitializer init = createLockTextureInitializer(cmdList, rawCube, cubeDesc);
            ASSERT_NE(init.getWritableData(), nullptr);
            for (gp::UInt32 face = 0u; face < kCubeFaceCount; ++face)
            {
                for (gp::UInt32 mip = 0u; mip < 2u; ++mip)
                {
                    const TextureSubresourceInitializer sub = init.getTextureCubeSubresource(face, mip);
                    const gp::UInt8 tag = static_cast<gp::UInt8>((face * 10u) + mip + 1u);
                    gp::memory::setMemory(
                        sub.writableData, static_cast<gp::Int32>(tag), static_cast<gp::USize>(sub.size)
                    );
                }
            }
            // Intentionally do not call finalize(): destructor must auto-finalize and upload all 12 subresources.
        }

        EXPECT_EQ(mockRHI.lockCalls, 12u);
        EXPECT_EQ(mockRHI.unlockCalls, 12u);

        gp::UInt64 stride = 0u;
        gp::UInt64 size = 0u;
        const gp::UInt64 face5Mip1Offset = calculateSubresourceOffset(cubeDesc, 5u, 0u, 1u, stride, size);
        EXPECT_EQ(rawCube->backing[static_cast<gp::USize>(face5Mip1Offset)], 52u);
    }

    // 2. Zero-sized texture (PixelFormat::Unknown) allocates nullptr staging buffer safely.
    {
        const TextureCreateDesc zeroDesc =
            TextureCreateDesc::create2D("ZeroTex", 4, 4, PixelFormat::Unknown).determineInitialState();
        MockTexture* const rawZero = new MockTexture(zeroDesc);
        LockTextureInitializer init = createLockTextureInitializer(cmdList, rawZero, zeroDesc);
        EXPECT_EQ(init.getWritableData(), nullptr);
        EXPECT_EQ(init.getWritableSize(), 0u);
        static_cast<void>(init.finalize());
    }

    // 3. Direct BaseTextureInitializer (both constructors) + fillTextureInitializerFromDataProvider with 3D volume,
    //    default firstMipIndex = 0, unbound discarder, and unbound subresource callback (no UB on null dest).
    {
        const TextureCreateDesc volDesc = TextureCreateDesc::create3D("VolTex", 2, 2, 2, PixelFormat::R8_UNorm)
                                              .setNumMips(1u)
                                              .setFlags(TextureCreateFlags::ShaderResource)
                                              .determineInitialState();
        MockTexture* const rawVol = new MockTexture(volDesc);

        gp::Array<gp::UInt8, 8u> staging{};
        BaseTextureInitializer baseWithData(
            cmdList,
            rawVol,
            staging.data(),
            staging.size(),
            [heldTex = TextureRef(rawVol)](command::CommandListBase&) mutable noexcept -> TextureRef
        {
            return std::move(heldTex);
        },
            [&staging](const TextureSubresourceIndex) noexcept -> TextureSubresourceInitializer
        {
            return TextureSubresourceInitializer{ staging.data(), 2u, staging.size() };
        }
        );
        EXPECT_EQ(baseWithData.getWritableData(), staging.data());
        EXPECT_EQ(baseWithData.getWritableSize(), 8u);

        const gp::Array<gp::UInt8, 16u> volSrc{ 1, 2, 0, 0, 3, 4, 0, 0, 5, 6, 0, 0, 7, 8, 0, 0 };
        TextureDataProvider providerWithoutDiscarder(
            [&volSrc](const TextureSubresourceIndex) noexcept -> TextureDataProvider::SubresourceData
        {
            return TextureDataProvider::SubresourceData{ volSrc.data(), volSrc.size(), 4u };
        }
        );

        fillTextureInitializerFromDataProvider(baseWithData, volDesc, providerWithoutDiscarder);
        const gp::Array<gp::UInt8, 8u> expectedVol{ 1, 2, 3, 4, 5, 6, 7, 8 };
        EXPECT_EQ(staging, expectedVol);
        static_cast<void>(baseWithData.finalize());

        // Unbound subresource callback returns nullptr dest: fillTextureInitializerFromDataProvider must not UB.
        MockTexture* const rawNoSub = new MockTexture(volDesc);
        BaseTextureInitializer baseNoSub(
            cmdList,
            rawNoSub,
            [heldTex = TextureRef(rawNoSub)](command::CommandListBase&) mutable noexcept -> TextureRef
        {
            return std::move(heldTex);
        }
        );
        EXPECT_EQ(baseNoSub.getWritableData(), nullptr);
        EXPECT_EQ(baseNoSub.getWritableSize(), 0u);
        fillTextureInitializerFromDataProvider(baseNoSub, volDesc, providerWithoutDiscarder);
        static_cast<void>(baseNoSub.finalize());
    }
}

}   // namespace gp::rhi::resource::tests
