// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashOf.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include <type_traits>

namespace gp::rhi::pipeline
{

/// @brief Creation parameters describing depth testing, depth writes, and front/back stencil operations.
struct DepthStencilStateDesc
{
    /// @brief Enables writing depth values to the depth buffer when the depth test passes.
    bool enableDepthWrite{ true };

    /// @brief Comparison function applied to incoming fragment depth against the depth buffer.
    CompareFunction depthTest{ CompareFunction::LessEqual };

    /// @brief Enables stencil testing for front-facing primitives (and back faces when two-sided stencil is off).
    bool enableFrontFaceStencil{ false };

    /// @brief Comparison function for front-face stencil testing.
    CompareFunction frontFaceStencilTest{ CompareFunction::Always };

    /// @brief Stencil operation executed when the front-face stencil test fails.
    StencilOp frontFaceStencilFailOp{ StencilOp::Keep };

    /// @brief Stencil operation executed when front-face stencil passes and the depth test fails.
    StencilOp frontFaceDepthFailOp{ StencilOp::Keep };

    /// @brief Stencil operation executed when both front-face stencil and depth tests pass.
    StencilOp frontFacePassOp{ StencilOp::Keep };

    /// @brief Enables independent stencil test and operations for back-facing primitives.
    bool enableBackFaceStencil{ false };

    /// @brief Comparison function for back-face stencil testing.
    CompareFunction backFaceStencilTest{ CompareFunction::Always };

    /// @brief Stencil operation executed when the back-face stencil test fails.
    StencilOp backFaceStencilFailOp{ StencilOp::Keep };

    /// @brief Stencil operation executed when back-face stencil passes and the depth test fails.
    StencilOp backFaceDepthFailOp{ StencilOp::Keep };

    /// @brief Stencil operation executed when both back-face stencil and depth tests pass.
    StencilOp backFacePassOp{ StencilOp::Keep };

    /// @brief Bitmask applied to stencil buffer values and reference values before comparison.
    gp::UInt8 stencilReadMask{ 0xFFu };

    /// @brief Bitmask controlling which bits of the stencil buffer may be modified on write.
    gp::UInt8 stencilWriteMask{ 0xFFu };

public:
    /// @brief Constructs a depth-stencil descriptor with explicit depth and two-sided stencil parameters.
    /// @param[in] inEnableDepthWrite Enables depth writes.
    /// @param[in] inDepthTest Depth comparison function.
    /// @param[in] inEnableFrontFaceStencil Enables front-face stencil test.
    /// @param[in] inFrontFaceStencilTest Front-face stencil comparison function.
    /// @param[in] inFrontFaceStencilFailOp Front-face stencil-fail operation.
    /// @param[in] inFrontFaceDepthFailOp Front-face depth-fail operation.
    /// @param[in] inFrontFacePassOp Front-face pass operation.
    /// @param[in] inEnableBackFaceStencil Enables separate back-face stencil test.
    /// @param[in] inBackFaceStencilTest Back-face stencil comparison function.
    /// @param[in] inBackFaceStencilFailOp Back-face stencil-fail operation.
    /// @param[in] inBackFaceDepthFailOp Back-face depth-fail operation.
    /// @param[in] inBackFacePassOp Back-face pass operation.
    /// @param[in] inStencilReadMask Stencil read bitmask.
    /// @param[in] inStencilWriteMask Stencil write bitmask.
    constexpr DepthStencilStateDesc(
        const bool inEnableDepthWrite = true,
        const CompareFunction inDepthTest = CompareFunction::LessEqual,
        const bool inEnableFrontFaceStencil = false,
        const CompareFunction inFrontFaceStencilTest = CompareFunction::Always,
        const StencilOp inFrontFaceStencilFailOp = StencilOp::Keep,
        const StencilOp inFrontFaceDepthFailOp = StencilOp::Keep,
        const StencilOp inFrontFacePassOp = StencilOp::Keep,
        const bool inEnableBackFaceStencil = false,
        const CompareFunction inBackFaceStencilTest = CompareFunction::Always,
        const StencilOp inBackFaceStencilFailOp = StencilOp::Keep,
        const StencilOp inBackFaceDepthFailOp = StencilOp::Keep,
        const StencilOp inBackFacePassOp = StencilOp::Keep,
        const gp::UInt8 inStencilReadMask = 0xFFu,
        const gp::UInt8 inStencilWriteMask = 0xFFu
    ) noexcept
        : enableDepthWrite(inEnableDepthWrite)
        , depthTest(inDepthTest)
        , enableFrontFaceStencil(inEnableFrontFaceStencil)
        , frontFaceStencilTest(inFrontFaceStencilTest)
        , frontFaceStencilFailOp(inFrontFaceStencilFailOp)
        , frontFaceDepthFailOp(inFrontFaceDepthFailOp)
        , frontFacePassOp(inFrontFacePassOp)
        , enableBackFaceStencil(inEnableBackFaceStencil)
        , backFaceStencilTest(inBackFaceStencilTest)
        , backFaceStencilFailOp(inBackFaceStencilFailOp)
        , backFaceDepthFailOp(inBackFaceDepthFailOp)
        , backFacePassOp(inBackFacePassOp)
        , stencilReadMask(inStencilReadMask)
        , stencilWriteMask(inStencilWriteMask)
    {}

    /// @brief Compares two depth-stencil descriptors field by field.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const DepthStencilStateDesc& rhs) const noexcept = default;

    /// @brief Computes a 64-bit hash across all depth and stencil fields for state caching.
    /// @return 64-bit hash value.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashOf(enableDepthWrite);
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(depthTest));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(enableFrontFaceStencil));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(frontFaceStencilTest));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(frontFaceStencilFailOp));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(frontFaceDepthFailOp));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(frontFacePassOp));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(enableBackFaceStencil));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(backFaceStencilTest));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(backFaceStencilFailOp));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(backFaceDepthFailOp));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(backFacePassOp));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(stencilReadMask));
        hash = gp::hashing::hashCombine(hash, gp::hashing::hashOf(stencilWriteMask));
        return hash;
    }

    /// @brief Streams all descriptor fields into a `gp::hashing` streaming accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(enableDepthWrite);
        hasher.append(depthTest);
        hasher.append(enableFrontFaceStencil);
        hasher.append(frontFaceStencilTest);
        hasher.append(frontFaceStencilFailOp);
        hasher.append(frontFaceDepthFailOp);
        hasher.append(frontFacePassOp);
        hasher.append(enableBackFaceStencil);
        hasher.append(backFaceStencilTest);
        hasher.append(backFaceStencilFailOp);
        hasher.append(backFaceDepthFailOp);
        hasher.append(backFacePassOp);
        hasher.append(stencilReadMask);
        hasher.append(stencilWriteMask);
    }

    /// @brief Serializes all 14 depth and stencil fields into a `<...>` token sequence.
    /// @return Serialized string representation.
    [[nodiscard]] GP_RHI_API gp::String toString() const noexcept;

    /// @brief Parses a 14-token `<...>` or whitespace-delimited sequence into this depth-stencil descriptor.
    /// @param[in] src Serialized depth-stencil descriptor text.
    /// @return True when all fields parsed within valid bounds; false if `src` is malformed (leaves `*this` unchanged).
    GP_RHI_API bool fromString(gp::StringView src) noexcept;
};

static_assert(
    std::has_unique_object_representations_v<DepthStencilStateDesc>,
    "DepthStencilStateDesc must have zero compiler padding."
);

}   // namespace gp::rhi::pipeline
