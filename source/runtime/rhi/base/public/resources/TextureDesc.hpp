// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/ClearValueBinding.hpp"
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashOf.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"

namespace gp::rhi::resource
{

/// @brief Hardware texture dimension and reserved-resource limits consulted by `TextureDesc::validate()`.
struct TextureValidationLimits
{
    /// @brief Maximum width, height, and 3D depth in texels.
    gp::UInt32 maxTextureDimensions{ kDefaultMaxTextureDimensions };

    /// @brief Maximum face width and height in texels for cubemap textures.
    gp::UInt32 maxCubeTextureDimensions{ kDefaultMaxCubeTextureDimensions };

    /// @brief Maximum array slice count for texture arrays.
    gp::UInt32 maxTextureArrayLayers{ kDefaultMaxTextureArrayLayers };

    /// @brief Maximum number of mip levels per texture.
    gp::UInt32 maxTextureMipCount{ kMaxTextureMipCount };

    /// @brief Minimum smallest-mip dimension required by reserved texture arrays.
    gp::UInt32 reservedTextureArrayMinMipDimension{ kDefaultReservedTextureArrayMinMipDimension };

    /// @brief True when the active backend supports reserved (tiled/sparse) textures.
    bool supportsReservedResources{ true };

    /// @brief True when the active backend supports reserved 3D volume textures.
    bool supportsReservedVolumeTextures{ true };
};

/// @brief Descriptor capturing dimensionality, format, flags, extent, clear value, and aliasing metadata of a texture.
/// @details Shared between texture creation, transient resource allocation, and descriptor caching. Provides exact
///          validation, mip memory estimation, subresource counting, equality, and deterministic hashing.
struct TextureDesc
{
    /// @brief Usage, memory placement, and binding flags.
    TextureCreateFlags flags{ TextureCreateFlags::None };

    /// @brief Bound hardware fast-clear color or depth-stencil value.
    ClearValueBinding clearValue{};

    /// @brief Multi-GPU adapter node affinity mask.
    GPUMask gpuMask{ GPUMask::all() };

    /// @brief Optional platform-specific offline texture metadata payload.
    gp::UInt32 extData{ 0u };

    /// @brief Base mip width (`x`) and height (`y`) in texels.
    IntPoint extent{ 1, 1 };

    /// @brief Base mip depth in texels for 3D textures; must be `1` for all non-3D dimensions.
    gp::UInt16 depth{ 1u };

    /// @brief Array slice count for 2D/cube arrays; must be `1` for non-array dimensions.
    gp::UInt16 arraySize{ 1u };

    /// @brief Total number of mip levels in the mip chain.
    gp::UInt8 numMips{ 1u };

    /// @brief Sample count per pixel (`> 1` enables MSAA on 2D/2D-array textures).
    gp::UInt8 numSamples{ 1u };

    /// @brief Texture dimensionality (2D, 2D array, 3D, cube, or cube array).
    TextureDimension dimension{ TextureDimension::Texture2D };

    /// @brief Primary storage pixel format.
    PixelFormat format{ PixelFormat::Unknown };

    /// @brief Optional alternate format used for unordered access views (`PixelFormat::Unknown` uses `format`).
    PixelFormat uavFormat{ PixelFormat::Unknown };

    /// @brief Fraction of memory requested in fast VRAM, mapped from `[0, 255]` to `[0%, 100%]`.
    gp::UInt8 fastVRAMPercentage{ 0xFFu };

    /// @brief Additional pixel formats that may alias this texture's memory through views.
    gp::InlineVector<PixelFormat, 1u> aliasableFormats{};

    /// @brief Constructs a default 1x1 2D texture descriptor with `PixelFormat::Unknown`.
    TextureDesc() noexcept = default;

    /// @brief Copy constructor.
    /// @param[in] other Descriptor to copy.
    TextureDesc(const TextureDesc& other) noexcept = default;

    /// @brief Move constructor.
    /// @param[in,out] other Descriptor to move from.
    TextureDesc(TextureDesc&& other) noexcept = default;

