// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <bit>

namespace gp::math
{

/// @brief Checks whether an integral value is a power of two.
/// @tparam T Integral type.
/// @param[in] value The value to test.
/// @return @c true if @p value is a strictly positive power of two, @c false otherwise.
template <concepts::IsIntegral T>
[[nodiscard]] constexpr bool isPowerOfTwo(const T value) noexcept
{
    return value > 0 && (value & (value - 1)) == 0;
}

/// @brief Returns the smallest power of two greater than or equal to @p value.
/// @tparam T Integral type.
/// @param[in] value The value to round up.
/// @return Smallest power of two >= @p value, or 1 if @p value is 0.
template <concepts::IsIntegral T>
[[nodiscard]] constexpr T nextPowerOfTwo(T value) noexcept
{
    if (value == 0)
    {
        return 1;
    }
    --value;
    for (USize i = 1; i < sizeof(T) * 8; i *= 2)
    {
        value = static_cast<T>(value | (value >> i));
    }
    return static_cast<T>(value + 1);
}

/// @brief Computes the base 2 logarithm for an unsigned integer value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to compute the logarithm for.
/// @return The base 2 logarithm of @p value, rounded down to the nearest integer. Returns 0 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T floorLog2(const T value) noexcept
{
    if (value == 0)
    {
        return 0;
    }
    return static_cast<T>(std::bit_width(value) - 1);
}

/// @brief Returns the smallest N such that (1 << N) >= value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to compute the ceiling log2 for.
/// @return The ceiling base 2 logarithm. Returns 0 if @p value is 0 or 1.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T ceilLog2(const T value) noexcept
{
    if (value <= 1)
    {
        return 0;
    }
    return static_cast<T>(std::bit_width(static_cast<T>(value - 1)));
}

/// @brief Counts the number of leading zeros in an unsigned integer value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to count leading zeros for.
/// @return The number of leading zero bits in @p value. Returns the bit width of T if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T countLeadingZeros(const T value) noexcept
{
    return static_cast<T>(std::countl_zero(value));
}

/// @brief Counts the number of trailing zeros in an unsigned integer value.
/// @details Uses C++20 `std::countr_zero`, guaranteeing well-defined bit-width results even when @p value is 0.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to count trailing zeros for.
/// @return The number of trailing zero bits starting from bit 0, or the bit width of @c T if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T countTrailingZeros(const T value) noexcept
{
    return static_cast<T>(std::countr_zero(value));
}

/// @brief Counts the number of set bits (population count) in an unsigned integer value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value whose set bits are counted.
/// @return Number of bits set to 1 in @p value.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T countBits(const T value) noexcept
{
    return static_cast<T>(std::popcount(value));
}

/// @brief Rounds the given number up to the next highest power of two.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to round up.
/// @return The next power of two. Returns 1 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T roundUpToPowerOfTwo(const T value) noexcept
{
    return std::bit_ceil(value);
}

/// @brief Rounds the given number down to the nearest power of two.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to round down.
/// @return The nearest power of two below the value. Returns 0 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T roundDownToPowerOfTwo(const T value) noexcept
{
    return std::bit_floor(value);
}

/// @brief Isolates the lowest set bit in @p value.
/// @details Uses two's-complement negation (`x & -x`) to mask off every bit except the least significant `1` bit.
/// @tparam T Unsigned integral type.
/// @param[in] value Input bitmask word.
/// @return Word containing only the least significant set bit of @p value, or 0 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr T isolateLowestSetBit(const T value) noexcept
{
    return static_cast<T>(value & static_cast<T>(T{ 0 } - value));
}

/// @brief Clears the lowest set bit in @p value and returns the updated mask.
/// @details Evaluates `x & (x - 1)` (single `blsr` instruction on x64 BMI1) to strip the lowest `1` bit branchlessly.
/// @tparam T Unsigned integral type.
/// @param[in] value Input bitmask word.
/// @return Copy of @p value with its least significant set bit cleared, or 0 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr T clearLowestSetBit(const T value) noexcept
{
    return static_cast<T>(value & static_cast<T>(value - T{ 1 }));
}

/// @brief Returns a single-bit mask for @p bitIndex.
/// @tparam T Unsigned integral type.
/// @param[in] bitIndex Zero-based bit position (must be less than the bit width of @c T).
/// @return Value with only bit @p bitIndex set.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr T bitMask(const UInt32 bitIndex) noexcept
{
    GP_ASSERT(bitIndex < static_cast<UInt32>(sizeof(T) * 8u) && "Bit index exceeds word width.");
    return static_cast<T>(T{ 1 } << bitIndex);
}

/// @brief Builds a bitmask with the lowest @p bitCount bits set.
/// @tparam T Unsigned integral type.
/// @param[in] bitCount Number of low bits to set (clamped to the bit width of @c T).
/// @return Mask containing @p bitCount trailing one bits.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr T trailingBitMask(const UInt32 bitCount) noexcept
{
    constexpr UInt32 kWordBits = static_cast<UInt32>(sizeof(T) * 8u);
    if (bitCount == 0u)
    {
        return T{ 0 };
    }
    if (bitCount >= kWordBits)
    {
        return static_cast<T>(~T{ 0 });
    }
    return static_cast<T>((T{ 1 } << bitCount) - T{ 1 });
}

/// @brief Extracts and clears the lowest set bit index from @p inOutWord.
/// @details Lowers to `tzcnt` + `blsr` on modern x64 targets and `rbit` + `clz` on ARM64, allowing sparse bit loops
///          such as `SparseSet` or dirty-mask sweeps to skip all zero bits in constant time.
/// @note Requires @p inOutWord to be non-zero; use `tryGetAndClearNextBit()` when an empty word is possible.
/// @tparam T Unsigned integral type.
/// @param[in,out] inOutWord Non-zero word whose lowest set bit is extracted and cleared.
/// @return Zero-based index of the extracted bit (`[0, sizeof(T) * 8)`).
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UInt32 getAndClearNextBit(T& inOutWord) noexcept
{
    GP_ASSERT(inOutWord != T{ 0 } && "getAndClearNextBit() requires a non-zero word.");
    const UInt32 bitIndex = static_cast<UInt32>(countTrailingZeros(inOutWord));
    inOutWord = static_cast<T>(inOutWord & static_cast<T>(inOutWord - T{ 1 }));
    return bitIndex;
}

/// @brief Extracts and clears the lowest set bit index from @p inOutWord if any bit is set.
/// @tparam T Unsigned integral type.
/// @param[in,out] inOutWord Word whose lowest set bit is extracted and cleared when non-zero.
/// @param[out] outBitIndex Receives the extracted zero-based bit index on success; left untouched if empty.
/// @return True if a set bit was extracted, false when @p inOutWord was zero.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryGetAndClearNextBit(T& inOutWord, UInt32& outBitIndex) noexcept
{
    if (inOutWord == T{ 0 })
    {
        return false;
    }
    outBitIndex = static_cast<UInt32>(countTrailingZeros(inOutWord));
    inOutWord = static_cast<T>(inOutWord & static_cast<T>(inOutWord - T{ 1 }));
    return true;
}

/// @brief Extracts and clears the highest set bit index from @p inOutWord.
/// @note Requires @p inOutWord to be non-zero; use `tryGetAndClearHighestBit()` when an empty word is possible.
/// @tparam T Unsigned integral type.
/// @param[in,out] inOutWord Non-zero word whose most significant set bit is extracted and cleared.
/// @return Zero-based index of the extracted highest set bit (`[0, sizeof(T) * 8)`).
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UInt32 getAndClearHighestBit(T& inOutWord) noexcept
{
    GP_ASSERT(inOutWord != T{ 0 } && "getAndClearHighestBit() requires a non-zero word.");
    constexpr UInt32 kMaxBit = static_cast<UInt32>(sizeof(T) * 8u - 1u);
    const UInt32 bitIndex = kMaxBit - static_cast<UInt32>(countLeadingZeros(inOutWord));
    inOutWord = static_cast<T>(inOutWord ^ static_cast<T>(T{ 1 } << bitIndex));
    return bitIndex;
}

/// @brief Extracts and clears the highest set bit index from @p inOutWord if any bit is set.
/// @tparam T Unsigned integral type.
/// @param[in,out] inOutWord Word whose highest set bit is extracted and cleared when non-zero.
/// @param[out] outBitIndex Receives the extracted zero-based bit index on success; left untouched if empty.
/// @return True if a set bit was extracted, false when @p inOutWord was zero.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool tryGetAndClearHighestBit(T& inOutWord, UInt32& outBitIndex) noexcept
{
    if (inOutWord == T{ 0 })
    {
        return false;
    }
    constexpr UInt32 kMaxBit = static_cast<UInt32>(sizeof(T) * 8u - 1u);
    outBitIndex = kMaxBit - static_cast<UInt32>(countLeadingZeros(inOutWord));
    inOutWord = static_cast<T>(inOutWord ^ static_cast<T>(T{ 1 } << outBitIndex));
    return true;
}

}   // namespace gp::math
