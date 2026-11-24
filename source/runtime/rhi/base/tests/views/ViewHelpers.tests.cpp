// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/ViewHelpers.hpp"
#include "resources/TextureDesc.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::view::tests
{

TEST(ViewHelpersTest, AdjustViewInfoDimensionForNarrowingPromotes2DOnArrayAndCube)
{
    ViewDesc::Texture::ViewInfo viewInfo{};
    viewInfo.dimension = TextureViewDimension::Texture2D;
    viewInfo.arrayRange = resource::Range16(0u, 1u);

    // Plain 2D texture keeps Texture2D.
    {
        resource::TextureDesc tex2D(resource::TextureDimension::Texture2D);
        tex2D.arraySize = 1u;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, tex2D), TextureViewDimension::Texture2D);
    }

    // 2D array texture promotes Texture2D view to Texture2DArray.
    {
        resource::TextureDesc tex2DArray(resource::TextureDimension::Texture2DArray);
        tex2DArray.arraySize = 4u;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, tex2DArray), TextureViewDimension::Texture2DArray);
    }

    // Cubemap texture promotes Texture2D view to Texture2DArray (also covers textureDesc.arraySize == 1 disjunct when
    // arrayRange.num > 1).
    {
        resource::TextureDesc texCube(resource::TextureDimension::TextureCube);
        texCube.arraySize = 1u;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, texCube), TextureViewDimension::Texture2DArray);

        ViewDesc::Texture::ViewInfo multiSliceView = viewInfo;
        multiSliceView.arrayRange = resource::Range16(0u, 6u);
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(multiSliceView, texCube), TextureViewDimension::Texture2DArray);
    }

    // 3D texture with Texture2D view stays Texture2D.
    {
        resource::TextureDesc tex3D(resource::TextureDimension::Texture3D);
        tex3D.arraySize = 1u;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, tex3D), TextureViewDimension::Texture2D);
    }

    // Cubemap array texture promotes Texture2D view to Texture2DArray.
    {
        resource::TextureDesc texCubeArray(resource::TextureDimension::TextureCubeArray);
        texCubeArray.arraySize = 2u;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, texCubeArray), TextureViewDimension::Texture2DArray);
    }

    // Non-2D view dimensions (including Unknown) pass through untouched.
    {
        resource::TextureDesc texCubeArray(resource::TextureDimension::TextureCubeArray);
        texCubeArray.arraySize = 2u;

        viewInfo.dimension = TextureViewDimension::Unknown;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, texCubeArray), TextureViewDimension::Unknown);

        viewInfo.dimension = TextureViewDimension::Texture2DArray;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, texCubeArray), TextureViewDimension::Texture2DArray);

        viewInfo.dimension = TextureViewDimension::TextureCube;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, texCubeArray), TextureViewDimension::TextureCube);

        viewInfo.dimension = TextureViewDimension::TextureCubeArray;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, texCubeArray), TextureViewDimension::TextureCubeArray);

        resource::TextureDesc tex3D(resource::TextureDimension::Texture3D);
        viewInfo.dimension = TextureViewDimension::Texture3D;
        EXPECT_EQ(adjustViewInfoDimensionForNarrowing(viewInfo, tex3D), TextureViewDimension::Texture3D);
    }
}

}   // namespace gp::rhi::view::tests
