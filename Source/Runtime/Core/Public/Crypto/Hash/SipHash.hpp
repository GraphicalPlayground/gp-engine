// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief SipHash - a cryptographically strong PRF suitable for hash-flooding-resistant hash tables.
/// Implements SipHash-2-4 (default), SipHash-1-3 (fast), and SipHash-4-8 (conservative).
/// Based on the original design by Jean-Philippe Aumasson and Daniel J. Bernstein.
struct SipHash final : public Template::NonInstantiable
{
public:
    using HashType = UInt64;   //<! SipHash produces 64-bit output.

public:
    static constexpr SipHashKey DefaultKey = { 0ULL, 0ULL };   //<! Default zero key for SipHash.

private:
    /// \brief Core SipHash round function.
    /// \param v0 State variable 0.
    /// \param v1 State variable 1.
    /// \param v2 State variable 2.
    /// \param v3 State variable 3.
    static constexpr void SipRound(UInt64& v0, UInt64& v1, UInt64& v2, UInt64& v3) noexcept
    {
        v0 += v1;
        v1 = Detail::HashUtilities::Rotl64(v1, 13);
        v1 ^= v0;
        v0 = Detail::HashUtilities::Rotl64(v0, 32);
        v2 += v3;
        v3 = Detail::HashUtilities::Rotl64(v3, 16);
        v3 ^= v2;
        v0 += v3;
        v3 = Detail::HashUtilities::Rotl64(v3, 21);
        v3 ^= v0;
        v2 += v1;
        v1 = Detail::HashUtilities::Rotl64(v1, 17);
        v1 ^= v2;
        v2 = Detail::HashUtilities::Rotl64(v2, 32);
    }

    /// \brief Generic SipHash implementation parameterized by compression and finalization rounds.
    /// \tparam cROUNDS Number of compression rounds per message block.
    /// \tparam dROUNDS Number of finalization rounds.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key 128-bit key for SipHash.
    /// \return 64-bit hash value.
    template <Int32 cROUNDS, Int32 dROUNDS>
    GP_NODISCARD static constexpr UInt64 SipHashCore(const char* data, SizeT length, SipHashKey key) noexcept
    {
        UInt64 v0 = 0x736F6D6570736575ULL ^ key.k0;
        UInt64 v1 = 0x646F72616E646F6DULL ^ key.k1;
        UInt64 v2 = 0x6C7967656E657261ULL ^ key.k0;
        UInt64 v3 = 0x7465646279746573ULL ^ key.k1;

        const SizeT nblocks = length / 8;

        for (SizeT i = 0; i < nblocks; ++i)
        {
            UInt64 m = Detail::HashUtilities::ReadU64LE(data, i * 8);
            v3 ^= m;
            for (Int32 r = 0; r < cROUNDS; ++r) { SipRound(v0, v1, v2, v3); }
            v0 ^= m;
        }

        UInt64 last = static_cast<UInt64>(length & 0xFF) << 56;
        const SizeT tail = nblocks * 8;

        switch (length & 7)
        {
        case 7: last |= static_cast<UInt64>(static_cast<UInt8>(data[tail + 6])) << 48; [[fallthrough]];
        case 6: last |= static_cast<UInt64>(static_cast<UInt8>(data[tail + 5])) << 40; [[fallthrough]];
        case 5: last |= static_cast<UInt64>(static_cast<UInt8>(data[tail + 4])) << 32; [[fallthrough]];
        case 4: last |= static_cast<UInt64>(static_cast<UInt8>(data[tail + 3])) << 24; [[fallthrough]];
        case 3: last |= static_cast<UInt64>(static_cast<UInt8>(data[tail + 2])) << 16; [[fallthrough]];
        case 2: last |= static_cast<UInt64>(static_cast<UInt8>(data[tail + 1])) << 8; [[fallthrough]];
        case 1: last |= static_cast<UInt64>(static_cast<UInt8>(data[tail])); break;
        case 0: break;
        }

        v3 ^= last;
        for (Int32 r = 0; r < cROUNDS; ++r) { SipRound(v0, v1, v2, v3); }
        v0 ^= last;

        v2 ^= 0xFF;
        for (Int32 r = 0; r < dROUNDS; ++r) { SipRound(v0, v1, v2, v3); }

        return v0 ^ v1 ^ v2 ^ v3;
    }

public:
    /// \brief Compute SipHash-2-4 for a data buffer (standard variant).
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key 128-bit key.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* data, SizeT length, SipHashKey key = DefaultKey) noexcept
    {
        return SipHashCore<2, 4>(data, length, key);
    }

    /// \brief Compute SipHash-2-4 from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key 128-bit key.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const void* data, SizeT length, SipHashKey key = DefaultKey) noexcept
    {
        return Hash64(static_cast<const char*>(data), length, key);
    }

    /// \brief Compute SipHash-2-4 for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str), DefaultKey);
    }

    /// \brief Compute SipHash-2-4 from a string view.
    /// \param view String view to hash.
    /// \param key 128-bit key.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(std::string_view view, SipHashKey key = DefaultKey) noexcept
    {
        return Hash64(view.data(), view.size(), key);
    }

    /// \brief Compute SipHash-1-3 for a data buffer (fast variant, lower security margin).
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key 128-bit key.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64
        Hash64Fast(const char* data, SizeT length, SipHashKey key = DefaultKey) noexcept
    {
        return SipHashCore<1, 3>(data, length, key);
    }

    /// \brief Compute SipHash-1-3 from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key 128-bit key.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64
        Hash64Fast(const void* data, SizeT length, SipHashKey key = DefaultKey) noexcept
    {
        return Hash64Fast(static_cast<const char*>(data), length, key);
    }

    /// \brief Compute SipHash-4-8 for a data buffer (conservative variant, higher security margin).
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key 128-bit key.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64
        Hash64Strong(const char* data, SizeT length, SipHashKey key = DefaultKey) noexcept
    {
        return SipHashCore<4, 8>(data, length, key);
    }

    /// \brief Compute SipHash-4-8 from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key 128-bit key.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64
        Hash64Strong(const void* data, SizeT length, SipHashKey key = DefaultKey) noexcept
    {
        return Hash64Strong(static_cast<const char*>(data), length, key);
    }

    /// \brief Convenience entry point (SipHash-2-4 with default key).
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(const char* data, SizeT length) noexcept
    {
        return Hash64(data, length, DefaultKey);
    }

    /// \brief Convenience overload for void pointer input.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Convenience overload for null-terminated strings.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Convenience overload for string views.
    /// \param view String view to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }
};

}   // namespace GP::Crypto

/// \brief User-defined literal for computing SipHash-2-4 at compile time from string literals.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Compile-time computed SipHash value (uses default zero key).
inline constexpr GP::UInt64 operator""_siphash(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::SipHash::Hash64(str, len);
}
