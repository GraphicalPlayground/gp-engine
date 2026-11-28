// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureUsage.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanDeviceCapabilities.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureUtils.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "translation/VulkanResourceEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

using resource::TextureCreateFlags;

/// @brief Reports whether `access` contains any bit of `mask`.
/// @param[in] access Access state.
/// @param[in] mask Bits to test.
/// @return True when at least one bit matches.
[[nodiscard]] constexpr bool hasAnyAccess(const sync::ResourceAccess access, const sync::ResourceAccess mask) noexcept
{
    return (access & mask) != sync::ResourceAccess::Unknown;
}

/// @brief Appends `format` to `outInfo.formatsUsed` when it is defined and not already listed.
/// @param[in,out] outInfo Create info being filled.
/// @param[in] format Format to add.
void addUniqueViewFormat(VulkanImageCreateInfo& outInfo, const VkFormat format) noexcept
{
    if (format == VK_FORMAT_UNDEFINED)
    {
        return;
    }
    for (const VkFormat existing: outInfo.formatsUsed)
    {
        if (existing == format)
        {
            return;
        }
    }
    if (outInfo.formatsUsed.size() < kVulkanMaxImageViewFormats)
    {
        outInfo.formatsUsed.emplaceBack(format);
    }
}

/// @brief Reports whether the texture declares a UAV view format different from its base format.
/// @param[in] desc Texture description.
/// @return True for a separate UAV format.
[[nodiscard]] bool hasSeparateUAVFormat(const resource::TextureDesc& desc) noexcept
{
    return (desc.uavFormat != PixelFormat::Unknown) && (desc.uavFormat != desc.format);
}

}   // namespace

