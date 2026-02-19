// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Generic.hpp"
#include <type_traits>

namespace GP::Crypto::Detail
{

/// \brief Internal utility functions shared across hash algorithm implementations.
/// \warning This is an internal detail header. Do not use directly in application code.
struct HashUtilities final : public Template::NonInstantiable
{
public:
    /// \brief Rotate a 32-bit value left by a given number of bits.
    /// \param x Value to rotate.
    /// \param r Number of bits to rotate (must be in range [0, 31]).
    /// \return Rotated value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt32 Rotl32(UInt32 x, Int32 r) noexcept
    {
        return (x << r) | (x >> (32 - r));
    }

    /// \brief Rotate a 64-bit value left by a given number of bits.
    /// \param x Value to rotate.
    /// \param r Number of bits to rotate (must be in range [0, 63]).
    /// \return Rotated value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt64 Rotl64(UInt64 x, Int32 r) noexcept
    {
        return (x << r) | (x >> (64 - r));
    }

    /// \brief Rotate a 32-bit value right by a given number of bits.
    /// \param x Value to rotate.
    /// \param r Number of bits to rotate (must be in range [0, 31]).
    /// \return Rotated value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt32 Rotr32(UInt32 x, Int32 r) noexcept
    {
        return (x >> r) | (x << (32 - r));
    }

    /// \brief Rotate a 64-bit value right by a given number of bits.
    /// \param x Value to rotate.
    /// \param r Number of bits to rotate (must be in range [0, 63]).
    /// \return Rotated value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt64 Rotr64(UInt64 x, Int32 r) noexcept
    {
        return (x >> r) | (x << (64 - r));
    }

    /// \brief Read a 16-bit unsigned integer from a byte buffer in little-endian order.
    /// \param data Pointer to the byte buffer.
    /// \param offset Byte offset to read from.
    /// \return 16-bit unsigned integer value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt16 ReadU16LE(const char* data, SizeT offset) noexcept
    {
        return static_cast<UInt16>(static_cast<UInt8>(data[offset])) |
               (static_cast<UInt16>(static_cast<UInt8>(data[offset + 1])) << 8);
    }

    /// \brief Read a 32-bit unsigned integer from a byte buffer in little-endian order.
    /// \param data Pointer to the byte buffer.
    /// \param offset Byte offset to read from.
    /// \return 32-bit unsigned integer value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt32 ReadU32LE(const char* data, SizeT offset) noexcept
    {
        return static_cast<UInt32>(static_cast<UInt8>(data[offset])) |
               (static_cast<UInt32>(static_cast<UInt8>(data[offset + 1])) << 8) |
               (static_cast<UInt32>(static_cast<UInt8>(data[offset + 2])) << 16) |
               (static_cast<UInt32>(static_cast<UInt8>(data[offset + 3])) << 24);
    }

