// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Generic.hpp"
#include <array>
#include <string_view>

namespace GP::Crypto
{

/// \brief CRC-32 (IEEE 802.3) implementation for checksums and streaming verification.
struct CRC32 final : public Template::NonInstantiable
{
public:
    using HashType = UInt32;   //<! CRC-32 is always 32 bits.

public:
    static constexpr HashType Polynomial = 0xEDB88320U;     //<! Reversed polynomial for CRC-32 (IEEE 802.3).
    static constexpr HashType InitialValue = 0xFFFFFFFFU;   //<! Initial value for CRC-32 computation.

private:
    /// \brief Get the precomputed CRC-32 lookup table.
    /// The table is generated at compile time using a lambda and stored as a static constexpr array.
    /// \return Reference to the CRC-32 lookup table.
    GP_NODISCARD static constexpr const std::array<UInt32, 256>& Table() noexcept
    {
        constexpr auto kTable = []
        {
            std::array<UInt32, 256> t{};
            for (UInt32 i = 0; i < 256; ++i)
            {
                UInt32 crc = i;
                for (UInt32 j = 0; j < 8; ++j) { crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0); }
                t[i] = crc;
            }
            return t;
        }();
        return kTable;
    }

public:
    /// \brief Compute CRC-32 for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to process.
    /// \return CRC-32 value.
    GP_NODISCARD static constexpr HashType Hash(const char* data, Size length) noexcept
    {
        UInt32 crc = InitialValue;
        for (Size i = 0; i < length; ++i) { crc = Table()[static_cast<UInt8>(crc ^ data[i])] ^ (crc >> 8); }
        return crc ^ InitialValue;
    }

    /// \brief Compute CRC-32 from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to process.
    /// \return CRC-32 value.
    GP_NODISCARD static constexpr HashType Hash(const void* data, Size length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Compute CRC-32 for a null-terminated string.
    /// \param str Null-terminated input string.
    /// \return CRC-32 value.
    GP_NODISCARD static constexpr HashType Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute CRC-32 from a string view.
    /// \param view String view to process.
    /// \return CRC-32 value.
    GP_NODISCARD static constexpr HashType Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

    /// \brief Append bytes to an existing CRC-32 value (useful for streaming).
    /// \param crc Previous CRC-32 value.
    /// \param data Pointer to additional bytes.
    /// \param length Number of additional bytes.
    /// \return Updated CRC-32 value.
    GP_NODISCARD static constexpr HashType Append(HashType crc, const char* data, Size length) noexcept
    {
        crc ^= InitialValue;
        for (Size i = 0; i < length; ++i) { crc = Table()[static_cast<UInt8>(crc ^ data[i])] ^ (crc >> 8); }
        return crc ^ InitialValue;
    }

    /// \brief Append bytes from a void pointer to an existing CRC-32 value.
    /// \param crc Previous CRC-32 value.
    /// \param data Pointer to additional bytes.
    /// \param length Number of additional bytes.
    /// \return Updated CRC-32 value.
    GP_NODISCARD static constexpr HashType Append(HashType crc, const void* data, Size length) noexcept
    {
        return Append(crc, static_cast<const char*>(data), length);
    }

    /// \brief Verify buffer contents against an expected CRC-32 value.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected CRC-32 value.
    /// \return True if computed CRC equals expected value.
    GP_NODISCARD static constexpr bool Verify(const char* data, Size length, HashType expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Verify buffer contents from a void pointer against an expected CRC-32 value.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected CRC-32 value.
    /// \return True if computed CRC equals expected value.
    GP_NODISCARD static constexpr bool Verify(const void* data, Size length, HashType expected) noexcept
    {
        return Hash(data, length) == expected;
    }
};

}   // namespace GP::Crypto

/// \brief User-defined literal for CRC-32 hashing of string literals at compile time.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Computed CRC-32 hash value.
inline constexpr GP::Crypto::CRC32::HashType operator""_crc32(const char* str, GP::Size len) noexcept
{
    return GP::Crypto::CRC32::Hash(str, len);
}