VkImageUsageFlags getVulkanImageUsageFlags(const TextureCreateFlags flags, const bool supportsMemoryless) noexcept
{
    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (hasAnyTextureFlags(flags, TextureCreateFlags::Presentable))
    {
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else if (
        hasAnyTextureFlags(flags, TextureCreateFlags::RenderTargetable | TextureCreateFlags::DepthStencilTargetable)
    )
    {
        if (hasAnyTextureFlags(flags, TextureCreateFlags::InputAttachmentRead))
        {
            usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
        usage |= hasAnyTextureFlags(flags, TextureCreateFlags::RenderTargetable)
                     ? static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                     : static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        // Tile memory is never resolved to DRAM, so the image can neither be copied nor sampled.
        if (hasAnyTextureFlags(flags, TextureCreateFlags::Memoryless) && supportsMemoryless)
        {
            usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            usage &= ~static_cast<VkImageUsageFlags>(
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            );
        }
    }
    else if (hasAnyTextureFlags(flags, TextureCreateFlags::DepthStencilResolveTarget))
    {
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    else if (hasAnyTextureFlags(flags, TextureCreateFlags::ResolveTargetable))
    {
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (hasAnyTextureFlags(flags, TextureCreateFlags::Foveation))
    {
        usage |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
    }

    if (hasAnyTextureFlags(flags, TextureCreateFlags::UAV))
    {
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    return usage;
}

VkImageUsageFlags filterVulkanImageUsageByFormatFeatures(
    VkImageUsageFlags usage,
    const VkFormatFeatureFlags formatFeatures,
    const bool hasUAVFormat,
    bool& outNeedsExtendedUsage
) noexcept
{
    outNeedsExtendedUsage = false;

    if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0u && (formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0u)
    {
        usage &= ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0u && (formatFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0u)
    {
        // A separate UAV view format may support storage even though the base format does not.
        if (hasUAVFormat)
        {
            outNeedsExtendedUsage = true;
        }
        else
        {
            usage &= ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT);
        }
    }

    if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u &&
        (formatFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0u)
    {
        usage &= ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    }

    if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u &&
        (formatFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0u)
    {
        usage &= ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    if ((usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u && (formatFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0u)
    {
        usage &= ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }

    if ((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u && (formatFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0u)
    {
        usage &= ~static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }

    return usage;
}

VkImageLayout getVulkanInitialImageLayout(
    const sync::ResourceAccess access,
    const bool isDepthStencilTarget,
    const bool supportsSampling,
    const bool supportsRenderingLocalRead
) noexcept
{
    using sync::ResourceAccess;

    if (hasAnyAccess(access, ResourceAccess::RTV))
    {
        return supportsRenderingLocalRead ? VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ
                                          : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else if (hasAnyAccess(access, ResourceAccess::Present))
    {
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    else if (hasAnyAccess(access, ResourceAccess::DSVWrite))
    {
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    }
    else if (hasAnyAccess(access, ResourceAccess::DSVRead))
    {
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    }
    else if (hasAnyAccess(access, ResourceAccess::SRVMask))
    {
        if (isDepthStencilTarget)
        {
            return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        }
        return supportsSampling ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    }
    else if (hasAnyAccess(access, ResourceAccess::UAVMask))
    {
        return VK_IMAGE_LAYOUT_GENERAL;
    }
    else if (hasAnyAccess(access, ResourceAccess::CopySrc))
    {
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    else if (hasAnyAccess(access, ResourceAccess::CopyDest))
    {
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    else if (hasAnyAccess(access, ResourceAccess::ShadingRateSource))
    {
        return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
    }

    // Unknown, Discard and CPURead need no layout: the first barrier starts from UNDEFINED.
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

gp::UInt32 getVulkanTextureLayerCount(const resource::TextureDesc& desc) noexcept
{
    if (desc.isTexture3D())
    {
        return 1u;
    }
    const gp::UInt32 arraySize = (desc.arraySize > 0u) ? static_cast<gp::UInt32>(desc.arraySize) : 1u;
    return desc.isTextureCube() ? (arraySize * resource::kCubeFaceCount) : arraySize;
}

bool isVulkanTextureFormatMutable(const resource::TextureDesc& desc) noexcept
{
    return hasAnyTextureFlags(desc.flags, TextureCreateFlags::SRGB) || (desc.format == PixelFormat::R64_UInt) ||
           hasSeparateUAVFormat(desc) || !desc.aliasableFormats.isEmpty();
}

VkFormat resolveVulkanTextureFormat(const VulkanDevice& device, const PixelFormat format, const bool isSRGB) noexcept
{
    const gp::USize index = static_cast<gp::USize>(format);
    if (index >= kPixelFormatCount)
    {
        return VK_FORMAT_UNDEFINED;
    }

    const VulkanFormatSupportTable& table = device.getFormatSupportTable();
    const VkFormat deviceFormat = isSRGB ? table.srgbFormats[index] : table.platformFormats[index];
    return (deviceFormat != VK_FORMAT_UNDEFINED) ? deviceFormat : pixelFormatToVkFormat(format, isSRGB);
}

bool generateVulkanImageCreateInfo(
    const VulkanDevice& device,
    const resource::TextureDesc& desc,
    bool forceLinearTexture,
    const bool supportsMemoryless,
    VulkanImageCreateInfo& outInfo,
    VkFormat& outViewFormat,
    VkFormat& outStorageFormat
) noexcept
{
    const VkPhysicalDeviceLimits& limits = device.getLimits();
    const gp::UInt32 sizeX = static_cast<gp::UInt32>(desc.extent.x);
    const gp::UInt32 sizeY = static_cast<gp::UInt32>(desc.extent.y);
    const gp::UInt32 sizeZ = desc.isTexture3D() ? static_cast<gp::UInt32>(desc.depth) : 1u;
    const gp::UInt32 arraySize = (desc.arraySize > 0u) ? static_cast<gp::UInt32>(desc.arraySize) : 1u;

    if (hasAnyTextureFlags(desc.flags, TextureCreateFlags::CPUReadback))
    {
        forceLinearTexture = true;
    }

    VkImageCreateInfo& imageCreateInfo = outInfo.imageCreateInfo;
    imageCreateInfo = zeroVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    outInfo.formatListInfo =
        zeroVulkanStruct<VkImageFormatListCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);
    outInfo.formatsUsed.clear();

    imageCreateInfo.imageType = textureDimensionToVkImageType(desc.dimension);
    switch (desc.dimension)
    {
    case resource::TextureDimension::Texture2D:
    case resource::TextureDimension::Texture2DArray:
        if (sizeX > limits.maxImageDimension2D || sizeY > limits.maxImageDimension2D ||
            arraySize > limits.maxImageArrayLayers)
        {
            GP_LOG_ERROR(
                LogVulkanRHI, "Texture of {}x{}x{} exceeds the 2D image limits of the device.", sizeX, sizeY, arraySize
            );
            return false;
        }
        break;
    case resource::TextureDimension::TextureCube:
    case resource::TextureDimension::TextureCubeArray:
        if (sizeX != sizeY || sizeX > limits.maxImageDimensionCube ||
            (arraySize * resource::kCubeFaceCount) > limits.maxImageArrayLayers)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Cube texture of {}x{} (x{}) is not square or exceeds the device limits.",
                sizeX,
                sizeY,
                arraySize
            );
            return false;
        }
        break;
    case resource::TextureDimension::Texture3D:
        if (sizeX > limits.maxImageDimension3D || sizeY > limits.maxImageDimension3D ||
            sizeZ > limits.maxImageDimension3D)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Volume texture of {}x{}x{} exceeds the 3D image limits of the device.",
                sizeX,
                sizeY,
                sizeZ
            );
            return false;
        }
        break;
    default:
        GP_LOG_ERROR(LogVulkanRHI, "Unsupported texture dimension {}.", static_cast<gp::UInt32>(desc.dimension));
        return false;
    }

    const bool isSRGB = hasAnyTextureFlags(desc.flags, TextureCreateFlags::SRGB);
    const VkFormat srgbFormat = resolveVulkanTextureFormat(device, desc.format, isSRGB);
    const VkFormat linearFormat = resolveVulkanTextureFormat(device, desc.format, false);

    // Storage views cannot use sRGB formats, so a UAV texture is created linear and sampled through an sRGB view.
    imageCreateInfo.format = hasAnyTextureFlags(desc.flags, TextureCreateFlags::UAV) ? linearFormat : srgbFormat;
    outViewFormat = srgbFormat;
    outStorageFormat = linearFormat;
    if (imageCreateInfo.format == VK_FORMAT_UNDEFINED)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Pixel format {} has no Vulkan equivalent.", static_cast<gp::UInt32>(desc.format));
        return false;
    }

    imageCreateInfo.extent.width = sizeX;
    imageCreateInfo.extent.height = sizeY;
    imageCreateInfo.extent.depth = sizeZ;
    imageCreateInfo.mipLevels = (desc.numMips > 0u) ? static_cast<gp::UInt32>(desc.numMips) : 1u;
    imageCreateInfo.arrayLayers = getVulkanTextureLayerCount(desc);

    if (desc.isTextureCube())
    {
        imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    const bool hasUAVFormat = hasSeparateUAVFormat(desc);
    const bool isMutable = isVulkanTextureFormatMutable(desc);
    if (isMutable && device.getOptionalExtensions().hasKHRImageFormatList)
    {
        addUniqueViewFormat(outInfo, imageCreateInfo.format);
        if (isSRGB)
        {
            addUniqueViewFormat(outInfo, srgbFormat);
            addUniqueViewFormat(outInfo, linearFormat);
        }
        if (desc.format == PixelFormat::R64_UInt)
        {
            // 64-bit atomics are emulated with two 32-bit channels on devices without native support.
            addUniqueViewFormat(outInfo, linearFormat);
            addUniqueViewFormat(outInfo, VK_FORMAT_R32G32_UINT);
        }
        if (hasUAVFormat)
        {
            addUniqueViewFormat(outInfo, linearFormat);
            addUniqueViewFormat(outInfo, resolveVulkanTextureFormat(device, desc.uavFormat, false));
        }
        for (const PixelFormat aliasFormat: desc.aliasableFormats)
        {
            addUniqueViewFormat(outInfo, resolveVulkanTextureFormat(device, aliasFormat, false));
        }

        outInfo.formatListInfo.viewFormatCount = static_cast<gp::UInt32>(outInfo.formatsUsed.size());
        outInfo.formatListInfo.pViewFormats = outInfo.formatsUsed.data();
        imageCreateInfo.pNext = &outInfo.formatListInfo;
    }

    // Views are always allowed to reinterpret the format; the format list above only narrows it for the driver.
    imageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    if (isBlockCompressed(desc.format) && hasUAVFormat && !isBlockCompressed(desc.uavFormat))
    {
        imageCreateInfo.flags |= VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
    }

    // Volume textures can be bound as 2D array render targets, one slice per layer.
    if (desc.isTexture3D() && !hasAnyTextureFlags(desc.flags, TextureCreateFlags::ReservedResource))
    {
        imageCreateInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }

    imageCreateInfo.tiling = forceLinearTexture ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
    if (hasAnyTextureFlags(
            desc.flags,
            TextureCreateFlags::RenderTargetable | TextureCreateFlags::DepthStencilTargetable |
                TextureCreateFlags::ResolveTargetable | TextureCreateFlags::DepthStencilResolveTarget
        ))
    {
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }

    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples = (imageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR)
                                  ? VK_SAMPLE_COUNT_1_BIT
                                  : sampleCountToVkSampleCountFlagBits(gp::math::max<gp::UInt32>(desc.numSamples, 1u));

    const VkImageUsageFlags usage = getVulkanImageUsageFlags(desc.flags, supportsMemoryless);

    const VkFormatProperties formatProperties = device.getFormatProperties(imageCreateInfo.format);
    const VkFormatFeatureFlags formatFeatures = (imageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR)
                                                    ? formatProperties.linearTilingFeatures
                                                    : formatProperties.optimalTilingFeatures;
    bool needsExtendedUsage = false;
    imageCreateInfo.usage =
        filterVulkanImageUsageByFormatFeatures(usage, formatFeatures, hasUAVFormat, needsExtendedUsage);
    if (needsExtendedUsage)
    {
        imageCreateInfo.flags |= VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
    }

    return true;
}

gp::UInt64 calculateVulkanSubresourceOffset(
    const resource::TextureDesc& desc,
    const gp::UInt32 mipIndex,
    const gp::UInt32 layerIndex,
    gp::UInt64& outRowStride,
    gp::UInt64& outSubresourceSize
) noexcept
{
    outRowStride = 0u;
    outSubresourceSize = 0u;

    const gp::UInt32 layerCount = getVulkanTextureLayerCount(desc);
    if (!isValidPixelFormat(desc.format) || mipIndex >= static_cast<gp::UInt32>(desc.numMips) ||
        layerIndex >= layerCount)
    {
        return 0u;
    }

    gp::UInt64 offset = 0u;
    for (gp::UInt32 mip = 0u; mip < mipIndex; ++mip)
    {
        offset += resource::calculateTextureMipSize(desc, mip) * layerCount;
    }

    const resource::IntVector blocks = resource::calculateMipBlockCount(desc, mipIndex);
    outRowStride =
        static_cast<gp::UInt64>(blocks.x) * static_cast<gp::UInt64>(getPixelFormatInfo(desc.format).blockBytes);
    outSubresourceSize = resource::calculateTextureMipSize(desc, mipIndex);
    return offset + (outSubresourceSize * layerIndex);
}

}   // namespace gp::rhi::vulkan
