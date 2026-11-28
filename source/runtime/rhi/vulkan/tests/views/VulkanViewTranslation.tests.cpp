// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanViewTranslation.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "RHIPixelFormat.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"
#include "views/VulkanViewEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Builds a buffer SRV description of `type`.
[[nodiscard]] view::ViewDesc
    makeBufferSRV(const view::BufferViewType type, const PixelFormat format = PixelFormat::Unknown) noexcept
{
    return view::ViewDesc::createBufferSRV().setType(type).setFormat(format).asViewDesc();
}

/// @brief Builds a buffer UAV description of `type`.
[[nodiscard]] view::ViewDesc
    makeBufferUAV(const view::BufferViewType type, const PixelFormat format = PixelFormat::Unknown) noexcept
{
    return view::ViewDesc::createBufferUAV().setType(type).setFormat(format).asViewDesc();
}

}   // namespace

static_assert(static_cast<gp::UInt8>(VulkanViewKind::Null) == 0u, "Null must be the zero kind.");
static_assert(sizeof(VulkanViewKind) == 1u, "VulkanViewKind must stay one byte.");

TEST(VulkanViewDescriptorTypeTest, TextureSRVIsSampledUnless64BitInteger)
{
    const view::ViewDesc sampled =
        view::ViewDesc::createTextureSRV().setFormat(PixelFormat::R8G8B8A8_UNorm).asViewDesc();
    const view::ViewDesc integer64 = view::ViewDesc::createTextureSRV().setFormat(PixelFormat::R64_UInt).asViewDesc();

    EXPECT_EQ(getVulkanSRVDescriptorType(sampled), view::DescriptorType::TextureSRV);
    EXPECT_EQ(getVulkanSRVDescriptorType(integer64), view::DescriptorType::TextureUAV);
}

TEST(VulkanViewDescriptorTypeTest, BufferSRVMapsEveryViewType)
{
    EXPECT_EQ(
        getVulkanSRVDescriptorType(makeBufferSRV(view::BufferViewType::Typed, PixelFormat::R32_UInt)),
        view::DescriptorType::TypedBufferSRV
    );
    EXPECT_EQ(getVulkanSRVDescriptorType(makeBufferSRV(view::BufferViewType::Raw)), view::DescriptorType::BufferUAV);
    EXPECT_EQ(
        getVulkanSRVDescriptorType(makeBufferSRV(view::BufferViewType::Structured)), view::DescriptorType::BufferUAV
    );
    EXPECT_EQ(
        getVulkanSRVDescriptorType(makeBufferSRV(view::BufferViewType::AccelerationStructure)),
        view::DescriptorType::AccelerationStructure
    );
}

TEST(VulkanViewDescriptorTypeTest, UAVMapsEveryViewType)
{
    const view::ViewDesc texture = view::ViewDesc::createTextureUAV().setFormat(PixelFormat::R32_Float).asViewDesc();

    EXPECT_EQ(getVulkanUAVDescriptorType(texture), view::DescriptorType::TextureUAV);
    EXPECT_EQ(
        getVulkanUAVDescriptorType(makeBufferUAV(view::BufferViewType::Typed, PixelFormat::R32_UInt)),
        view::DescriptorType::TypedBufferUAV
    );
    EXPECT_EQ(getVulkanUAVDescriptorType(makeBufferUAV(view::BufferViewType::Raw)), view::DescriptorType::BufferUAV);
    EXPECT_EQ(
        getVulkanUAVDescriptorType(makeBufferUAV(view::BufferViewType::Structured)), view::DescriptorType::BufferUAV
    );
}

