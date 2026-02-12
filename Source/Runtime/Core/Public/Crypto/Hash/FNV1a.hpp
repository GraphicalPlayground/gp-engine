// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief Fast, non-cryptographic FNV-1a hash utilities.
/// Provides constexpr helpers for 32/64-bit FNV-1a hashing of strings and binary data.
struct FNV1a final : public Template::NonInstantiable
{
public:
#if GP_ARCHITECTURE_64BIT
    using HashType = UInt64;
    static constexpr HashType OffsetBasis = 14695981039346656037ULL;   //<! 64-bit FNV-1a offset basis.
    static constexpr HashType Prime = 1099511628211ULL;                //<! 64-bit FNV-1a prime.
#else
    using HashType = UInt32;
    static constexpr HashType OffsetBasis = 2166136261U;   //<! 32-bit FNV-1a offset basis.
    static constexpr HashType Prime = 16777619U;           //<! 32-bit FNV-1a prime.
#endif
    static constexpr UInt32 OffsetBasis32 = 2166136261U;               //<! 32-bit FNV-1a offset basis.
    static constexpr UInt32 Prime32 = 16777619U;                       //<! 32-bit FNV-1a prime.
    static constexpr UInt64 OffsetBasis64 = 14695981039346656037ULL;   //<! 64-bit FNV-1a offset basis.
    static constexpr UInt64 Prime64 = 1099511628211ULL;                //<! 64-bit FNV-1a prime.

private:
    /// \brief Core FNV-1a hashing function template.
    /// \tparam T Hash type (UInt32 or UInt64).
    /// \param data Pointer to input bytes.
    /// \param length Number of bytes to hash.
    /// \param basis Initial hash value (offset basis).
    /// \param prime FNV prime for the hash size.
    /// \return Computed hash value.
    template <typename T>
    GP_NODISCARD static constexpr T Core(const char* data, SizeT length, T basis, T prime) noexcept
    {
        T hash = basis;
        for (SizeT i = 0; i < length; ++i)
        {
            hash ^= static_cast<UInt8>(data[i]);
            hash *= prime;
        }
        return hash;
    }

public:
    /// \brief Compute FNV-1a hash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return Computed hash value (architecture-sized).
    GP_NODISCARD static constexpr HashType Hash(const char* data, SizeT length) noexcept
    {
        return Core<HashType>(data, length, OffsetBasis, Prime);
    }

    /// \brief Overload accepting a raw pointer-to-void.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return Computed hash value (architecture-sized).
    GP_NODISCARD static constexpr HashType Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Compute hash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return Computed hash value (architecture-sized).
    GP_NODISCARD static constexpr HashType Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute hash from a string view.
    /// \param view String view to hash.
    /// \return Computed hash value (architecture-sized).
    GP_NODISCARD static constexpr HashType Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

    /// \brief Compute 32-bit FNV-1a hash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* data, SizeT length) noexcept
    {
        return Core<UInt32>(data, length, OffsetBasis32, Prime32);
    }

    /// \brief Compute 32-bit hash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const void* data, SizeT length) noexcept
    {
        return Hash32(static_cast<const char*>(data), length);
    }

    /// \brief Compute 32-bit hash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* str) noexcept
    {
        return Hash32(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute 32-bit hash from a string view.
    /// \param view String view to hash.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(std::string_view view) noexcept
    {
        return Hash32(view.data(), view.size());
    }

    /// \brief Compute 64-bit FNV-1a hash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* data, SizeT length) noexcept
    {
        return Core<UInt64>(data, length, OffsetBasis64, Prime64);
    }

    /// \brief Compute 64-bit hash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const void* data, SizeT length) noexcept
    {
        return Hash64(static_cast<const char*>(data), length);
    }

    /// \brief Compute 64-bit hash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute 64-bit hash from a string view.
    /// \param view String view to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(std::string_view view) noexcept
    {
        return Hash64(view.data(), view.size());
    }
};

}   // namespace GP::Crypto

/// \brief User-defined literal for FNV-1a hashing of string literals at compile time.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Computed FNV-1a hash value.
inline constexpr GP::Crypto::FNV1a::HashType operator""_fnv1a(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::FNV1a::Hash(str, len);
}
