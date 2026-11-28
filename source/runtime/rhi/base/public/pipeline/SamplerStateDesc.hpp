// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashMix.hpp"
#include "hashing/HashOf.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "templates/NumericLimits.hpp"

namespace gp::rhi::pipeline
{

/// @brief Creation parameters describing a GPU texture sampler state.
struct SamplerStateDesc
{
    /// @brief Magnification, minification, and mipmap filtering mode.
    SamplerFilter filter{ SamplerFilter::Point };

    /// @brief Horizontal (U) texture coordinate addressing mode.
    SamplerAddressMode addressU{ SamplerAddressMode::Wrap };

    /// @brief Vertical (V) texture coordinate addressing mode.
    SamplerAddressMode addressV{ SamplerAddressMode::Wrap };

    /// @brief Depth (W) texture coordinate addressing mode.
    SamplerAddressMode addressW{ SamplerAddressMode::Wrap };

    /// @brief Bias added to the computed mipmap LOD before clamping.
    gp::Float32 mipBias{ 0.0f };

    /// @brief Lowest mip level index sampled (`0` is full resolution).
    gp::Float32 minMipLevel{ 0.0f };

    /// @brief Highest mip level index sampled (`FLT_MAX` allows all available mips).
    gp::Float32 maxMipLevel{ gp::NumericLimits<gp::Float32>::max() };

    /// @brief Maximum anisotropic filtering taps (`0` uses backend defaults).
    gp::Int32 maxAnisotropy{ 0 };

    /// @brief Packed RGBA8 border color used when any address mode is `SamplerAddressMode::Border`.
    gp::UInt32 borderColor{ 0u };

    /// @brief Comparison operator for depth-comparison samplers.
    SamplerCompareFunction samplerComparisonFunction{ SamplerCompareFunction::Never };

public:
    /// @brief Constructs a default point-wrapped sampler descriptor.
    constexpr SamplerStateDesc() noexcept = default;

    /// @brief Constructs a sampler descriptor with explicit filtering, addressing, LOD, and comparison settings.
    /// @param[in] inFilter Texture filter mode.
    /// @param[in] inAddressU U coordinate addressing mode.
    /// @param[in] inAddressV V coordinate addressing mode.
    /// @param[in] inAddressW W coordinate addressing mode.
    /// @param[in] inMipBias Mip LOD bias.
    /// @param[in] inMaxAnisotropy Maximum anisotropic tap count.
    /// @param[in] inMinMipLevel Minimum mip level index.
    /// @param[in] inMaxMipLevel Maximum mip level index.
    /// @param[in] inBorderColor Packed border color value.
    /// @param[in] inSamplerComparisonFunction Comparison function for shadow/depth sampling.
    constexpr explicit(false) SamplerStateDesc(
        const SamplerFilter inFilter,
        const SamplerAddressMode inAddressU = SamplerAddressMode::Wrap,
        const SamplerAddressMode inAddressV = SamplerAddressMode::Wrap,
        const SamplerAddressMode inAddressW = SamplerAddressMode::Wrap,
        const gp::Float32 inMipBias = 0.0f,
        const gp::Int32 inMaxAnisotropy = 0,
        const gp::Float32 inMinMipLevel = 0.0f,
        const gp::Float32 inMaxMipLevel = gp::NumericLimits<gp::Float32>::max(),
        const gp::UInt32 inBorderColor = 0u,
        const SamplerCompareFunction inSamplerComparisonFunction = SamplerCompareFunction::Never
    ) noexcept
        : filter(inFilter)
        , addressU(inAddressU)
        , addressV(inAddressV)
        , addressW(inAddressW)
        , mipBias(inMipBias)
        , minMipLevel(inMinMipLevel)
        , maxMipLevel(inMaxMipLevel)
        , maxAnisotropy(inMaxAnisotropy)
        , borderColor(inBorderColor)
        , samplerComparisonFunction(inSamplerComparisonFunction)
    {}

    /// @brief Compares two sampler descriptors field by field.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when all fields compare equal.
    [[nodiscard]] constexpr bool operator==(const SamplerStateDesc& rhs) const noexcept = default;

    /// @brief Computes a 64-bit hash consistent with `operator==` for sampler state caching.
    /// @return 64-bit hash value.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashOf(filter);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(addressU));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(addressV));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(addressW));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashFloat(mipBias));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashFloat(minMipLevel));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashFloat(maxMipLevel));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(maxAnisotropy));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(borderColor));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(samplerComparisonFunction));
        return hash;
    }

    /// @brief Streams all descriptor fields into a `gp::hashing` streaming accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(filter);
        hasher.append(addressU);
        hasher.append(addressV);
        hasher.append(addressW);
        hasher.append((mipBias == 0.0f) ? 0.0f : mipBias);
        hasher.append((minMipLevel == 0.0f) ? 0.0f : minMipLevel);
        hasher.append((maxMipLevel == 0.0f) ? 0.0f : maxMipLevel);
        hasher.append(maxAnisotropy);
        hasher.append(borderColor);
        hasher.append(samplerComparisonFunction);
    }

    /// @brief Serializes this sampler descriptor into a compact `<...>` text token sequence.
    /// @return Serialized string representation.
    [[nodiscard]] GP_RHI_API gp::String toString() const noexcept;

    /// @brief Parses a `<...>` or whitespace-delimited token sequence into this sampler descriptor.
    /// @param[in] src Serialized sampler descriptor text.
    /// @return True when all fields parsed within valid bounds; false if `src` is malformed (leaves `*this` unchanged).
    GP_RHI_API bool fromString(gp::StringView src) noexcept;
};

}   // namespace gp::rhi::pipeline
