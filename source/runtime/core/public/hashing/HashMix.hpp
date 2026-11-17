// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include <bit>
#include <limits>
#include <type_traits>

#if GP_COMPILER_MSVC && GP_ARCHITECTURE_64BIT && !GP_COMPILER_CLANG
    #include <intrin.h>
#endif

namespace gp::hashing
{

/// @brief Applies the MurmurHash3 32-bit finalizer (`fmix32`) to break integer clustering.
/// @details Sequential integer IDs (entity slots, bone indices, draw call keys) cluster in power-of-two tables if used
///          unmixed. Three shift-XOR-multiply rounds spread bit changes across all 32 output bits.
/// @param[in] value Raw 32-bit integer value.
/// @return Avalanche-mixed 32-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 avalancheMix32(gp::UInt32 value) noexcept
{
    value ^= value >> 16u;
    value *= 0x85EBCA6Bu;
    value ^= value >> 13u;
    value *= 0xC2B2AE35u;
    value ^= value >> 16u;
    return value;
}

/// @brief Applies the MurmurHash3 64-bit finalizer (`fmix64`) to break integer clustering.
/// @details Used for handle and packed-identifier hashing (including `gp::Name::hash()`), where sequential IDs would
///          otherwise collide in open-addressed hash tables.
/// @param[in] value Raw 64-bit integer value.
/// @return Avalanche-mixed 64-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 avalancheMix64(gp::UInt64 value) noexcept
{
    value ^= value >> 33u;
    value *= 0xFF51AFD7ED558CCDULL;
    value ^= value >> 33u;
    value *= 0xC4CEB9FE1A85EC53ULL;
    value ^= value >> 33u;
    return value;
}

/// @brief Applies Stafford's Mix13 (`SplitMix64`) 64-bit bijective integer mixer.
/// @details Offers stronger bit independence than `fmix64` on sequential 64-bit integers and aligned pointer addresses.
///          Maps `0` to `0`, which suits pointer and sentinel hashing.
/// @param[in] value Raw 64-bit integer value.
/// @return Mixed 64-bit hash (`0` when `value == 0`).
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 splitMix64(gp::UInt64 value) noexcept
{
    value ^= value >> 30u;
    value *= 0xBF58476D1CE4E5B9ULL;
    value ^= value >> 27u;
    value *= 0x94D049BB133111EBULL;
    value ^= value >> 31u;
    return value;
}

/// @brief Applies Chris Wellons' `triple32` low-bias 32-bit reversible integer mixer.
/// @details Outperforms single-round shift-multiply mixers on sequential indices while remaining bijective on 32-bit
///          words (`0` maps to `0`).
/// @param[in] value Raw 32-bit integer value.
/// @return Mixed 32-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 prospectorMix32(gp::UInt32 value) noexcept
{
    value ^= value >> 17u;
    value *= 0xED5AD4BBu;
    value ^= value >> 11u;
    value *= 0xAC4C1B51u;
    value ^= value >> 15u;
    value *= 0x31848BABu;
    value ^= value >> 14u;
    return value;
}

/// @brief Reduces two 64-bit words into a single 64-bit hash using CityHash `Hash128to64`.
/// @details Identical to the mixing kernel used by Unreal Engine 5 `HashCombine` and Abseil. Provides full 64-bit
///          avalanche in three 64-bit multiplies without the low-bit weakness of simple golden-ratio XOR-shift.
/// @param[in] low First 64-bit word.
/// @param[in] high Second 64-bit word.
/// @return Combined 64-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
    cityHash128To64(const gp::UInt64 low, const gp::UInt64 high) noexcept
{
    constexpr gp::UInt64 kMul = 0x9DDFEA08EB382D69ULL;
    gp::UInt64 a = (low ^ high) * kMul;
    a ^= (a >> 47u);
    gp::UInt64 b = (high ^ a) * kMul;
    b ^= (b >> 47u);
    b *= kMul;
    return b;
}

/// @brief Folds a 64-bit hash down to 32 bits while preserving entropy from both halves.
/// @param[in] hash64 Source 64-bit hash value.
/// @return Mixed 32-bit hash value.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 foldHash64To32(const gp::UInt64 hash64) noexcept
{
    return avalancheMix32(static_cast<gp::UInt32>(hash64 ^ (hash64 >> 32u)));
}

/// @brief Normalizes IEEE-754 `Float32` bit patterns so `-0.0f` and all NaNs map canonically.
/// @details In IEEE-754, `-0.0f == +0.0f` is true even though their sign bits differ (`0x80000000` vs `0x00000000`).
///          Canonicalizing `-0.0f` to `0u` and any NaN payload to `0x7FC00000u` preserves hash-table equality rules.
/// @param[in] value Floating-point value to normalize.
/// @return Canonical 32-bit IEEE-754 representation.
[[nodiscard]] constexpr gp::UInt32 normalizeFloatBits32(const gp::Float32 value) noexcept
{
    static_assert(sizeof(gp::Float32) == 4u && std::numeric_limits<gp::Float32>::is_iec559, "IEEE-754 float required.");
    if (value == 0.0f)
    {
        return 0u;
    }
    const gp::UInt32 bits = std::bit_cast<gp::UInt32>(value);
    if ((bits & 0x7FFFFFFFu) > 0x7F800000u)
    {
        return 0x7FC00000u;
    }
    return bits;
}

/// @brief Normalizes IEEE-754 `Float64` bit patterns so `-0.0` and all NaNs map canonically.
/// @param[in] value Double-precision value to normalize.
/// @return Canonical 64-bit IEEE-754 representation.
[[nodiscard]] constexpr gp::UInt64 normalizeFloatBits64(const gp::Float64 value) noexcept
{
    static_assert(
        sizeof(gp::Float64) == 8u && std::numeric_limits<gp::Float64>::is_iec559, "IEEE-754 double required."
    );
    if (value == 0.0)
    {
        return 0ULL;
    }
    const gp::UInt64 bits = std::bit_cast<gp::UInt64>(value);
    if ((bits & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ULL)
    {
        return 0x7FF8000000000000ULL;
    }
    return bits;
}

/// @brief Hashes any signed or unsigned 32-bit (or smaller) integer with `prospectorMix32`.
/// @tparam InIntT Integral type up to 32 bits wide.
/// @param[in] value Integer value to hash.
/// @return Mixed 32-bit hash.
template <gp::concepts::IsIntegral InIntT>
requires(sizeof(InIntT) <= 4u)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 hashInt32(const InIntT value) noexcept
{
    using UnsignedT = std::make_unsigned_t<InIntT>;
    return prospectorMix32(static_cast<gp::UInt32>(static_cast<UnsignedT>(value)));
}

/// @brief Hashes any signed or unsigned integer up to 64 bits with `splitMix64`.
/// @tparam InIntT Integral type up to 64 bits wide.
/// @param[in] value Integer value to hash.
/// @return Mixed 64-bit hash.
template <gp::concepts::IsIntegral InIntT>
requires(sizeof(InIntT) <= 8u)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 hashInt64(const InIntT value) noexcept
{
    using UnsignedT = std::make_unsigned_t<InIntT>;
    return splitMix64(static_cast<gp::UInt64>(static_cast<UnsignedT>(value)));
}

/// @brief Hashes a `Float32` value deterministically after normalizing `-0.0f` and NaNs.
/// @param[in] value Single-precision float to hash.
/// @return Mixed 64-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 hashFloat(const gp::Float32 value) noexcept
{
    return splitMix64(static_cast<gp::UInt64>(normalizeFloatBits32(value)));
}

/// @brief Hashes a `Float64` value deterministically after normalizing `-0.0` and NaNs.
/// @param[in] value Double-precision float to hash.
/// @return Mixed 64-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 hashDouble(const gp::Float64 value) noexcept
{
    return splitMix64(normalizeFloatBits64(value));
}

/// @brief Computes a deterministic hash of a raw memory address (`0` for `nullptr`).
/// @details Evaluates `nullptr` to `0` at compile time. Non-null pointers mix every address bit with `splitMix64` at
///          runtime without an `if consteval` shortcut so `const gp::UInt64 h = hashPointer(&local)` always runs
///          the real runtime mixer instead of collapsing to a constant sentinel.
/// @param[in] ptr Raw pointer address to hash.
/// @return 64-bit mixed address hash, or `0` if @p ptr is `nullptr`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 hashPointer(const void* const ptr) noexcept
{
    if (ptr == nullptr)
    {
        return 0ULL;
    }
    return splitMix64(static_cast<gp::UInt64>(reinterpret_cast<gp::UIntPtr>(ptr)));
}

/// @brief Hashes 2D integer grid coordinates for spatial hash tables, chunk maps, and broadphase grids.
/// @details Uses Teschner's prime-XOR spatial hash followed by `avalancheMix32` to break axis-aligned clustering.
/// @param[in] x Integer X cell coordinate.
/// @param[in] y Integer Y cell coordinate.
/// @return 32-bit mixed spatial hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 hashGrid2D(const gp::Int32 x, const gp::Int32 y) noexcept
{
    const gp::UInt32 ux = static_cast<gp::UInt32>(x) * 73856093u;
    const gp::UInt32 uy = static_cast<gp::UInt32>(y) * 19349663u;
    return avalancheMix32(ux ^ uy);
}

/// @brief Hashes 3D integer voxel or broadphase coordinates into a 32-bit cell hash.
/// @details Uses Teschner's large-prime XOR combination (`73856093`, `19349663`, `83492791`) followed by
///          `avalancheMix32`. Unsigned arithmetic prevents signed-overflow UB on negative world coordinates.
/// @param[in] x Integer X cell coordinate.
/// @param[in] y Integer Y cell coordinate.
/// @param[in] z Integer Z cell coordinate.
/// @return 32-bit mixed spatial hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
    hashGrid3D(const gp::Int32 x, const gp::Int32 y, const gp::Int32 z) noexcept
{
    const gp::UInt32 ux = static_cast<gp::UInt32>(x) * 73856093u;
    const gp::UInt32 uy = static_cast<gp::UInt32>(y) * 19349663u;
    const gp::UInt32 uz = static_cast<gp::UInt32>(z) * 83492791u;
    return avalancheMix32(ux ^ uy ^ uz);
}

/// @brief Hashes 2D integer grid coordinates into a 64-bit spatial cell hash.
/// @param[in] x Integer X cell coordinate.
/// @param[in] y Integer Y cell coordinate.
/// @return 64-bit mixed spatial hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 hashGrid2D64(const gp::Int32 x, const gp::Int32 y) noexcept
{
    const gp::UInt64 packed = (static_cast<gp::UInt64>(static_cast<gp::UInt32>(x)) << 32u) |
                              static_cast<gp::UInt64>(static_cast<gp::UInt32>(y));
    return splitMix64(packed);
}

/// @brief Hashes 3D integer voxel coordinates into a 64-bit spatial cell hash.
/// @param[in] x Integer X cell coordinate.
/// @param[in] y Integer Y cell coordinate.
/// @param[in] z Integer Z cell coordinate.
/// @return 64-bit mixed spatial hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
    hashGrid3D64(const gp::Int32 x, const gp::Int32 y, const gp::Int32 z) noexcept
{
    const gp::UInt64 xy = hashGrid2D64(x, y);
    const gp::UInt64 hz = splitMix64(static_cast<gp::UInt64>(static_cast<gp::UInt32>(z)) ^ 0x9E3779B97F4A7C15ULL);
    return cityHash128To64(xy, hz);
}

/// @brief Maps a uniform 32-bit hash into `[0, bucketCount)` without integer division (Lemire `fastrange`).
/// @details Replaces `hash % bucketCount` with `(UInt64(hash) * bucketCount) >> 32`. Assumes the upper bits of @p hash
///          have good dispersion (true for Murmur3, XXHash, SplitMix, and `avalancheMix`). Constraining by width
///          prevents 64-bit `unsigned long` vs `unsigned long long` overload ambiguity across Windows and Linux LP64.
/// @tparam InHashT 32-bit unsigned integral hash type.
/// @tparam InBucketT Unsigned integral bucket count type.
/// @param[in] hash Uniform 32-bit hash value.
/// @param[in] bucketCount Number of target buckets (`0` safely returns `0`).
/// @return Bucket index in `[0, bucketCount)`.
template <gp::concepts::IsUnsignedIntegral InHashT, gp::concepts::IsUnsignedIntegral InBucketT>
requires(sizeof(InHashT) == 4u) [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
    mapToBucket(const InHashT hash, const InBucketT bucketCount) noexcept
{
    if (bucketCount == 0u)
    {
        return 0u;
    }
    if constexpr (sizeof(InBucketT) > 4u)
    {
        if (static_cast<gp::UInt64>(bucketCount) > 0xFFFFFFFFULL)
        {
            return static_cast<gp::UInt32>(hash);
        }
    }
    return static_cast<gp::UInt32>((static_cast<gp::UInt64>(hash) * static_cast<gp::UInt64>(bucketCount)) >> 32u);
}

/// @brief Maps a uniform 64-bit hash into `[0, bucketCount)` without integer division (64-bit Lemire `fastrange`).
/// @details Computes the high 64 bits of the unsigned 128-bit product `hash * bucketCount`, using compiler 128-bit
///          intrinsics at runtime and portable 32-bit limbs during `constexpr` evaluation. Accepts any 64-bit unsigned
///          type (`gp::UInt64`, `gp::USize`, `unsigned long long`) identically on Windows, Linux, and macOS.
/// @tparam InHashT 64-bit unsigned integral hash type.
/// @tparam InBucketT Unsigned integral bucket count type.
/// @param[in] hash Uniform 64-bit hash value.
/// @param[in] bucketCount Number of target buckets (`0` safely returns `0`).
/// @return Bucket index in `[0, bucketCount)`.
template <gp::concepts::IsUnsignedIntegral InHashT, gp::concepts::IsUnsignedIntegral InBucketT>
requires(sizeof(InHashT) == 8u)
[[nodiscard]] constexpr gp::UInt64 mapToBucket(const InHashT hash, const InBucketT bucketCount) noexcept
{
    const gp::UInt64 h = static_cast<gp::UInt64>(hash);
    const gp::UInt64 n = static_cast<gp::UInt64>(bucketCount);
    if (n == 0ULL)
    {
        return 0ULL;
    }

    if (!std::is_constant_evaluated())
    {
#if defined(__SIZEOF_INT128__)
        return static_cast<gp::UInt64>((static_cast<unsigned __int128>(h) * static_cast<unsigned __int128>(n)) >> 64u);
#elif GP_COMPILER_MSVC && GP_ARCHITECTURE_64BIT
        gp::UInt64 high = 0ULL;
        (void)_umul128(h, n, &high);
        return high;
#endif
    }

    const gp::UInt64 aLo = h & 0xFFFFFFFFULL;
    const gp::UInt64 aHi = h >> 32u;
    const gp::UInt64 bLo = n & 0xFFFFFFFFULL;
    const gp::UInt64 bHi = n >> 32u;

    const gp::UInt64 loLo = aLo * bLo;
    const gp::UInt64 loHi = aLo * bHi;
    const gp::UInt64 hiLo = aHi * bLo;
    const gp::UInt64 hiHi = aHi * bHi;

    const gp::UInt64 cross = (loLo >> 32u) + (loHi & 0xFFFFFFFFULL) + (hiLo & 0xFFFFFFFFULL);
    return hiHi + (loHi >> 32u) + (hiLo >> 32u) + (cross >> 32u);
}

}   // namespace gp::hashing