TEST(VulkanViewImageViewTypeTest, SRVKeepsCubeViewTypes)
{
    EXPECT_EQ(
        getVulkanSRVImageViewType(view::TextureViewDimension::Texture2D, VK_IMAGE_VIEW_TYPE_CUBE), VK_IMAGE_VIEW_TYPE_2D
    );
    EXPECT_EQ(
        getVulkanSRVImageViewType(view::TextureViewDimension::Texture2DArray, VK_IMAGE_VIEW_TYPE_2D),
        VK_IMAGE_VIEW_TYPE_2D_ARRAY
    );
    EXPECT_EQ(
        getVulkanSRVImageViewType(view::TextureViewDimension::Texture3D, VK_IMAGE_VIEW_TYPE_3D), VK_IMAGE_VIEW_TYPE_3D
    );
    EXPECT_EQ(
        getVulkanSRVImageViewType(view::TextureViewDimension::TextureCube, VK_IMAGE_VIEW_TYPE_CUBE),
        VK_IMAGE_VIEW_TYPE_CUBE
    );
    EXPECT_EQ(
        getVulkanSRVImageViewType(view::TextureViewDimension::TextureCubeArray, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY),
        VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
    );
}

TEST(VulkanViewImageViewTypeTest, UAVAddressesCubeFacesAsArrayLayers)
{
    EXPECT_EQ(
        getVulkanUAVImageViewType(view::TextureViewDimension::Texture2D, VK_IMAGE_VIEW_TYPE_2D), VK_IMAGE_VIEW_TYPE_2D
    );
    EXPECT_EQ(
        getVulkanUAVImageViewType(view::TextureViewDimension::Texture2DArray, VK_IMAGE_VIEW_TYPE_2D_ARRAY),
        VK_IMAGE_VIEW_TYPE_2D_ARRAY
    );
    EXPECT_EQ(
        getVulkanUAVImageViewType(view::TextureViewDimension::TextureCube, VK_IMAGE_VIEW_TYPE_CUBE),
        VK_IMAGE_VIEW_TYPE_2D_ARRAY
    );
    EXPECT_EQ(
        getVulkanUAVImageViewType(view::TextureViewDimension::TextureCubeArray, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY),
        VK_IMAGE_VIEW_TYPE_2D_ARRAY
    );
    EXPECT_EQ(
        getVulkanUAVImageViewType(view::TextureViewDimension::Texture3D, VK_IMAGE_VIEW_TYPE_3D), VK_IMAGE_VIEW_TYPE_3D
    );
}

TEST(VulkanViewImageViewTypeTest, UnknownDimensionFallsBackToTextureViewType)
{
    EXPECT_EQ(
        getVulkanSRVImageViewType(view::TextureViewDimension::Unknown, VK_IMAGE_VIEW_TYPE_CUBE), VK_IMAGE_VIEW_TYPE_CUBE
    );
    EXPECT_EQ(
        getVulkanUAVImageViewType(view::TextureViewDimension::Unknown, VK_IMAGE_VIEW_TYPE_3D), VK_IMAGE_VIEW_TYPE_3D
    );
}

TEST(VulkanViewArrayRangeTest, NonCubeRangesAreUnchanged)
{
    gp::UInt32 firstLayer = 99u;
    gp::UInt32 numLayers = 99u;
    getVulkanCubeAdjustedArrayRange(
        view::TextureViewDimension::Texture2DArray, resource::Range16(1u, 2u), 4u, firstLayer, numLayers
    );

    EXPECT_EQ(firstLayer, 1u);
    EXPECT_EQ(numLayers, 2u);
}

TEST(VulkanViewArrayRangeTest, CubeRangesCountFaces)
{
    gp::UInt32 firstLayer = 0u;
    gp::UInt32 numLayers = 0u;
    getVulkanCubeAdjustedArrayRange(
        view::TextureViewDimension::TextureCube, resource::Range16(0u, 1u), 6u, firstLayer, numLayers
    );
    EXPECT_EQ(firstLayer, 0u);
    EXPECT_EQ(numLayers, 6u);

    getVulkanCubeAdjustedArrayRange(
        view::TextureViewDimension::TextureCubeArray, resource::Range16(1u, 1u), 12u, firstLayer, numLayers
    );
    EXPECT_EQ(firstLayer, 6u);
    EXPECT_EQ(numLayers, 6u);
}

}   // namespace gp::rhi::vulkan::tests
