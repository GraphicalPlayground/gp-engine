// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/Texture.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureReference.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"
#include "views/DescriptorHandle.hpp"

namespace gp::rhi::resource
{

namespace
{

/// @brief Global fallback texture retained for null-targeted `TextureReference` instances.
TextureRef& getDefaultTextureStorage() noexcept
{
    static TextureRef defaultTexture{};
    return defaultTexture;
}

/// @brief Computes ceiling integer division `ceil(dividend / divisor)` for positive divisors.
/// @param[in] dividend Numerator value.
/// @param[in] divisor Positive denominator value.
/// @return Smallest integer greater than or equal to `dividend / divisor`.
[[nodiscard]] constexpr gp::UInt64 divideAndRoundUp(const gp::UInt64 dividend, const gp::UInt64 divisor) noexcept
{
    return (dividend + divisor - 1u) / divisor;
}

}   // namespace

sync::ResourceAccess getDefaultResourceState(const TextureCreateFlags usage, const bool hasInitialData) noexcept
{
    sync::ResourceAccess resourceState = sync::ResourceAccess::SRVMask;

    if (!hasInitialData)
    {
        if ((usage & TextureCreateFlags::RenderTargetable) != TextureCreateFlags::None)
        {
            resourceState = sync::ResourceAccess::RTV;
        }
        else if ((usage & TextureCreateFlags::DepthStencilTargetable) != TextureCreateFlags::None)
        {
            resourceState = sync::ResourceAccess::DSVWrite | sync::ResourceAccess::DSVRead;
        }
        else if ((usage & TextureCreateFlags::UAV) != TextureCreateFlags::None)
        {
            resourceState = sync::ResourceAccess::UAVMask;
        }
        else if ((usage & TextureCreateFlags::Presentable) != TextureCreateFlags::None)
        {
            resourceState = sync::ResourceAccess::Present;
        }
        else if ((usage & TextureCreateFlags::ShaderResource) != TextureCreateFlags::None)
        {
            resourceState = sync::ResourceAccess::SRVMask;
        }
        else if ((usage & TextureCreateFlags::Foveation) != TextureCreateFlags::None)
        {
            resourceState = sync::ResourceAccess::ShadingRateSource;
        }
    }

    GP_CHECKF(
        resourceState != sync::ResourceAccess::Unknown,
        "Unable to derive a valid default ResourceAccess state for the given TextureCreateFlags."
    );
    return resourceState;
}

bool TextureDesc::validate(
    const TextureDesc& desc, const gp::StringView debugName, const bool isFatal, const TextureValidationLimits& limits
) noexcept
{
    const auto checkRule = [isFatal, debugName](const bool condition, const char* const message) noexcept -> bool
    {
        if (!condition)
        {
            if (isFatal)
            {
                const int nameLen = static_cast<int>(debugName.size());
                const char* const namePtr = (nameLen > 0) ? debugName.data() : "<unnamed>";
                GP_CHECKF(false, "Texture '%.*s': %s", nameLen > 0 ? nameLen : 9, namePtr, message);
            }
            return false;
        }
        return true;
    };

    // Format checks
    if (!checkRule(desc.format != PixelFormat::Unknown, "TextureDesc format must not be PixelFormat::Unknown."))
    {
        return false;
    }
    if (!checkRule(isValidPixelFormat(desc.format), "TextureDesc format is out of valid PixelFormat range."))
    {
        return false;
    }

    // Extent checks
    const gp::UInt32 maxDimension =
        desc.isTextureCube() ? limits.maxCubeTextureDimensions : limits.maxTextureDimensions;
    if (!checkRule(
            desc.extent.x > 0 && static_cast<gp::UInt32>(desc.extent.x) <= maxDimension,
            "TextureDesc extent.x must be in [1, maxDimension]."
        ))
    {
        return false;
    }
    if (!checkRule(
            desc.extent.y > 0 && static_cast<gp::UInt32>(desc.extent.y) <= maxDimension,
            "TextureDesc extent.y must be in [1, maxDimension]."
        ))
    {
        return false;
    }

    // Depth checks
    if (desc.isTexture3D())
    {
        if (!checkRule(
                desc.depth > 0u && static_cast<gp::UInt32>(desc.depth) <= limits.maxTextureDimensions,
                "3D TextureDesc depth must be in [1, maxTextureDimensions]."
            ))
        {
            return false;
        }
    }
    else
    {
        if (!checkRule(desc.depth == 1u, "Non-3D TextureDesc must have depth == 1."))
        {
            return false;
        }
    }

    // ArraySize checks
    if (desc.isTextureArray())
    {
        if (!checkRule(
                desc.arraySize > 0u && static_cast<gp::UInt32>(desc.arraySize) <= limits.maxTextureArrayLayers,
                "Array TextureDesc arraySize must be in [1, maxTextureArrayLayers]."
            ))
        {
            return false;
        }
    }
    else
    {
        if (!checkRule(desc.arraySize == 1u, "Non-array TextureDesc must have arraySize == 1."))
        {
            return false;
        }
    }

    // NumSamples checks
    if (desc.isTexture2D())
    {
        if (!checkRule(desc.numSamples > 0u, "2D TextureDesc numSamples must be greater than zero."))
        {
            return false;
        }
    }
    else
    {
        if (!checkRule(desc.numSamples == 1u, "Non-2D TextureDesc must have numSamples == 1."))
        {
            return false;
        }
    }

    // NumMips checks
    if (desc.isMultisample())
    {
        if (!checkRule(desc.numMips == 1u, "Multisampled TextureDesc must have numMips == 1."))
        {
            return false;
        }
    }
    else
    {
        if (!checkRule(
                desc.numMips > 0u && static_cast<gp::UInt32>(desc.numMips) <= limits.maxTextureMipCount,
                "TextureDesc numMips must be in [1, maxTextureMipCount]."
            ))
        {
            return false;
        }
    }

    // Reserved resource checks
    if ((desc.flags & TextureCreateFlags::ReservedResource) != TextureCreateFlags::None)
    {
        if (!checkRule(limits.supportsReservedResources, "Reserved textures are not supported by the active limits."))
        {
            return false;
        }

        if (desc.isTexture3D())
        {
            if (!checkRule(
                    limits.supportsReservedVolumeTextures,
                    "Reserved 3D volume textures are not supported by the active limits."
                ))
            {
                return false;
            }
        }
        else
        {
            if (!checkRule(
                    desc.isTexture2D(), "Reserved non-3D textures must have Texture2D or Texture2DArray dimension."
                ))
            {
                return false;
            }

            if (desc.dimension == TextureDimension::Texture2DArray)
            {
                const gp::UInt32 mipShift = gp::math::max<gp::UInt32>(1u, static_cast<gp::UInt32>(desc.numMips)) - 1u;
                const gp::UInt32 smallestX =
                    (mipShift < 32u) ? (static_cast<gp::UInt32>(desc.extent.x) >> mipShift) : 0u;
                const gp::UInt32 smallestY =
                    (mipShift < 32u) ? (static_cast<gp::UInt32>(desc.extent.y) >> mipShift) : 0u;
                if (!checkRule(
                        smallestX >= limits.reservedTextureArrayMinMipDimension &&
                            smallestY >= limits.reservedTextureArrayMinMipDimension,
                        "Reserved Texture2DArray smallest mip dimension is below minimum requirement."
                    ))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool TextureDesc::isValid(const TextureValidationLimits& limits) const noexcept
{
    return validate(*this, gp::StringView{}, false, limits);
}

bool TextureDesc::checkValidity(
    const TextureDesc& desc, const gp::StringView debugName, const TextureValidationLimits& limits
) noexcept
{
    return validate(desc, debugName, true, limits);
}

gp::UInt64
    TextureDesc::calcMemorySizeEstimate(const gp::UInt32 firstMipIndex, const gp::UInt32 lastMipIndex) const noexcept
{
#if GP_DO_CHECK
    validate(*this, gp::StringView("calcMemorySizeEstimate"), true, TextureValidationLimits{});
#endif

    GP_CHECKF(
        firstMipIndex < static_cast<gp::UInt32>(numMips) && firstMipIndex <= lastMipIndex &&
            lastMipIndex < static_cast<gp::UInt32>(numMips),
        "Mip range [%u, %u] is out of bounds for texture with %u mips.",
        firstMipIndex,
        lastMipIndex,
        static_cast<gp::UInt32>(numMips)
    );

    const PixelFormatInfo& formatInfo = getPixelFormatInfo(format);
    const gp::UInt64 blockSizeX = static_cast<gp::UInt64>(formatInfo.blockSizeX);
    const gp::UInt64 blockSizeY = static_cast<gp::UInt64>(formatInfo.blockSizeY);
    const gp::UInt64 blockSizeZ = static_cast<gp::UInt64>(formatInfo.blockSizeZ);
    const gp::UInt64 blockBytes = static_cast<gp::UInt64>(formatInfo.blockBytes);

    gp::UInt64 memorySize = 0u;
    for (gp::UInt32 mipIndex = firstMipIndex; mipIndex <= lastMipIndex; ++mipIndex)
    {
        const gp::UInt64 mipX = static_cast<gp::UInt64>(gp::math::max(extent.x >> mipIndex, 1));
        const gp::UInt64 mipY = static_cast<gp::UInt64>(gp::math::max(extent.y >> mipIndex, 1));
        const gp::UInt64 mipZ = static_cast<gp::UInt64>(gp::math::max(static_cast<gp::Int32>(depth) >> mipIndex, 1));

        const gp::UInt64 blocksX = divideAndRoundUp(mipX, blockSizeX);
        const gp::UInt64 blocksY = divideAndRoundUp(mipY, blockSizeY);
        const gp::UInt64 blocksZ = divideAndRoundUp(mipZ, blockSizeZ);

        memorySize += blocksX * blocksY * blocksZ * blockBytes;
    }

    memorySize *= static_cast<gp::UInt64>(arraySize);
    memorySize *= static_cast<gp::UInt64>(numSamples);
    if (isTextureCube())
    {
        memorySize *= static_cast<gp::UInt64>(kCubeFaceCount);
    }

    return memorySize;
}

Texture::Texture(const TextureCreateDesc& createDesc) noexcept
    : ViewableResource(ResourceType::Texture, createDesc.initialState, createDesc.debugName, createDesc.ownerName)
    , m_textureDesc(createDesc)
{
#if GP_ENABLE_RHI_VALIDATION
    const gp::UInt32 numMips = gp::math::max<gp::UInt32>(1u, static_cast<gp::UInt32>(m_textureDesc.numMips));
    const gp::UInt32 baseArraySlices = gp::math::max<gp::UInt32>(1u, static_cast<gp::UInt32>(m_textureDesc.arraySize));
    const gp::UInt32 effectiveSlices =
        m_textureDesc.isTextureCube() ? (baseArraySlices * kCubeFaceCount) : baseArraySlices;
    const bool isDepthStencil =
        ((m_textureDesc.flags & TextureCreateFlags::DepthStencilTargetable) != TextureCreateFlags::None) ||
        (m_textureDesc.format == PixelFormat::D24_UNorm_S8_UInt) ||
        (m_textureDesc.format == PixelFormat::D32_Float_S8_UInt) || (m_textureDesc.format == PixelFormat::D32_Float) ||
        (m_textureDesc.format == PixelFormat::D16_UNorm);
    const gp::UInt32 numPlanes = isDepthStencil ? static_cast<gp::UInt32>(validation::ResourcePlane::Max) : 1u;

    m_validationResource.initBarrierTracking(
        numMips, effectiveSlices, numPlanes, createDesc.initialState, createDesc.debugName, false
    );
#endif
}

Texture::Texture(const ResourceType resourceType) noexcept
    : ViewableResource(resourceType, sync::ResourceAccess::Unknown, gp::StringView{}, gp::Name{})
{
    GP_CHECKF(
        resourceType == ResourceType::TextureReference,
        "Texture(ResourceType) constructor is reserved exclusively for TextureReference."
    );
}

Texture::~Texture() noexcept = default;

const TextureDesc& Texture::getDesc() const noexcept
{
    return m_textureDesc;
}

TextureReference* Texture::getTextureReference() noexcept
{
    return nullptr;
}

view::DescriptorHandle Texture::getDefaultBindlessHandle() const noexcept
{
    return view::DescriptorHandle{};
}

void* Texture::getNativeResource() const noexcept
{
    return nullptr;
}

void* Texture::getNativeShaderResourceView() const noexcept
{
    return nullptr;
}

void* Texture::getTextureBaseRHI() noexcept
{
    return nullptr;
}

void Texture::getWriteMaskProperties(void*& outData, gp::UInt32& outSize) noexcept
{
    outData = nullptr;
    outSize = 0u;
}

void Texture::setName(const gp::Name inName) noexcept
{
    ViewableResource::setName(inName);
#if GP_ENABLE_RHI_VALIDATION
    m_validationResource.setDebugName(inName.toString().view());
#endif
}

TextureReference::TextureReference(Texture* const referencedTexture) noexcept
    : Texture(ResourceType::TextureReference)
    , m_referencedTexture(referencedTexture != nullptr ? referencedTexture : getDefaultTextureStorage().get())
    , m_bindlessHandle()
{
    GP_CHECKF(
        m_referencedTexture.isValid(),
        "TextureReference requires either a non-null referencedTexture or a registered default texture."
    );
}

TextureReference::TextureReference(
    Texture* const referencedTexture, const view::DescriptorHandle bindlessHandle
) noexcept
    : Texture(ResourceType::TextureReference)
    , m_referencedTexture(referencedTexture != nullptr ? referencedTexture : getDefaultTextureStorage().get())
    , m_bindlessHandle(bindlessHandle)
{
    GP_CHECKF(
        m_referencedTexture.isValid(),
        "TextureReference requires either a non-null referencedTexture or a registered default texture."
    );
}

TextureReference::~TextureReference() noexcept = default;

const TextureDesc& TextureReference::getDesc() const noexcept
{
    GP_CHECK(m_referencedTexture.isValid());
    return m_referencedTexture->getDesc();
}

TextureReference* TextureReference::getTextureReference() noexcept
{
    return this;
}

view::DescriptorHandle TextureReference::getDefaultBindlessHandle() const noexcept
{
    if (m_bindlessHandle.isValid())
    {
        return m_bindlessHandle;
    }

    GP_CHECK(m_referencedTexture.isValid());
    return m_referencedTexture->getDefaultBindlessHandle();
}

void* TextureReference::getNativeResource() const noexcept
{
    GP_CHECK(m_referencedTexture.isValid());
    return m_referencedTexture->getNativeResource();
}

void* TextureReference::getNativeShaderResourceView() const noexcept
{
    GP_CHECK(m_referencedTexture.isValid());
    return m_referencedTexture->getNativeShaderResourceView();
}

void* TextureReference::getTextureBaseRHI() noexcept
{
    GP_CHECK(m_referencedTexture.isValid());
    return m_referencedTexture->getTextureBaseRHI();
}

void TextureReference::getWriteMaskProperties(void*& outData, gp::UInt32& outSize) noexcept
{
    GP_CHECK(m_referencedTexture.isValid());
    m_referencedTexture->getWriteMaskProperties(outData, outSize);
}

#if GP_ENABLE_RHI_VALIDATION
validation::Resource* TextureReference::getValidationTrackerResource() noexcept
{
    return m_referencedTexture.isValid() ? m_referencedTexture->getValidationTrackerResource() : nullptr;
}
#endif

Texture* TextureReference::getDefaultTexture() noexcept
{
    return getDefaultTextureStorage().get();
}

void TextureReference::setDefaultTexture(Texture* const defaultTexture) noexcept
{
    getDefaultTextureStorage() = TextureRef(defaultTexture);
}

void TextureReference::setTrackedAccessFromContext(const sync::TrackedAccess trackedAccess) noexcept
{
    GP_CHECK(m_referencedTexture.isValid());
    m_referencedTexture->setTrackedAccessFromContext(trackedAccess);
}

void TextureReference::setReferencedTexture(Texture* const newTexture) noexcept
{
    m_referencedTexture = TextureRef(newTexture != nullptr ? newTexture : getDefaultTextureStorage().get());
    GP_CHECK(m_referencedTexture.isValid());
}

}   // namespace gp::rhi::resource
