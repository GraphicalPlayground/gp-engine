// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashOf.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include <type_traits>

namespace gp::rhi::pipeline
{

/// @brief Blend equations, blend factors, and color write mask for one color render target slot.
struct RenderTargetBlendDesc
{
    /// @brief Arithmetic operation combining source and destination RGB channels.
    BlendOperation colorBlendOp{ BlendOperation::Add };

    /// @brief Multiplier applied to source RGB channels.
    BlendFactor colorSrcBlend{ BlendFactor::One };

    /// @brief Multiplier applied to destination RGB channels.
    BlendFactor colorDestBlend{ BlendFactor::Zero };

    /// @brief Arithmetic operation combining source and destination alpha channels.
    BlendOperation alphaBlendOp{ BlendOperation::Add };

    /// @brief Multiplier applied to the source alpha channel.
    BlendFactor alphaSrcBlend{ BlendFactor::One };

    /// @brief Multiplier applied to the destination alpha channel.
    BlendFactor alphaDestBlend{ BlendFactor::Zero };

    /// @brief Bitmask selecting which RGBA channels are written to the render target.
    ColorWriteMask colorWriteMask{ ColorWriteMask::RGBA };

public:
    /// @brief Constructs a per-render-target blend descriptor (defaults to opaque overwrite on RGBA).
    /// @param[in] inColorBlendOp RGB blend operation.
    /// @param[in] inColorSrcBlend Source RGB blend factor.
    /// @param[in] inColorDestBlend Destination RGB blend factor.
    /// @param[in] inAlphaBlendOp Alpha blend operation.
    /// @param[in] inAlphaSrcBlend Source alpha blend factor.
    /// @param[in] inAlphaDestBlend Destination alpha blend factor.
    /// @param[in] inColorWriteMask Enabled RGBA channel write mask.
    constexpr RenderTargetBlendDesc(
        const BlendOperation inColorBlendOp = BlendOperation::Add,
        const BlendFactor inColorSrcBlend = BlendFactor::One,
        const BlendFactor inColorDestBlend = BlendFactor::Zero,
        const BlendOperation inAlphaBlendOp = BlendOperation::Add,
        const BlendFactor inAlphaSrcBlend = BlendFactor::One,
        const BlendFactor inAlphaDestBlend = BlendFactor::Zero,
        const ColorWriteMask inColorWriteMask = ColorWriteMask::RGBA
    ) noexcept
        : colorBlendOp(inColorBlendOp)
        , colorSrcBlend(inColorSrcBlend)
        , colorDestBlend(inColorDestBlend)
        , alphaBlendOp(inAlphaBlendOp)
        , alphaSrcBlend(inAlphaSrcBlend)
        , alphaDestBlend(inAlphaDestBlend)
        , colorWriteMask(inColorWriteMask)
    {}

    /// @brief Compares two per-target blend descriptors field by field.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const RenderTargetBlendDesc& rhs) const noexcept = default;

    /// @brief Computes a 64-bit hash across all blend equation, factor, and write-mask fields.
    /// @return 64-bit hash value.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashOf(colorBlendOp);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(colorSrcBlend));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(colorDestBlend));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(alphaBlendOp));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(alphaSrcBlend));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(alphaDestBlend));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(colorWriteMask));
        return hash;
    }

    /// @brief Streams all fields into a `gp::hashing` streaming accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(colorBlendOp);
        hasher.append(colorSrcBlend);
        hasher.append(colorDestBlend);
        hasher.append(alphaBlendOp);
        hasher.append(alphaSrcBlend);
        hasher.append(alphaDestBlend);
        hasher.append(colorWriteMask);
    }
};

static_assert(
    std::has_unique_object_representations_v<RenderTargetBlendDesc>,
    "RenderTargetBlendDesc must have zero compiler padding."
);

/// @brief Creation parameters describing color blending across all simultaneous render targets.
struct BlendStateDesc
{
    /// @brief Alias for per-render-target blend configuration.
    using RenderTarget = RenderTargetBlendDesc;

