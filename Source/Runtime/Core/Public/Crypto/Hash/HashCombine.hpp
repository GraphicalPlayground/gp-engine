// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Generic.hpp"

namespace GP::Crypto
{

/// \brief Utilities to combine multiple hash values into a single hash.
struct HashCombine final : public Template::NonInstantiable
{
public:
#if GP_ARCHITECTURE_64BIT
    using HashType = UInt64;                                         //<! 64-bit hash type.
    static constexpr HashType GoldenRatio = 0x9E3779B97F4A7C15ULL;   //<! 64-bit golden ratio constant for mixing.
#else
    using HashType = UInt32;                               //<! 32-bit hash type.
    static constexpr HashType GoldenRatio = 0x9E3779B9U;   //<! 32-bit golden ratio constant for mixing.
#endif

public:
    /// \brief Combine two hash values into one.
    /// \param seed Running hash value.
    /// \param value New hash value to mix in.
    /// \return Combined hash.
    GP_NODISCARD static constexpr HashType Combine(HashType seed, HashType value) noexcept
    {
        seed ^= value + GoldenRatio + (seed << 6) + (seed >> 2);
        return seed;
    }

    /// \brief Combine multiple values into a single hash (variadic).
    /// \tparam Args Types of values to combine (will be cast to HashType).
    /// \param seed Initial seed value.
    /// \param values Values to fold into the seed.
    /// \return Final combined hash.
    template <typename... Args>
    GP_NODISCARD static constexpr HashType Multiple(HashType seed, Args... values) noexcept
    {
        ((seed = Combine(seed, static_cast<HashType>(values))), ...);
        return seed;
    }

    /// \brief 32-bit specific combine helper.
    /// \param seed Running hash value.
    /// \param value New hash value to mix in.
    /// \return Combined hash.
    GP_NODISCARD static constexpr UInt32 Combine32(UInt32 seed, UInt32 value) noexcept
    {
        seed ^= value + 0x9E3779B9U + (seed << 6) + (seed >> 2);
        return seed;
    }

    /// \brief 64-bit specific combine helper.
    /// \param seed Running hash value.
    /// \param value New hash value to mix in.
    /// \return Combined hash.
    GP_NODISCARD static constexpr UInt64 Combine64(UInt64 seed, UInt64 value) noexcept
    {
        seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
};

}   // namespace GP::Crypto
