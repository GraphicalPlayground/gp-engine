// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanViewTranslation.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "translation/VulkanResourceEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

view::DescriptorType getVulkanSRVDescriptorType(const view::ViewDesc& viewDesc) noexcept
{
    if (viewDesc.isTexture())
    {
        // 64-bit integer images cannot be sampled; the shader compiler turns their SRVs into storage images.
        if (viewDesc.texture.srv.format == PixelFormat::R64_UInt)
        {
            return view::DescriptorType::TextureUAV;
        }

        return view::DescriptorType::TextureSRV;
    }

    switch (viewDesc.buffer.srv.bufferType)
    {
    case view::BufferViewType::Raw:
    case view::BufferViewType::Structured:
        // There is no read-only storage buffer descriptor: SRVs and UAVs share the storage buffer heap.
        return view::DescriptorType::BufferUAV;

    case view::BufferViewType::Typed:
        return view::DescriptorType::TypedBufferSRV;

    case view::BufferViewType::AccelerationStructure:
        return view::DescriptorType::AccelerationStructure;

    case view::BufferViewType::Unknown:
    default:
        GP_CHECKF(false, "Buffer SRV has no buffer view type.");
        return view::DescriptorType::Invalid;
    }
}

view::DescriptorType getVulkanUAVDescriptorType(const view::ViewDesc& viewDesc) noexcept
{
    if (viewDesc.isTexture())
    {
        return view::DescriptorType::TextureUAV;
    }

    switch (viewDesc.buffer.uav.bufferType)
    {
    case view::BufferViewType::Raw:
    case view::BufferViewType::Structured:
        return view::DescriptorType::BufferUAV;

    case view::BufferViewType::Typed:
        return view::DescriptorType::TypedBufferUAV;

    case view::BufferViewType::AccelerationStructure:
        return view::DescriptorType::AccelerationStructure;

    case view::BufferViewType::Unknown:
    default:
        GP_CHECKF(false, "Buffer UAV has no buffer view type.");
        return view::DescriptorType::Invalid;
    }
}

VkImageViewType getVulkanSRVImageViewType(
    const view::TextureViewDimension dimension, const VkImageViewType textureViewType
) noexcept
{
    switch (dimension)
    {
    case view::TextureViewDimension::Texture2D:
        return VK_IMAGE_VIEW_TYPE_2D;

    case view::TextureViewDimension::Texture2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    case view::TextureViewDimension::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;

    case view::TextureViewDimension::TextureCube:
        return VK_IMAGE_VIEW_TYPE_CUBE;

    case view::TextureViewDimension::TextureCubeArray:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

    case view::TextureViewDimension::Unknown:
        return textureViewType;

    default:
        break;
    }

    GP_CHECKF(false, "Unknown texture view dimension.");
    return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}

VkImageViewType getVulkanUAVImageViewType(
    const view::TextureViewDimension dimension, const VkImageViewType textureViewType
) noexcept
{
    switch (dimension)
    {
    case view::TextureViewDimension::Texture2D:
        return VK_IMAGE_VIEW_TYPE_2D;

    case view::TextureViewDimension::Texture2DArray:
    case view::TextureViewDimension::TextureCube:
    case view::TextureViewDimension::TextureCubeArray:
        // Storage images address cube faces as array layers.
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    case view::TextureViewDimension::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;

    case view::TextureViewDimension::Unknown:
        return textureViewType;

    default:
        break;
    }

    GP_CHECKF(false, "Unknown texture view dimension.");
    return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}

void getVulkanCubeAdjustedArrayRange(
    const view::TextureViewDimension dimension,
    const resource::Range16& arrayRange,
    const gp::UInt32 textureLayerCount,
    gp::UInt32& outFirstLayer,
    gp::UInt32& outNumLayers
) noexcept
{
    outFirstLayer = static_cast<gp::UInt32>(arrayRange.first);
    outNumLayers = static_cast<gp::UInt32>(arrayRange.num);

    if ((dimension == view::TextureViewDimension::TextureCube) ||
        (dimension == view::TextureViewDimension::TextureCubeArray))
    {
        outFirstLayer *= resource::kCubeFaceCount;
        outNumLayers *= resource::kCubeFaceCount;
        GP_CHECKF(
            (outFirstLayer + outNumLayers) <= textureLayerCount,
            "View extends beyond the layer count of the cube texture."
        );
    }
    static_cast<void>(textureLayerCount);
}

