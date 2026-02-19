// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Generic.hpp"

namespace GP::Crypto
{

/// \brief Procedural noise hash functions commonly used in game development.
/// Provides integer-to-integer hashing for procedural generation, noise functions, and random seeding.
struct NoiseHash final : public Template::NonInstantiable
{
public:
    /// \brief Squirrel Eiserloh's Squirrel3 noise hash (GDC 2017).
    /// Fast, high-quality 1D integer hash for procedural noise and seeded random number generation.
    /// \param position Input position/index to hash.
    /// \param seed Additional seed value for variation.
    /// \return Pseudo-random 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Squirrel3(Int32 position, UInt32 seed = 0) noexcept
    {
        constexpr UInt32 kNoise1 = 0xB5297A4DU;
        constexpr UInt32 kNoise2 = 0x68E31DA4U;
        constexpr UInt32 kNoise3 = 0x1B56C4E9U;

        UInt32 mangled = static_cast<UInt32>(position);
        mangled *= kNoise1;
        mangled += seed;
        mangled ^= (mangled >> 8);
        mangled += kNoise2;
        mangled ^= (mangled << 8);
        mangled *= kNoise3;
        mangled ^= (mangled >> 8);
        return mangled;
    }

    /// \brief Squirrel Eiserloh's Squirrel5 noise hash (updated variant with better avalanche).
    /// \param position Input position/index to hash.
    /// \param seed Additional seed value for variation.
    /// \return Pseudo-random 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Squirrel5(Int32 position, UInt32 seed = 0) noexcept
    {
        constexpr UInt32 kBit1 = 0xD2A80A3FU;
        constexpr UInt32 kBit2 = 0xA884F197U;
        constexpr UInt32 kBit3 = 0x6C736F4BU;
        constexpr UInt32 kBit4 = 0xB79F3ABBU;
        constexpr UInt32 kBit5 = 0x1B56C4E9U;

        UInt32 mangled = static_cast<UInt32>(position);
        mangled *= kBit1;
        mangled += seed;
        mangled ^= (mangled >> 9);
        mangled += kBit2;
        mangled ^= (mangled >> 11);
        mangled *= kBit3;
        mangled ^= (mangled >> 13);
        mangled += kBit4;
        mangled ^= (mangled >> 15);
        mangled *= kBit5;
        mangled ^= (mangled >> 17);
        return mangled;
    }

    /// \brief Thomas Wang's 32-bit integer hash.
    /// Excellent avalanche properties for scrambling sequential keys.
    /// \param key Input key to hash.
    /// \return Hashed 32-bit value.
    GP_NODISCARD static constexpr UInt32 WangHash32(UInt32 key) noexcept
    {
        key = ~key + (key << 15);
        key ^= (key >> 12);
        key += (key << 2);
        key ^= (key >> 4);
        key *= 2057;
        key ^= (key >> 16);
        return key;
    }

    /// \brief Thomas Wang's 64-bit integer hash.
    /// \param key Input key to hash.
    /// \return Hashed 64-bit value.
    GP_NODISCARD static constexpr UInt64 WangHash64(UInt64 key) noexcept
    {
        key = (~key) + (key << 21);
        key ^= (key >> 24);
        key = (key + (key << 3)) + (key << 8);
        key ^= (key >> 14);
        key = (key + (key << 2)) + (key << 4);
        key ^= (key >> 28);
        key += (key << 31);
        return key;
    }

    /// \brief PCG-derived 32-bit integer hash for procedural noise.
    /// Based on the PCG family of random number generators by Melissa O'Neill.
    /// \param input Input value to hash.
    /// \return Pseudo-random 32-bit value.
    GP_NODISCARD static constexpr UInt32 PCGHash32(UInt32 input) noexcept
    {
        UInt32 state = input * 747796405U + 2891336453U;
        UInt32 word = ((state >> ((state >> 28) + 4)) ^ state) * 277803737U;
        return (word >> 22) ^ word;
    }

    /// \brief 2D coordinate noise hash for grid-based procedural generation.
    /// \param x X coordinate.
    /// \param y Y coordinate.
    /// \param seed Additional seed value.
    /// \return Pseudo-random 32-bit value for the coordinate.
    GP_NODISCARD static constexpr UInt32 Noise2D(Int32 x, Int32 y, UInt32 seed = 0) noexcept
    {
        constexpr Int32 kPrime = 198491317;
        return Squirrel5(x + (kPrime * y), seed);
    }

    /// \brief 3D coordinate noise hash for volumetric procedural generation.
    /// \param x X coordinate.
    /// \param y Y coordinate.
    /// \param z Z coordinate.
    /// \param seed Additional seed value.
    /// \return Pseudo-random 32-bit value for the coordinate.
    GP_NODISCARD static constexpr UInt32 Noise3D(Int32 x, Int32 y, Int32 z, UInt32 seed = 0) noexcept
    {
        constexpr Int32 kPrimeY = 198491317;
        constexpr Int32 kPrimeZ = 6542989;
        return Squirrel5(x + (kPrimeY * y) + (kPrimeZ * z), seed);
    }

    /// \brief 4D coordinate noise hash for spatio-temporal procedural generation.
    /// \param x X coordinate.
    /// \param y Y coordinate.
    /// \param z Z coordinate.
    /// \param w W coordinate (e.g., time).
    /// \param seed Additional seed value.
    /// \return Pseudo-random 32-bit value for the coordinate.
    GP_NODISCARD static constexpr UInt32 Noise4D(Int32 x, Int32 y, Int32 z, Int32 w, UInt32 seed = 0) noexcept
    {
        constexpr Int32 kPrimeY = 198491317;
        constexpr Int32 kPrimeZ = 6542989;
        constexpr Int32 kPrimeW = 357239;
        return Squirrel5(x + (kPrimeY * y) + (kPrimeZ * z) + (kPrimeW * w), seed);
    }

    /// \brief Convert a hash value to a normalized float in [0, 1).
    /// \param hash Input hash value.
    /// \return Float in the range [0.0, 1.0).
    /// \note Shifts to 24-bit precision before the float cast to avoid the rounding of 0xFFFFFFFF
    ///       up to 2^32 in float32, which would produce 1.0 when divided by 2^32.
    GP_NODISCARD static constexpr Float32 ToFloat01(UInt32 hash) noexcept
    {
        return static_cast<Float32>(hash >> 8) * (1.0f / 16777216.0f);
    }

    /// \brief Convert a hash value to a normalized float in [-1, 1).
    /// \param hash Input hash value.
    /// \return Float in the range [-1.0, 1.0).
    GP_NODISCARD static constexpr Float32 ToFloatNeg11(UInt32 hash) noexcept { return ToFloat01(hash) * 2.0f - 1.0f; }
};

}   // namespace GP::Crypto
