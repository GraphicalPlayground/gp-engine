// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashMix.hpp"
#include "hashing/HashOf.hpp"
#include "maths/base/FloatingPoint.hpp"
#include "maths/color/LinearColor.hpp"
#include "templates/NumericLimits.hpp"
#include <bit>

namespace gp::rhi::resource
{

/// @brief Tagged hardware fast-clear value bound to a color or depth-stencil render target.
/// @details Stores a `ClearBinding` discriminator alongside a union of either four `Float32` RGBA channels or a
///          `(depth, stencil)` pair. Color equality and hashing compare the exact IEEE-754 bit patterns so special
///          sentinel clear values (including negative zero or bit-packed NaNs) match reliably. Depth-stencil equality
///          and hashing use floating-point semantics where `-0.0f` equals `+0.0f`.
struct ClearValueBinding
{
    /// @brief Depth and stencil pair stored when `colorBinding == ClearBinding::DepthStencilBound`.
    struct DepthStencilValue
    {
        /// @brief Floating-point depth clear value.
        gp::Float32 depth{ 0.0f };

        /// @brief Integer stencil clear value.
        gp::UInt32 stencil{ 0u };
    };

    /// @brief Active payload storage matching `colorBinding`.
    union ClearValue
    {
        /// @brief Four-channel linear RGBA clear color (`ColorBound`).
        gp::Float32 color[4];

        /// @brief Depth and stencil clear pair (`DepthStencilBound`).
        DepthStencilValue depthStencil;

        /// @brief Initializes all four RGBA channels to zero.
        constexpr ClearValue() noexcept
            : color{ 0.0f, 0.0f, 0.0f, 0.0f }
        {}

        /// @brief Initializes the RGBA channels from `inColor`.
        /// @param[in] inColor Linear RGBA clear color.
        constexpr explicit ClearValue(const gp::math::LinearColor& inColor) noexcept
            : color{ inColor.r, inColor.g, inColor.b, inColor.a }
        {}

        /// @brief Initializes the depth-stencil payload.
        /// @param[in] inDepth Floating-point depth value.
        /// @param[in] inStencil Integer stencil value.
        constexpr ClearValue(const gp::Float32 inDepth, const gp::UInt32 inStencil) noexcept
            : depthStencil{ inDepth, inStencil }
        {}
    };

public:
    /// @brief Discriminator indicating which union member is active.
    ClearBinding colorBinding{ ClearBinding::ColorBound };

    /// @brief Bound color or depth-stencil clear payload.
    ClearValue value{};

public:
    /// @brief Preset representing no bound clear value.
    static const ClearValueBinding kNone;

    /// @brief Preset opaque black `(0, 0, 0, 1)`.
    static const ClearValueBinding kBlack;

    /// @brief Preset black with maximum finite alpha `(0, 0, 0, FLT_MAX)`.
    static const ClearValueBinding kBlackMaxAlpha;

    /// @brief Preset opaque white `(1, 1, 1, 1)`.
    static const ClearValueBinding kWhite;

    /// @brief Preset transparent black `(0, 0, 0, 0)`.
    static const ClearValueBinding kTransparent;

    /// @brief Preset depth `1.0f`, stencil `0`.
    static const ClearValueBinding kDepthOne;

    /// @brief Preset depth `0.0f`, stencil `0`.
    static const ClearValueBinding kDepthZero;

    /// @brief Preset reversed-Z near depth (`1.0f`), stencil `0`.
    static const ClearValueBinding kDepthNear;

    /// @brief Preset reversed-Z far depth (`0.0f`), stencil `0`.
    static const ClearValueBinding kDepthFar;

    /// @brief Preset opaque green `(0, 1, 0, 1)`.
    static const ClearValueBinding kGreen;

    /// @brief Preset 8-bit neutral normal `(128/255, 128/255, 128/255, 1)` that decodes to zero.
    static const ClearValueBinding kDefaultNormal8Bit;

public:
    /// @brief Constructs a color-bound transparent black clear value `(0, 0, 0, 0)`.
    constexpr ClearValueBinding() noexcept = default;

    /// @brief Constructs an unbound clear value (`ClearBinding::NoneBound`).
    /// @param[in] noBinding Must be `ClearBinding::NoneBound`.
    constexpr explicit(false) ClearValueBinding(const ClearBinding noBinding) noexcept
        : colorBinding(noBinding)
        , value()
    {
        if !consteval
        {
            GP_CHECKF(
                colorBinding == ClearBinding::NoneBound,
                "ClearValueBinding(ClearBinding) only accepts ClearBinding::NoneBound."
            );
        }
    }

    /// @brief Constructs a color-bound clear value from a linear RGBA color.
    /// @param[in] inClearColor Linear RGBA clear color.
    constexpr explicit ClearValueBinding(const gp::math::LinearColor& inClearColor) noexcept
        : colorBinding(ClearBinding::ColorBound)
        , value(inClearColor)
    {}

    /// @brief Constructs a depth-stencil-bound clear value.
    /// @param[in] depthClearValue Depth clear value (must not be NaN).
    /// @param[in] stencilClearValue Stencil clear value (defaults to `0`).
    constexpr explicit ClearValueBinding(
        const gp::Float32 depthClearValue, const gp::UInt32 stencilClearValue = 0u
    ) noexcept
        : colorBinding(ClearBinding::DepthStencilBound)
        , value(depthClearValue, stencilClearValue)
    {
        if !consteval
        {
            GP_ENSURE_MSG(!gp::math::isNaN(depthClearValue), "Depth clear value must not be NaN.");
        }
    }