    /// \brief Read a 64-bit unsigned integer from a byte buffer in little-endian order.
    /// \param data Pointer to the byte buffer.
    /// \param offset Byte offset to read from.
    /// \return 64-bit unsigned integer value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt64 ReadU64LE(const char* data, SizeT offset) noexcept
    {
        return static_cast<UInt64>(static_cast<UInt8>(data[offset])) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 1])) << 8) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 2])) << 16) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 3])) << 24) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 4])) << 32) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 5])) << 40) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 6])) << 48) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 7])) << 56);
    }

    /// \brief Read a 32-bit unsigned integer from a byte buffer in big-endian order.
    /// \param data Pointer to the byte buffer.
    /// \param offset Byte offset to read from.
    /// \return 32-bit unsigned integer value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt32 ReadU32BE(const char* data, SizeT offset) noexcept
    {
        return (static_cast<UInt32>(static_cast<UInt8>(data[offset])) << 24) |
               (static_cast<UInt32>(static_cast<UInt8>(data[offset + 1])) << 16) |
               (static_cast<UInt32>(static_cast<UInt8>(data[offset + 2])) << 8) |
               static_cast<UInt32>(static_cast<UInt8>(data[offset + 3]));
    }

    /// \brief Read a 64-bit unsigned integer from a byte buffer in big-endian order.
    /// \param data Pointer to the byte buffer.
    /// \param offset Byte offset to read from.
    /// \return 64-bit unsigned integer value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt64 ReadU64BE(const char* data, SizeT offset) noexcept
    {
        return (static_cast<UInt64>(static_cast<UInt8>(data[offset])) << 56) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 1])) << 48) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 2])) << 40) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 3])) << 32) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 4])) << 24) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 5])) << 16) |
               (static_cast<UInt64>(static_cast<UInt8>(data[offset + 6])) << 8) |
               static_cast<UInt64>(static_cast<UInt8>(data[offset + 7]));
    }

    /// \brief Write a 32-bit unsigned integer to a byte buffer in big-endian order.
    /// \param dst Pointer to the destination byte buffer.
    /// \param offset Byte offset to write at.
    /// \param value The value to write.
    static GP_FORCEINLINE constexpr void WriteU32BE(char* dst, SizeT offset, UInt32 value) noexcept
    {
        dst[offset] = static_cast<char>(static_cast<UInt8>((value >> 24) & 0xFF));
        dst[offset + 1] = static_cast<char>(static_cast<UInt8>((value >> 16) & 0xFF));
        dst[offset + 2] = static_cast<char>(static_cast<UInt8>((value >> 8) & 0xFF));
        dst[offset + 3] = static_cast<char>(static_cast<UInt8>(value & 0xFF));
    }

    /// \brief Write a 64-bit unsigned integer to a byte buffer in big-endian order.
    /// \param dst Pointer to the destination byte buffer.
    /// \param offset Byte offset to write at.
    /// \param value The value to write.
    static GP_FORCEINLINE constexpr void WriteU64BE(char* dst, SizeT offset, UInt64 value) noexcept
    {
        dst[offset] = static_cast<char>(static_cast<UInt8>((value >> 56) & 0xFF));
        dst[offset + 1] = static_cast<char>(static_cast<UInt8>((value >> 48) & 0xFF));
        dst[offset + 2] = static_cast<char>(static_cast<UInt8>((value >> 40) & 0xFF));
        dst[offset + 3] = static_cast<char>(static_cast<UInt8>((value >> 32) & 0xFF));
        dst[offset + 4] = static_cast<char>(static_cast<UInt8>((value >> 24) & 0xFF));
        dst[offset + 5] = static_cast<char>(static_cast<UInt8>((value >> 16) & 0xFF));
        dst[offset + 6] = static_cast<char>(static_cast<UInt8>((value >> 8) & 0xFF));
        dst[offset + 7] = static_cast<char>(static_cast<UInt8>(value & 0xFF));
    }

    /// \brief Write a 32-bit unsigned integer to a byte buffer in little-endian order.
    /// \param dst Pointer to the destination byte buffer.
    /// \param offset Byte offset to write at.
    /// \param value The value to write.
    static GP_FORCEINLINE constexpr void WriteU32LE(char* dst, SizeT offset, UInt32 value) noexcept
    {
        dst[offset] = static_cast<char>(static_cast<UInt8>(value & 0xFF));
        dst[offset + 1] = static_cast<char>(static_cast<UInt8>((value >> 8) & 0xFF));
        dst[offset + 2] = static_cast<char>(static_cast<UInt8>((value >> 16) & 0xFF));
        dst[offset + 3] = static_cast<char>(static_cast<UInt8>((value >> 24) & 0xFF));
    }

    /// \brief Write a 64-bit unsigned integer to a byte buffer in little-endian order.
    /// \param dst Pointer to the destination byte buffer.
    /// \param offset Byte offset to write at.
    /// \param value The value to write.
    static GP_FORCEINLINE constexpr void WriteU64LE(char* dst, SizeT offset, UInt64 value) noexcept
    {
        dst[offset] = static_cast<char>(static_cast<UInt8>(value & 0xFF));
        dst[offset + 1] = static_cast<char>(static_cast<UInt8>((value >> 8) & 0xFF));
        dst[offset + 2] = static_cast<char>(static_cast<UInt8>((value >> 16) & 0xFF));
        dst[offset + 3] = static_cast<char>(static_cast<UInt8>((value >> 24) & 0xFF));
        dst[offset + 4] = static_cast<char>(static_cast<UInt8>((value >> 32) & 0xFF));
        dst[offset + 5] = static_cast<char>(static_cast<UInt8>((value >> 40) & 0xFF));
        dst[offset + 6] = static_cast<char>(static_cast<UInt8>((value >> 48) & 0xFF));
        dst[offset + 7] = static_cast<char>(static_cast<UInt8>((value >> 56) & 0xFF));
    }

    /// \brief Swap the byte order of a 32-bit integer.
    /// \param value Value to byte-swap.
    /// \return Byte-swapped value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt32 ByteSwap32(UInt32 value) noexcept
    {
        return ((value & 0x000000FFU) << 24) | ((value & 0x0000FF00U) << 8) | ((value & 0x00FF0000U) >> 8) |
               ((value & 0xFF000000U) >> 24);
    }

    /// \brief Swap the byte order of a 64-bit integer.
    /// \param value Value to byte-swap.
    /// \return Byte-swapped value.
    GP_NODISCARD static GP_FORCEINLINE constexpr UInt64 ByteSwap64(UInt64 value) noexcept
    {
        return ((value & 0x00000000000000FFULL) << 56) | ((value & 0x000000000000FF00ULL) << 40) |
               ((value & 0x0000000000FF0000ULL) << 24) | ((value & 0x00000000FF000000ULL) << 8) |
               ((value & 0x000000FF00000000ULL) >> 8) | ((value & 0x0000FF0000000000ULL) >> 24) |
               ((value & 0x00FF000000000000ULL) >> 40) | ((value & 0xFF00000000000000ULL) >> 56);
    }

    /// \brief Perform a widening multiply of two 64-bit values and return both halves.
    /// \param a First operand.
    /// \param b Second operand.
    /// \param lo Output: lower 64 bits of the 128-bit product.
    /// \param hi Output: upper 64 bits of the 128-bit product.
    static GP_FORCEINLINE constexpr void Multiply128(UInt64 a, UInt64 b, UInt64& lo, UInt64& hi) noexcept
    {
#if !GP_BUILD_DEBUG && (GP_COMPILER_GCC || GP_COMPILER_CLANG) && GP_ARCHITECTURE_64BIT
        if (!std::is_constant_evaluated())
        {
            __uint128_t result = static_cast<__uint128_t>(a) * b;
            lo = static_cast<UInt64>(result);
            hi = static_cast<UInt64>(result >> 64);
            return;
        }
#endif
        const UInt64 aLo = a & 0xFFFFFFFFULL;
        const UInt64 aHi = a >> 32;
        const UInt64 bLo = b & 0xFFFFFFFFULL;
        const UInt64 bHi = b >> 32;

        const UInt64 p0 = aLo * bLo;
        const UInt64 p1 = aLo * bHi;
        const UInt64 p2 = aHi * bLo;
        const UInt64 p3 = aHi * bHi;

        const UInt64 mid = (p0 >> 32) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL);
        lo = (p0 & 0xFFFFFFFFULL) | (mid << 32);
        hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
    }
};

}   // namespace GP::Crypto::Detail