    /// @brief Per-slot blend configurations indexed `[0, kMaxSimultaneousRenderTargets - 1]`.
    gp::Array<RenderTargetBlendDesc, kMaxSimultaneousRenderTargets> renderTargets{};

    /// @brief When true, each render target uses its own slot in `renderTargets`; when false, slot `0` applies to all.
    bool useIndependentRenderTargetBlendStates{ false };

    /// @brief Enables alpha-to-coverage multisampling mask conversion on render target `0`.
    bool useAlphaToCoverage{ false };

public:
    /// @brief Constructs a default opaque blend state descriptor.
    constexpr BlendStateDesc() noexcept = default;

    /// @brief Constructs a blend state applying a single render target configuration across all bound targets.
    /// @param[in] inRenderTargetBlendState Blend configuration copied into slot `0`.
    /// @param[in] inUseAlphaToCoverage True to enable alpha-to-coverage.
    constexpr explicit(false) BlendStateDesc(
        const RenderTargetBlendDesc& inRenderTargetBlendState, const bool inUseAlphaToCoverage = false
    ) noexcept
        : useIndependentRenderTargetBlendStates(false)
        , useAlphaToCoverage(inUseAlphaToCoverage)
    {
        renderTargets[0] = inRenderTargetBlendState;
    }

    /// @brief Constructs a blend state from a fixed array of per-target blend configurations.
    /// @details Sets `useIndependentRenderTargetBlendStates` to `true` automatically when `InNumRenderTargets > 1`.
    /// @tparam InNumRenderTargets Number of render target descriptors supplied (`<= kMaxSimultaneousRenderTargets`).
    /// @param[in] inRenderTargetBlendStates Array of per-target blend configurations.
    /// @param[in] inUseAlphaToCoverage True to enable alpha-to-coverage.
    template <gp::USize InNumRenderTargets>
    constexpr explicit BlendStateDesc(
        const gp::Array<RenderTargetBlendDesc, InNumRenderTargets>& inRenderTargetBlendStates,
        const bool inUseAlphaToCoverage = false
    ) noexcept
        : useIndependentRenderTargetBlendStates(InNumRenderTargets > 1u)
        , useAlphaToCoverage(inUseAlphaToCoverage)
    {
        static_assert(
            InNumRenderTargets <= kMaxSimultaneousRenderTargets, "BlendStateDesc exceeds kMaxSimultaneousRenderTargets."
        );

        for (gp::USize i = 0u; i < InNumRenderTargets; ++i)
        {
            renderTargets[i] = inRenderTargetBlendStates[i];
        }
    }

    /// @brief Compares two blend state descriptors across flags and every render target slot.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when flags and all `kMaxSimultaneousRenderTargets` slots match.
    [[nodiscard]] constexpr bool operator==(const BlendStateDesc& rhs) const noexcept
    {
        if (useIndependentRenderTargetBlendStates != rhs.useIndependentRenderTargetBlendStates ||
            useAlphaToCoverage != rhs.useAlphaToCoverage)
        {
            return false;
        }

        for (gp::USize i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            if (renderTargets[i] != rhs.renderTargets[i])
            {
                return false;
            }
        }

        return true;
    }

    /// @brief Computes a 64-bit hash across flags and all render target slots for state caching.
    /// @return 64-bit hash value.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashOf(useIndependentRenderTargetBlendStates);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(useAlphaToCoverage));
        for (gp::USize i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            hash = gp::hashing::hashCombine(hash, renderTargets[i].getHash());
        }
        return hash;
    }

    /// @brief Streams all flags and render target slots into a `gp::hashing` streaming accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(useIndependentRenderTargetBlendStates);
        hasher.append(useAlphaToCoverage);
        for (gp::USize i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            renderTargets[i].hashAppend(hasher);
        }
    }
};

}   // namespace gp::rhi::pipeline