    /// @brief Returns the bound linear RGBA clear color.
    /// @note Triggers an ensure if `colorBinding != ClearBinding::ColorBound`.
    /// @return Linear RGBA clear color.
    [[nodiscard]] constexpr gp::math::LinearColor getClearColor() const noexcept
    {
        if !consteval
        {
            GP_ENSURE(colorBinding == ClearBinding::ColorBound);
        }
        return gp::math::LinearColor(value.color[0], value.color[1], value.color[2], value.color[3]);
    }

    /// @brief Reads the bound depth and stencil clear values.
    /// @note Triggers an ensure if `colorBinding != ClearBinding::DepthStencilBound`.
    /// @param[out] outDepth Receives the depth clear value.
    /// @param[out] outStencil Receives the stencil clear value.
    constexpr void getDepthStencil(gp::Float32& outDepth, gp::UInt32& outStencil) const noexcept
    {
        if !consteval
        {
            GP_ENSURE(colorBinding == ClearBinding::DepthStencilBound);
        }
        outDepth = value.depthStencil.depth;
        outStencil = value.depthStencil.stencil;
    }

    /// @brief Compares two clear value bindings for equality.
    /// @details Uses bitwise comparison for `ColorBound` so bit-packed float sentinels (such as NaN payloads or
    ///          negative zero) compare by exact representation, and floating-point comparison for `DepthStencilBound`.
    /// @param[in] other Binding to compare against.
    /// @return True when bindings have the same mode and equal values.
    [[nodiscard]] constexpr bool operator==(const ClearValueBinding& other) const noexcept
    {
        if (colorBinding != other.colorBinding)
        {
            return false;
        }

        if (colorBinding == ClearBinding::ColorBound)
        {
            for (gp::USize i = 0u; i < 4u; ++i)
            {
                if (std::bit_cast<gp::UInt32>(value.color[i]) != std::bit_cast<gp::UInt32>(other.value.color[i]))
                {
                    return false;
                }
            }
            return true;
        }

        if (colorBinding == ClearBinding::DepthStencilBound)
        {
            return (value.depthStencil.depth == other.value.depthStencil.depth) &&
                   (value.depthStencil.stencil == other.value.depthStencil.stencil);
        }

        return true;
    }

    /// @brief Computes a 64-bit hash consistent with `operator==`.
    /// @details Hashes raw color bytes via CRC-32 for `ColorBound` so `-0.0f` and `+0.0f` stay distinct and match
    ///          bitwise equality. Uses normalized float hashing for `DepthStencilBound` so `-0.0f` and `+0.0f` hash
    ///          identically to match floating-point equality.
    /// @return 64-bit hash value.
    [[nodiscard]] constexpr gp::UInt64 getHash() const noexcept
    {
        gp::UInt64 hash = gp::hashing::hashOf(colorBinding);

        if (colorBinding == ClearBinding::ColorBound)
        {
            struct ColorBytes
            {
                gp::UInt8 bytes[sizeof(value.color)];
            };

            const ColorBytes rawBytes = std::bit_cast<ColorBytes>(value.color);
            const gp::UInt32 colorCrc = gp::hashing::Crc32::hash(rawBytes.bytes, sizeof(rawBytes.bytes));
            hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(colorCrc));
        }
        else if (colorBinding == ClearBinding::DepthStencilBound)
        {
            hash = gp::hashing::hashCombine(hash, gp::hashing::hashFloat(value.depthStencil.depth));
            hash = gp::hashing::hashCombine(hash, gp::hashing::hashInt64(value.depthStencil.stencil));
        }

        return hash;
    }

    /// @brief Streams this binding into any `gp::hashing` streaming accumulator consistently with `operator==`.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(colorBinding);

        if (colorBinding == ClearBinding::ColorBound)
        {
            struct ColorBytes
            {
                gp::UInt8 bytes[sizeof(value.color)];
            };

            const ColorBytes rawBytes = std::bit_cast<ColorBytes>(value.color);
            hasher.update(rawBytes.bytes, sizeof(rawBytes.bytes));
        }
        else if (colorBinding == ClearBinding::DepthStencilBound)
        {
            const gp::Float32 normalizedDepth = (value.depthStencil.depth == 0.0f) ? 0.0f : value.depthStencil.depth;
            hasher.append(normalizedDepth);
            hasher.append(value.depthStencil.stencil);
        }
    }
};

inline constexpr ClearValueBinding ClearValueBinding::kNone(ClearBinding::NoneBound);
inline constexpr ClearValueBinding ClearValueBinding::kBlack(gp::math::LinearColor(0.0f, 0.0f, 0.0f, 1.0f));
inline constexpr ClearValueBinding
    ClearValueBinding::kBlackMaxAlpha(gp::math::LinearColor(0.0f, 0.0f, 0.0f, gp::NumericLimits<gp::Float32>::max()));
inline constexpr ClearValueBinding ClearValueBinding::kWhite(gp::math::LinearColor(1.0f, 1.0f, 1.0f, 1.0f));
inline constexpr ClearValueBinding ClearValueBinding::kTransparent(gp::math::LinearColor(0.0f, 0.0f, 0.0f, 0.0f));
inline constexpr ClearValueBinding ClearValueBinding::kDepthOne(1.0f, 0u);
inline constexpr ClearValueBinding ClearValueBinding::kDepthZero(0.0f, 0u);
inline constexpr ClearValueBinding ClearValueBinding::kDepthNear(1.0f, 0u);
inline constexpr ClearValueBinding ClearValueBinding::kDepthFar(0.0f, 0u);
inline constexpr ClearValueBinding ClearValueBinding::kGreen(gp::math::LinearColor(0.0f, 1.0f, 0.0f, 1.0f));
inline constexpr ClearValueBinding ClearValueBinding::kDefaultNormal8Bit(
    gp::math::LinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f)
);

}   // namespace gp::rhi::resource
