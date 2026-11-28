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

namespace gp::rhi::pipeline
{

/// @brief Creation parameters describing a fixed-function polygon rasterizer state.
struct RasterizerStateDesc
{
    /// @brief Polygon fill mode (point, wireframe, or solid).
    RasterizerFillMode fillMode{ RasterizerFillMode::Point };

    /// @brief Face culling mode (none, clockwise, or counter-clockwise).
    RasterizerCullMode cullMode{ RasterizerCullMode::None };

    /// @brief Constant depth offset added to each pixel's depth value.
    gp::Float32 depthBias{ 0.0f };

    /// @brief Slope-scaled depth offset multiplier for shadow bias.
    gp::Float32 slopeScaleDepthBias{ 0.0f };

    /// @brief Controls clipping vs. clamping outside the near and far clip planes.
    RasterizerDepthClipMode depthClipMode{ RasterizerDepthClipMode::DepthClip };

    /// @brief Enables multisample antialiasing rasterization rules when targeting MSAA surfaces.
    bool allowMSAA{ false };

public:
    /// @brief Constructs a default rasterizer descriptor (`Point`, `CullMode::None`, zero bias, MSAA disabled).
    constexpr RasterizerStateDesc() noexcept = default;

    /// @brief Constructs a rasterizer descriptor with fill mode, cull mode, and MSAA toggle.
    /// @param[in] inFillMode Polygon fill mode.
    /// @param[in] inCullMode Triangle face culling mode.
    /// @param[in] inAllowMSAA True to enable MSAA rasterization rules.
    constexpr RasterizerStateDesc(
        const RasterizerFillMode inFillMode, const RasterizerCullMode inCullMode, const bool inAllowMSAA
    ) noexcept
        : fillMode(inFillMode)
        , cullMode(inCullMode)
        , allowMSAA(inAllowMSAA)
    {}

    /// @brief Constructs a rasterizer descriptor with explicit bias and depth-clip settings.
    /// @param[in] inFillMode Polygon fill mode.
    /// @param[in] inCullMode Triangle face culling mode.
    /// @param[in] inDepthBias Constant depth bias.
    /// @param[in] inSlopeScaleDepthBias Slope-scaled depth bias factor.
    /// @param[in] inDepthClipMode Near/far depth clip or clamp mode.
    /// @param[in] inAllowMSAA True to enable MSAA rasterization rules.
    constexpr RasterizerStateDesc(
        const RasterizerFillMode inFillMode,
        const RasterizerCullMode inCullMode,
        const gp::Float32 inDepthBias,
        const gp::Float32 inSlopeScaleDepthBias,
        const RasterizerDepthClipMode inDepthClipMode,
        const bool inAllowMSAA
    ) noexcept
        : fillMode(inFillMode)
        , cullMode(inCullMode)
        , depthBias(inDepthBias)
        , slopeScaleDepthBias(inSlopeScaleDepthBias)
        , depthClipMode(inDepthClipMode)
        , allowMSAA(inAllowMSAA)
    {}

    /// @brief Compares two rasterizer descriptors field by field.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when all fields compare equal.
    [[nodiscard]] constexpr bool operator==(const RasterizerStateDesc& rhs) const noexcept = default;

    /// @brief Computes a 64-bit hash consistent with `operator==` for state caching.
    /// @return 64-bit hash value.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashOf(fillMode);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(cullMode));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashFloat(depthBias));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashFloat(slopeScaleDepthBias));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(depthClipMode));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(allowMSAA));
        return hash;
    }

    /// @brief Streams all descriptor fields into a `gp::hashing` streaming accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(fillMode);
        hasher.append(cullMode);
        hasher.append((depthBias == 0.0f) ? 0.0f : depthBias);
        hasher.append((slopeScaleDepthBias == 0.0f) ? 0.0f : slopeScaleDepthBias);
        hasher.append(depthClipMode);
        hasher.append(allowMSAA);
    }

    /// @brief Serializes this rasterizer descriptor into `<depthBias slopeScaleDepthBias fillMode cullMode
    ///        depthClipMode allowMSAA>`.
    /// @return Serialized string representation.
    [[nodiscard]] GP_RHI_API gp::String toString() const noexcept;

    /// @brief Parses a `<...>` or whitespace-delimited token sequence into this rasterizer descriptor.
    /// @param[in] src Serialized rasterizer descriptor text.
    /// @return True when all fields parsed within valid bounds; false if `src` is malformed (leaves `*this` unchanged).
    GP_RHI_API bool fromString(gp::StringView src) noexcept;
};

}   // namespace gp::rhi::pipeline