VulkanTextureViewDesc translateVulkanTextureViewDesc(
    const VulkanTexture& texture, const view::ViewDesc::TextureSRV::ViewInfo& viewInfo
) noexcept
{
    VulkanTextureViewDesc viewDesc{};
    getVulkanCubeAdjustedArrayRange(
        viewInfo.dimension,
        viewInfo.arrayRange,
        getVulkanTextureLayerCount(texture.getDesc()),
        viewDesc.arraySliceIndex,
        viewDesc.numArraySlices
    );

    viewDesc.viewType =
        getVulkanSRVImageViewType(viewInfo.dimension, textureDimensionToVkImageViewType(texture.getDesc().dimension));
    viewDesc.aspectFlags = texture.getPartialAspectMask();
    viewDesc.pixelFormat = viewInfo.format;
    viewDesc.format = pixelFormatToVkFormat(viewInfo.format, viewInfo.isSRGB != 0u);
    viewDesc.firstMip = static_cast<gp::UInt32>(viewInfo.mipRange.first);
    viewDesc.numMips = static_cast<gp::UInt32>(viewInfo.mipRange.num);
    viewDesc.useIdentitySwizzle = false;

    // Shader reads never need the storage usage, and keeping it would make sRGB or other non-storage formats invalid.
    viewDesc.imageUsageFlags =
        texture.getImageUsageFlags() & ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT);
    return viewDesc;
}

VulkanTextureViewDesc translateVulkanTextureViewDesc(
    const VulkanTexture& texture, const view::ViewDesc::TextureUAV::ViewInfo& viewInfo
) noexcept
{
    VulkanTextureViewDesc viewDesc{};
    getVulkanCubeAdjustedArrayRange(
        viewInfo.dimension,
        viewInfo.arrayRange,
        getVulkanTextureLayerCount(texture.getDesc()),
        viewDesc.arraySliceIndex,
        viewDesc.numArraySlices
    );

    viewDesc.viewType =
        getVulkanUAVImageViewType(viewInfo.dimension, textureDimensionToVkImageViewType(texture.getDesc().dimension));
    viewDesc.aspectFlags = texture.getPartialAspectMask();
    viewDesc.pixelFormat = viewInfo.format;
    viewDesc.format = pixelFormatToVkFormat(viewInfo.format, false);
    viewDesc.firstMip = static_cast<gp::UInt32>(viewInfo.mipLevel);
    viewDesc.numMips = 1u;
    viewDesc.useIdentitySwizzle = true;
    return viewDesc;
}

VkBufferViewCreateInfo translateVulkanTypedBufferViewDesc(
    const VulkanDevice& device,
    const VulkanBuffer& buffer,
    const PixelFormat format,
    const gp::UInt32 offset,
    const gp::UInt32 size
) noexcept
{
    GP_CHECKF(format != PixelFormat::Unknown, "Typed buffer views need a texel format.");
    const VkFormat vulkanFormat = pixelFormatToVkFormat(format);
    GP_CHECKF(vulkanFormat != VK_FORMAT_UNDEFINED, "Pixel format has no Vulkan texel buffer format.");

    VkBufferViewCreateInfo createInfo =
        zeroVulkanStruct<VkBufferViewCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
    createInfo.buffer = buffer.getHandle();
    createInfo.offset = static_cast<VkDeviceSize>(buffer.getOffset()) + static_cast<VkDeviceSize>(offset);
    createInfo.format = vulkanFormat;

    // The view size is not checked against the texel size: buffers are sometimes reused with another format while
    // views built for the old format still exist, and those views are rebuilt with the new stride on the next lock.
    const VkPhysicalDeviceLimits& limits = device.getLimits();
    const gp::UInt64 texelSize = static_cast<gp::UInt64>(getNumBitsPerPixel(vulkanFormat) / 8u);
    const gp::UInt64 maxSize = static_cast<gp::UInt64>(limits.maxTexelBufferElements) * texelSize;
    createInfo.range = gp::math::min<gp::UInt64>(static_cast<gp::UInt64>(size), maxSize);

    constexpr VkBufferUsageFlags kTexelBufferUsage =
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    GP_CHECKF(
        (buffer.getBufferUsageFlags() & kTexelBufferUsage) != 0u, "Typed buffer views need a texel buffer usage."
    );
    GP_CHECKF(
        (limits.minTexelBufferOffsetAlignment == 0u) ||
            ((static_cast<VkDeviceSize>(offset) % limits.minTexelBufferOffsetAlignment) == 0u),
        "Typed buffer view offset must honor minTexelBufferOffsetAlignment."
    );
    return createInfo;
}

gp::UInt32 getVulkanStructuredBufferViewSize(const VulkanBuffer& buffer, const gp::UInt32 size) noexcept
{
    // Volatile buffers get a fresh allocation on every lock, sized for that lock rather than for the whole buffer.
    if ((buffer.getDesc().usage & resource::BufferUsageFlags::Volatile) != resource::BufferUsageFlags::None)
    {
        return gp::math::min<gp::UInt32>(size, buffer.getCurrentSize());
    }

    return size;
}

}   // namespace gp::rhi::vulkan