    /// @brief Copy assignment operator.
    /// @param[in] other Descriptor to copy.
    /// @return Reference to `*this`.
    TextureDesc& operator=(const TextureDesc& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param[in,out] other Descriptor to move from.
    /// @return Reference to `*this`.
    TextureDesc& operator=(TextureDesc&& other) noexcept = default;

    /// @brief Constructs a descriptor targeting `inDimension` with default extent and format.
    /// @param[in] inDimension Texture dimensionality.
    explicit TextureDesc(const TextureDimension inDimension) noexcept
        : dimension(inDimension)
    {}

    /// @brief Constructs a texture descriptor with all core creation properties.
    /// @param[in] inDimension Texture dimensionality.
    /// @param[in] inFlags Usage and placement flags.
    /// @param[in] inFormat Primary pixel format.
    /// @param[in] inClearValue Bound fast-clear value.
    /// @param[in] inExtent Base mip width and height.
    /// @param[in] inDepth Base mip depth (`1` for non-3D).
    /// @param[in] inArraySize Array slice count (`1` for non-arrays).
    /// @param[in] inNumMips Number of mip levels.
    /// @param[in] inNumSamples Sample count per pixel.
    /// @param[in] inExtData Optional platform metadata word.
    TextureDesc(
        const TextureDimension inDimension,
        const TextureCreateFlags inFlags,
        const PixelFormat inFormat,
        const ClearValueBinding& inClearValue,
        const IntPoint inExtent,
        const gp::UInt16 inDepth,
        const gp::UInt16 inArraySize,
        const gp::UInt8 inNumMips,
        const gp::UInt8 inNumSamples,
        const gp::UInt32 inExtData
    ) noexcept
        : flags(inFlags)
        , clearValue(inClearValue)
        , extData(inExtData)
        , extent(inExtent)
        , depth(inDepth)
        , arraySize(inArraySize)
        , numMips(inNumMips)
        , numSamples(inNumSamples)
        , dimension(inDimension)
        , format(inFormat)
    {}

    /// @brief Reports whether this descriptor represents a 2D or 2D-array texture.
    /// @return True for `Texture2D` and `Texture2DArray`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isTexture2D() const noexcept
    {
        return (dimension == TextureDimension::Texture2D) || (dimension == TextureDimension::Texture2DArray);
    }

    /// @brief Reports whether this descriptor represents a 3D volume texture.
    /// @return True for `Texture3D`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isTexture3D() const noexcept
    {
        return dimension == TextureDimension::Texture3D;
    }

    /// @brief Reports whether this descriptor represents a cubemap or cubemap array texture.
    /// @return True for `TextureCube` and `TextureCubeArray`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isTextureCube() const noexcept
    {
        return (dimension == TextureDimension::TextureCube) || (dimension == TextureDimension::TextureCubeArray);
    }

    /// @brief Reports whether this descriptor represents an arrayed texture.
    /// @return True for `Texture2DArray` and `TextureCubeArray`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isTextureArray() const noexcept
    {
        return (dimension == TextureDimension::Texture2DArray) || (dimension == TextureDimension::TextureCubeArray);
    }

    /// @brief Reports whether the texture has more than one mip level.
    /// @return True when `numMips > 1`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isMipChain() const noexcept
    {
        return numMips > 1u;
    }

    /// @brief Reports whether the texture uses multisampling.
    /// @return True when `numSamples > 1`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isMultisample() const noexcept
    {
        return numSamples > 1u;
    }

    /// @brief Returns the base mip `(extent.x, extent.y, depth)` size vector.
    /// @return 3D integer extent vector.
    [[nodiscard]] GP_FORCEINLINE_HINT IntVector getSize() const noexcept
    {
        return IntVector(extent.x, extent.y, static_cast<gp::Int32>(depth));
    }

    /// @brief Resets transient/MSAA/alias fields to a clean single-sampled render-target baseline.
    /// @details Sets `numSamples = 1`, adds `RenderTargetable`, strips `UAV`, `ResolveTargetable`,
    ///          `DepthStencilResolveTarget`, and `Memoryless`, and clears `aliasableFormats`.
    void reset() noexcept
    {
        numSamples = 1u;
        flags |= TextureCreateFlags::RenderTargetable;
        flags &=
            ~(TextureCreateFlags::UAV | TextureCreateFlags::ResolveTargetable |
              TextureCreateFlags::DepthStencilResolveTarget | TextureCreateFlags::Memoryless);
        aliasableFormats.clear();
    }

    /// @brief Validates all descriptor invariants against `limits` without asserting.
    /// @param[in] limits Hardware limit set to validate against.
    /// @return True when every field satisfies texture creation constraints; false otherwise.
    [[nodiscard]] GP_RHI_API bool
        isValid(const TextureValidationLimits& limits = TextureValidationLimits{}) const noexcept;

    /// @brief Validates `desc` against `limits`, triggering a `GP_CHECKF` assertion on violation.
    /// @param[in] desc Texture descriptor to validate.
    /// @param[in] debugName Optional debug name included in diagnostic messages.
    /// @param[in] limits Hardware limit set to validate against.
    /// @return True if valid.
    GP_RHI_API static bool checkValidity(
        const TextureDesc& desc,
        gp::StringView debugName = gp::StringView{},
        const TextureValidationLimits& limits = TextureValidationLimits{}
    ) noexcept;

    /// @brief Estimates total GPU memory footprint in bytes across `[firstMipIndex, lastMipIndex]`.
    /// @param[in] firstMipIndex Inclusive first mip index.
    /// @param[in] lastMipIndex Inclusive last mip index.
    /// @return Estimated memory footprint in bytes, accounting for block compression, array slices, MSAA, and cube
    /// faces.
    [[nodiscard]] GP_RHI_API gp::UInt64
        calcMemorySizeEstimate(gp::UInt32 firstMipIndex, gp::UInt32 lastMipIndex) const noexcept;

    /// @brief Estimates total GPU memory footprint in bytes from `firstMipIndex` through the final mip level.
    /// @param[in] firstMipIndex Inclusive first mip index (defaults to `0`).
    /// @return Estimated memory footprint in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64
        calcMemorySizeEstimate(const gp::UInt32 firstMipIndex = 0u) const noexcept
    {
        return calcMemorySizeEstimate(firstMipIndex, static_cast<gp::UInt32>(numMips) - 1u);
    }

    /// @brief Computes the total number of subresources (`arraySize * numMips * numFaces * numPlanes`).
    /// @return Total subresource count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt16 getSubresourceCount() const noexcept
    {
        const gp::UInt32 numFaces = isTextureCube() ? kCubeFaceCount : 1u;
        const gp::UInt32 numPlanes = isStencilFormat(format) ? 2u : 1u;
        return static_cast<gp::UInt16>(
            static_cast<gp::UInt32>(arraySize) * static_cast<gp::UInt32>(numMips) * numFaces * numPlanes
        );
    }

    /// @brief Compares all 14 descriptor fields for exact equality.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when every field and aliasable format entry matches.
    [[nodiscard]] bool operator==(const TextureDesc& rhs) const noexcept
    {
        return (dimension == rhs.dimension) && (flags == rhs.flags) && (format == rhs.format) &&
               (uavFormat == rhs.uavFormat) && (extent == rhs.extent) && (depth == rhs.depth) &&
               (arraySize == rhs.arraySize) && (numMips == rhs.numMips) && (numSamples == rhs.numSamples) &&
               (fastVRAMPercentage == rhs.fastVRAMPercentage) && (clearValue == rhs.clearValue) &&
               (extData == rhs.extData) && (gpuMask == rhs.gpuMask) && (aliasableFormats == rhs.aliasableFormats);
    }

    /// @brief Computes a deterministic 64-bit hash covering all fields and the format's block byte width.
    /// @return 64-bit hash digest.
    [[nodiscard]] gp::UInt64 getHash() const noexcept
    {
        const gp::UInt32 blockBytes = isValidPixelFormat(format) ? getPixelFormatInfo(format).blockBytes : 0u;
        gp::UInt64 hash = gp::hashing::hashOf(dimension);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(flags));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(format));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(uavFormat));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(extent.x));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(extent.y));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(depth));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(arraySize));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(numMips));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(numSamples));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(fastVRAMPercentage));
        hash = gp::hashing::hashCombine(hash, clearValue.getHash());
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(extData));
        hash = gp::hashing::hashCombine(hash, gpuMask.getHash());
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(blockBytes));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(static_cast<gp::UInt32>(aliasableFormats.size())));
        for (const PixelFormat aliasFormat: aliasableFormats)
        {
            hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(aliasFormat));
        }
        return hash;
    }

    /// @brief Streams all descriptor fields and format block byte size into a streaming hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target hasher accumulator.
    template <typename InHasher>
    void hashAppend(InHasher& hasher) const noexcept
    {
        const gp::UInt32 blockBytes = isValidPixelFormat(format) ? getPixelFormatInfo(format).blockBytes : 0u;
        hasher.appendAll(
            dimension,
            flags,
            format,
            uavFormat,
            extent,
            depth,
            arraySize,
            numMips,
            numSamples,
            fastVRAMPercentage,
            clearValue,
            extData,
            gpuMask,
            blockBytes,
            static_cast<gp::UInt32>(aliasableFormats.size())
        );
        for (const PixelFormat aliasFormat: aliasableFormats)
        {
            hasher.append(aliasFormat);
        }
    }

private:
    /// @brief Shared validation core invoked by `isValid()` (non-fatal) and `checkValidity()` (fatal).
    /// @param[in] desc Descriptor to check.
    /// @param[in] debugName Diagnostic name for assertion messages.
    /// @param[in] isFatal When true, fails via `GP_CHECKF`; when false, returns `false`.
    /// @param[in] limits Hardware limits to validate against.
    /// @return True when valid; false when invalid and `isFatal` is false.
    GP_RHI_API static bool validate(
        const TextureDesc& desc, gp::StringView debugName, bool isFatal, const TextureValidationLimits& limits
    ) noexcept;
};

/// @brief Derives the default initial GPU resource access state for a texture from its creation flags.
/// @param[in] usage Texture creation flags.
/// @param[in] hasInitialData True if initial subresources will be uploaded at creation time.
/// @return Resolved `sync::ResourceAccess` state suitable for initial barrier tracking.
[[nodiscard]] GP_RHI_API sync::ResourceAccess
    getDefaultResourceState(TextureCreateFlags usage, bool hasInitialData) noexcept;

}   // namespace gp::rhi::resource
