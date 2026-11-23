// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/View.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"

namespace gp::rhi::view
{

const char* ViewDesc::getBufferTypeString(const BufferViewType bufferType) noexcept
{
    switch (bufferType)
    {
    case BufferViewType::Unknown:
        return "Unknown";

    case BufferViewType::Typed:
        return "Typed";

    case BufferViewType::Structured:
        return "Structured";

    case BufferViewType::AccelerationStructure:
        return "AccelerationStructure";

    case BufferViewType::Raw:
        return "Raw";

    default:
        GP_CHECKF(false, "Invalid BufferViewType enumerator.");
        return "";
    }
}

const char* ViewDesc::getTextureDimensionString(const TextureViewDimension dimension) noexcept
{
    switch (dimension)
    {
    case TextureViewDimension::Texture2D:
        return "Texture2D";

    case TextureViewDimension::Texture2DArray:
        return "Texture2DArray";

    case TextureViewDimension::Texture3D:
        return "Texture3D";

    case TextureViewDimension::TextureCube:
        return "TextureCube";

    case TextureViewDimension::TextureCubeArray:
        return "TextureCubeArray";

    default:
        GP_CHECKF(false, "Invalid TextureViewDimension enumerator.");
        return "";
    }
}

ViewDesc::Buffer::ViewInfo ViewDesc::Buffer::getViewInfo(
    resource::Buffer* const targetBuffer,
    const gp::UInt32 minBufferSRVAlignment,
    const bool supportsRawViewsForAnyBuffer
) const noexcept
{
    GP_CHECK(targetBuffer != nullptr);
    GP_CHECKF(
        bufferType != BufferViewType::Unknown,
        "Buffer view type must be specified via setType() or setTypeFromBuffer()."
    );

    const resource::BufferDesc& desc = targetBuffer->getDesc();
    if (desc.isNull())
    {
        ViewInfo nullInfo{};
        nullInfo.isNullView = true;
        return nullInfo;
    }

    ViewInfo info{};
    info.bufferType = bufferType;
    info.format = format;

    switch (bufferType)
    {
    case BufferViewType::Typed:
    {
        GP_CHECKF(
            (desc.usage & (resource::BufferUsageFlags::StructuredBuffer |
                           resource::BufferUsageFlags::AccelerationStructure)) == resource::BufferUsageFlags::None,
            "Cannot create a Typed view over a StructuredBuffer or AccelerationStructure buffer."
        );
        GP_CHECKF(format != PixelFormat::Unknown, "Typed buffer views require a valid PixelFormat.");
        GP_CHECKF(range.stride == 0u, "Do not specify a manual stride for Typed buffer views.");
        GP_CHECKF(isValidPixelFormat(info.format), "Typed buffer view PixelFormat is invalid.");
        info.strideInBytes = getPixelFormatInfo(info.format).blockBytes;
        const gp::UInt32 effectiveAlignment = gp::math::max(minBufferSRVAlignment, info.strideInBytes);
        GP_CHECKF(
            effectiveAlignment > 0u && (offsetInBytes % effectiveAlignment) == 0u,
            "Typed buffer view offsetInBytes (%u) must be aligned to %u bytes.",
            offsetInBytes,
            effectiveAlignment
        );
        break;
    }

    case BufferViewType::Structured:
    {
        GP_CHECKF(
            (desc.usage & resource::BufferUsageFlags::StructuredBuffer) != resource::BufferUsageFlags::None,
            "Structured buffer views require BufferUsageFlags::StructuredBuffer."
        );
        GP_CHECKF(format == PixelFormat::Unknown, "Structured buffer views must leave format as PixelFormat::Unknown.");
        info.strideInBytes = (range.stride == 0u) ? desc.stride : range.stride;
        GP_CHECKF(info.strideInBytes > 0u, "Structured buffer views require a non-zero stride.");
        GP_CHECKF(
            (offsetInBytes % info.strideInBytes) == 0u,
            "Structured buffer view offsetInBytes (%u) must be a multiple of stride (%u).",
            offsetInBytes,
            info.strideInBytes
        );
        break;
    }

    case BufferViewType::AccelerationStructure:
    {
        GP_CHECKF(
            (desc.usage & resource::BufferUsageFlags::AccelerationStructure) != resource::BufferUsageFlags::None,
            "AccelerationStructure views require BufferUsageFlags::AccelerationStructure."
        );
        GP_CHECKF(
            format == PixelFormat::Unknown, "AccelerationStructure views must leave format as PixelFormat::Unknown."
        );
        GP_CHECKF(rayTracingScene != nullptr, "AccelerationStructure views require a non-null RayTracingScene.");
        info.strideInBytes = 1u;
        break;
    }

    case BufferViewType::Raw:
    {
        GP_CHECKF(
            supportsRawViewsForAnyBuffer ||
                (desc.usage & resource::BufferUsageFlags::ByteAddressBuffer) != resource::BufferUsageFlags::None,
            "Raw buffer views require BufferUsageFlags::ByteAddressBuffer unless the backend allows raw views for any buffer."
        );
        GP_CHECKF(format == PixelFormat::Unknown, "Raw buffer views must leave format as PixelFormat::Unknown.");
        GP_CHECKF(range.stride == 0u, "Do not specify a manual stride for Raw buffer views.");
        GP_CHECKF(
            (offsetInBytes % kRawViewAlignment) == 0u,
            "Raw buffer view offsetInBytes (%u) must be aligned to %u bytes.",
            offsetInBytes,
            kRawViewAlignment
        );
        info.strideInBytes = static_cast<gp::UInt32>(sizeof(gp::UInt32));
        info.format = PixelFormat::Unknown;
        break;
    }

    default:
        GP_CHECKF(false, "Unhandled BufferViewType in ViewDesc::Buffer::getViewInfo().");
        break;
    }

    GP_CHECKF(
        offsetInBytes < desc.size,
        "Buffer view offsetInBytes (%u) must be strictly less than buffer size (%u).",
        offsetInBytes,
        desc.size
    );
    info.offsetInBytes = offsetInBytes;

    GP_CHECKF(
        info.bufferType == BufferViewType::AccelerationStructure || (offsetInBytes == 0u || range.numElements > 0u),
        "Non-zero offsetInBytes requires an explicit numElements count."
    );

    info.numElements = (info.bufferType == BufferViewType::AccelerationStructure || range.numElements == 0u)
                           ? ((desc.size - offsetInBytes) / info.strideInBytes)
                           : range.numElements;

    info.sizeInBytes = info.numElements * info.strideInBytes;

    GP_CHECKF(
        info.offsetInBytes + info.sizeInBytes <= desc.size,
        "Buffer view range [%u, %u) exceeds buffer size (%u).",
        info.offsetInBytes,
        info.offsetInBytes + info.sizeInBytes,
        desc.size
    );
    GP_CHECKF(
        info.format == PixelFormat::Unknown || getPixelFormatInfo(info.format).isValid(),
        "Buffer view format metadata is invalid."
    );

    return info;
}

ViewDesc::BufferSRV::ViewInfo ViewDesc::BufferSRV::getViewInfo(
    resource::Buffer* const targetBuffer,
    const gp::UInt32 minBufferSRVAlignment,
    const bool supportsRawViewsForAnyBuffer
) const noexcept
{
    GP_CHECK(viewType == ViewType::BufferSRV);
    ViewInfo info{};
    static_cast<Buffer::ViewInfo&>(info) =
        Buffer::getViewInfo(targetBuffer, minBufferSRVAlignment, supportsRawViewsForAnyBuffer);
    return info;
}

ViewDesc::BufferUAV::ViewInfo ViewDesc::BufferUAV::getViewInfo(
    resource::Buffer* const targetBuffer,
    const gp::UInt32 minBufferSRVAlignment,
    const bool supportsRawViewsForAnyBuffer
) const noexcept
{
    GP_CHECK(viewType == ViewType::BufferUAV);
    ViewInfo info{};
    static_cast<Buffer::ViewInfo&>(info) =
        Buffer::getViewInfo(targetBuffer, minBufferSRVAlignment, supportsRawViewsForAnyBuffer);
    info.atomicCounter = (atomicCounter != 0u);
    info.appendBuffer = (appendBuffer != 0u);
    return info;
}

ViewDesc::Texture::ViewInfo ViewDesc::Texture::getViewInfo(resource::Texture* const targetTexture) const noexcept
{
    GP_CHECK(targetTexture != nullptr);
    GP_CHECKF(
        dimension != TextureViewDimension::Unknown,
        "Texture view dimension must be specified via setDimension() or setDimensionFromTexture()."
    );

    const resource::TextureDesc& desc = targetTexture->getDesc();

    GP_CHECKF(
        !desc.isTexture3D() || dimension == TextureViewDimension::Texture3D, "3D textures only allow Texture3D views."
    );
    GP_CHECKF(
        desc.isTexture3D() || dimension != TextureViewDimension::Texture3D,
        "Texture3D views require a 3D target texture."
    );
    GP_CHECKF(
        desc.isTextureCube() ||
            (dimension != TextureViewDimension::TextureCube && dimension != TextureViewDimension::TextureCubeArray),
        "Cubemap and cubemap-array views require a cubemap target texture."
    );

    GP_CHECKF(mipRange.num > 0u || mipRange.first == 0u, "Cannot use mipRange.num == 0 with a non-zero first mip.");
    GP_CHECKF(
        static_cast<gp::UInt32>(mipRange.first) + static_cast<gp::UInt32>(mipRange.num) <=
            static_cast<gp::UInt32>(desc.numMips),
        "Texture view mip range [%u, %u) exceeds texture numMips (%u).",
        static_cast<gp::UInt32>(mipRange.first),
        static_cast<gp::UInt32>(mipRange.first) + static_cast<gp::UInt32>(mipRange.num),
        static_cast<gp::UInt32>(desc.numMips)
    );

#if GP_DO_CHECK
    GP_CHECKF(
        arrayRange.num > 0u || arrayRange.first == 0u,
        "Cannot use arrayRange.num == 0 with a non-zero first array slice."
    );
    const gp::UInt32 textureArraySize = desc.isTextureCube()
                                            ? (static_cast<gp::UInt32>(desc.arraySize) * resource::kCubeFaceCount)
                                            : static_cast<gp::UInt32>(desc.arraySize);
    gp::UInt32 viewArrayFirst = static_cast<gp::UInt32>(arrayRange.first);
    gp::UInt32 viewArrayNum =
        (arrayRange.num == 0u) ? static_cast<gp::UInt32>(desc.arraySize) : static_cast<gp::UInt32>(arrayRange.num);
    if (dimension == TextureViewDimension::TextureCube || dimension == TextureViewDimension::TextureCubeArray)
    {
        viewArrayFirst *= resource::kCubeFaceCount;
        viewArrayNum *= resource::kCubeFaceCount;
    }
    GP_CHECKF(
        viewArrayFirst + viewArrayNum <= textureArraySize,
        "Texture view array slice range [%u, %u) exceeds target texture slice count (%u).",
        viewArrayFirst,
        viewArrayFirst + viewArrayNum,
        textureArraySize
    );
#endif

    gp::UInt16 adjustedTextureArraySize = desc.arraySize;
    if (dimension == TextureViewDimension::Texture2DArray && desc.isTextureCube())
    {
        adjustedTextureArraySize =
            static_cast<gp::UInt16>(static_cast<gp::UInt32>(adjustedTextureArraySize) * resource::kCubeFaceCount);
    }

    ViewInfo info{};
    info.format = (format == PixelFormat::Unknown) ? desc.format : format;
    info.plane = plane;
    info.dimension = dimension;

    switch (plane)
    {
    case resource::TexturePlane::Primary:
    case resource::TexturePlane::PrimaryCompressed:
        if (format == PixelFormat::X24_G8_UInt || format == PixelFormat::X32_G8_UInt)
        {
            GP_CHECKF(
                (desc.flags & resource::TextureCreateFlags::DepthStencilTargetable) !=
                    resource::TextureCreateFlags::None,
                "Stencil alias formats require a DepthStencilTargetable texture."
            );
            info.plane = resource::TexturePlane::Stencil;
        }
        break;

    case resource::TexturePlane::Stencil:
        GP_CHECKF(
            (desc.flags & resource::TextureCreateFlags::DepthStencilTargetable) != resource::TextureCreateFlags::None,
            "Stencil plane views require a DepthStencilTargetable texture."
        );
        if (format == PixelFormat::Unknown)
        {
            info.format =
                (desc.format == PixelFormat::D32_Float_S8_UInt) ? PixelFormat::X32_G8_UInt : PixelFormat::X24_G8_UInt;
        }
        break;

    case resource::TexturePlane::HTile:
        GP_CHECKF(
            (desc.flags & resource::TextureCreateFlags::DepthStencilTargetable) != resource::TextureCreateFlags::None,
            "HTile plane views require a DepthStencilTargetable texture."
        );
        break;

    case resource::TexturePlane::Depth:
    case resource::TexturePlane::FMask:
    case resource::TexturePlane::CMask:
        break;

    default:
        GP_CHECKF(false, "Unhandled TexturePlane enumerator in ViewDesc::Texture::getViewInfo().");
        break;
    }

    GP_CHECKF(
        info.format == PixelFormat::Unknown || getPixelFormatInfo(info.format).isValid(),
        "Resolved texture view PixelFormat is invalid."
    );

    info.arrayRange.first = arrayRange.first;
    info.arrayRange.num = (arrayRange.num == 0u) ? adjustedTextureArraySize : arrayRange.num;
    info.allSlices = (info.arrayRange.first == 0u && info.arrayRange.num == adjustedTextureArraySize) ? 1u : 0u;
    return info;
}

ViewDesc::TextureSRV::ViewInfo ViewDesc::TextureSRV::getViewInfo(resource::Texture* const targetTexture) const noexcept
{
    GP_CHECK(viewType == ViewType::TextureSRV);
    GP_CHECK(targetTexture != nullptr);

    const resource::TextureDesc& desc = targetTexture->getDesc();

    ViewInfo info{};
    static_cast<Texture::ViewInfo&>(info) = Texture::getViewInfo(targetTexture);
    info.mipRange.first = mipRange.first;
    info.mipRange.num = (mipRange.num == 0u) ? desc.numMips : mipRange.num;
    info.allMips = (info.mipRange.first == 0u && info.mipRange.num == desc.numMips) ? 1u : 0u;
    info.isSRGB =
        (disableSRGB != 0u)
            ? 0u
            : (((desc.flags & resource::TextureCreateFlags::SRGB) != resource::TextureCreateFlags::None) ? 1u : 0u);
    return info;
}

ViewDesc::TextureUAV::ViewInfo ViewDesc::TextureUAV::getViewInfo(resource::Texture* const targetTexture) const noexcept
{
    GP_CHECK(viewType == ViewType::TextureUAV);
    GP_CHECK(targetTexture != nullptr);

    const resource::TextureDesc& desc = targetTexture->getDesc();

    ViewInfo info{};
    static_cast<Texture::ViewInfo&>(info) = Texture::getViewInfo(targetTexture);
    info.mipLevel = mipRange.first;

    GP_CHECK(mipRange.num == 1u);
    GP_CHECKF(desc.numSamples == 1u, "Cannot create an unordered access view of a multisampled texture.");
    GP_CHECKF(
        info.mipLevel < desc.numMips,
        "Texture UAV mipLevel (%u) is out of bounds for texture with %u mips.",
        static_cast<gp::UInt32>(info.mipLevel),
        static_cast<gp::UInt32>(desc.numMips)
    );

    info.allMips = (desc.numMips == 1u) ? 1u : 0u;
    return info;
}

View::View(
    const resource::ResourceType resourceType,
    resource::ViewableResource* const targetResource,
    const ViewDesc& viewDesc
) noexcept
    : resource::Resource(resourceType)
    , m_resource(targetResource)
    , m_viewDesc(viewDesc)
{
    GP_CHECKF(targetResource != nullptr, "Cannot create a view of a null ViewableResource.");
}

View::~View() noexcept = default;

DescriptorHandle View::getBindlessHandle() const noexcept
{
    return DescriptorHandle{};
}

ShaderResourceView::ShaderResourceView(
    resource::ViewableResource* const targetResource, const ViewDesc& viewDesc
) noexcept
    : View(resource::ResourceType::ShaderResourceView, targetResource, viewDesc)
{
    GP_CHECKF(m_viewDesc.isSRV(), "ShaderResourceView requires an SRV ViewDesc.");
}

ShaderResourceView::~ShaderResourceView() noexcept = default;

UnorderedAccessView::UnorderedAccessView(
    resource::ViewableResource* const targetResource, const ViewDesc& viewDesc
) noexcept
    : View(resource::ResourceType::UnorderedAccessView, targetResource, viewDesc)
{
    GP_CHECKF(m_viewDesc.isUAV(), "UnorderedAccessView requires a UAV ViewDesc.");
}

UnorderedAccessView::~UnorderedAccessView() noexcept = default;

}   // namespace gp::rhi::view
