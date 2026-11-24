// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "RHIPixelFormat.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::tests
{

static_assert(
    hasPixelFormatCapabilities(PixelFormatCapabilities::AllFlags, PixelFormatCapabilities::None),
    "Any capability mask must satisfy None."
);
static_assert(
    hasPixelFormatCapabilities(
        PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::RenderTarget,
        PixelFormatCapabilities::RenderTarget
    ),
    "Subset capability check failed."
);
static_assert(
    !hasPixelFormatCapabilities(
        PixelFormatCapabilities::Texture2D, PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::RenderTarget
    ),
    "Missing required capability must return false."
);
static_assert(
    toString(PixelFormat::B8G8R8A8_UNorm) == gp::StringView("B8G8R8A8_UNorm"), "Re-exported gp::rhi::toString failed."
);

TEST(RHIPixelFormatTest, HasCapabilitiesChecksAllRequiredBits)
{
    const PixelFormatCapabilities swapchainCaps =
        PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::RenderTarget |
        PixelFormatCapabilities::TextureSample | PixelFormatCapabilities::TextureFilterable;

    EXPECT_TRUE(hasPixelFormatCapabilities(swapchainCaps, PixelFormatCapabilities::None));
    EXPECT_TRUE(hasPixelFormatCapabilities(swapchainCaps, PixelFormatCapabilities::Texture2D));
    EXPECT_TRUE(hasPixelFormatCapabilities(
        swapchainCaps, PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::RenderTarget
    ));
    EXPECT_TRUE(hasPixelFormatCapabilities(swapchainCaps, swapchainCaps));

    EXPECT_FALSE(hasPixelFormatCapabilities(swapchainCaps, PixelFormatCapabilities::DepthStencil));
    EXPECT_FALSE(
        hasPixelFormatCapabilities(swapchainCaps, PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::UAV)
    );
    EXPECT_FALSE(hasPixelFormatCapabilities(PixelFormatCapabilities::None, PixelFormatCapabilities::Texture2D));
}

TEST(RHIPixelFormatTest, CompositeMasksIncludeExpectedFlags)
{
    EXPECT_TRUE(hasPixelFormatCapabilities(
        PixelFormatCapabilities::AnyTexture,
        PixelFormatCapabilities::Texture1D | PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::Texture3D |
            PixelFormatCapabilities::TextureCube
    ));
    EXPECT_TRUE(hasPixelFormatCapabilities(
        PixelFormatCapabilities::AllTextureFlags,
        PixelFormatCapabilities::AnyTexture | PixelFormatCapabilities::RenderTarget |
            PixelFormatCapabilities::DepthStencil | PixelFormatCapabilities::TextureMipmaps |
            PixelFormatCapabilities::TextureLoad | PixelFormatCapabilities::TextureSample |
            PixelFormatCapabilities::TextureGather | PixelFormatCapabilities::TextureAtomics |
            PixelFormatCapabilities::TextureBlendable | PixelFormatCapabilities::TextureStore |
            PixelFormatCapabilities::TextureFilterable
    ));
    EXPECT_TRUE(hasPixelFormatCapabilities(
        PixelFormatCapabilities::AllBufferFlags,
        PixelFormatCapabilities::Buffer | PixelFormatCapabilities::VertexBuffer | PixelFormatCapabilities::IndexBuffer |
            PixelFormatCapabilities::BufferLoad | PixelFormatCapabilities::BufferStore |
            PixelFormatCapabilities::BufferAtomics
    ));
    EXPECT_TRUE(hasPixelFormatCapabilities(
        PixelFormatCapabilities::AllUAVFlags,
        PixelFormatCapabilities::UAV | PixelFormatCapabilities::TypedUAVLoad | PixelFormatCapabilities::TypedUAVStore
    ));
    EXPECT_TRUE(hasPixelFormatCapabilities(
        PixelFormatCapabilities::AllFlags,
        PixelFormatCapabilities::AllTextureFlags | PixelFormatCapabilities::AllBufferFlags |
            PixelFormatCapabilities::AllUAVFlags | PixelFormatCapabilities::LossyCompressible
    ));
}

TEST(RHIPixelFormatTest, CorePixelFormatReExportsWorkInRHINamespace)
{
    const PixelFormatInfo& info = getPixelFormatInfo(PixelFormat::R8G8B8A8_UNorm);

    EXPECT_EQ(info.format, PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(info.blockBytes, 4u);
    EXPECT_EQ(pixelFormatFromString("R8G8B8A8_UNorm"), PixelFormat::R8G8B8A8_UNorm);
    EXPECT_TRUE(supportsSRGB(PixelFormat::B8G8R8A8_UNorm));
}

}   // namespace gp::rhi::tests
