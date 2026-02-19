// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP::Crypto
{

/// \brief 128-bit hash result structure used by multiple hash algorithms.
struct Hash128Result
{
public:
    UInt64 low;    //<! Lower 64 bits of the 128-bit hash.
    UInt64 high;   //<! Upper 64 bits of the 128-bit hash.

public:
    GP_NODISCARD constexpr bool operator==(const Hash128Result&) const noexcept = default;
};

/// \brief Fixed-size digest result for MD5 (128-bit / 16 bytes).
struct MD5Digest
{
public:
    static constexpr SizeT DIGEST_SIZE = 16;

public:
    Byte bytes[DIGEST_SIZE]{};

public:
    GP_NODISCARD constexpr bool operator==(const MD5Digest&) const noexcept = default;
};

/// \brief Fixed-size digest result for SHA-1 (160-bit / 20 bytes).
struct SHA1Digest
{
public:
    static constexpr SizeT DIGEST_SIZE = 20;

public:
    Byte bytes[DIGEST_SIZE]{};

public:
    GP_NODISCARD constexpr bool operator==(const SHA1Digest&) const noexcept = default;
};

/// \brief Fixed-size digest result for SHA-256 (256-bit / 32 bytes).
struct SHA256Digest
{
public:
    static constexpr SizeT DIGEST_SIZE = 32;

public:
    Byte bytes[DIGEST_SIZE]{};

public:
    GP_NODISCARD constexpr bool operator==(const SHA256Digest&) const noexcept = default;
};

/// \brief Fixed-size digest result for SHA-384 (384-bit / 48 bytes).
struct SHA384Digest
{
public:
    static constexpr SizeT DIGEST_SIZE = 48;

public:
    Byte bytes[DIGEST_SIZE]{};

public:
    GP_NODISCARD constexpr bool operator==(const SHA384Digest&) const noexcept = default;
};

/// \brief Fixed-size digest result for SHA-512 (512-bit / 64 bytes).
struct SHA512Digest
{
public:
    static constexpr SizeT DIGEST_SIZE = 64;

public:
    Byte bytes[DIGEST_SIZE]{};

public:
    GP_NODISCARD constexpr bool operator==(const SHA512Digest&) const noexcept = default;
};

/// \brief Variable-size digest result for BLAKE2b (1-64 bytes, default 32).
struct BLAKE2bDigest
{
public:
    static constexpr SizeT MAX_DIGEST_SIZE = 64;

public:
    Byte bytes[MAX_DIGEST_SIZE]{};
    SizeT length{};

public:
    GP_NODISCARD constexpr bool operator==(const BLAKE2bDigest& other) const noexcept
    {
        if (length != other.length)
        {
            return false;
        }
        for (SizeT i = 0; i < length; ++i)
        {
            if (bytes[i] != other.bytes[i])
            {
                return false;
            }
        }
        return true;
    }
};

/// \brief Fixed-size digest result for BLAKE3 (256-bit / 32 bytes default).
struct BLAKE3Digest
{
public:
    static constexpr SizeT DIGEST_SIZE = 32;

public:
    Byte bytes[DIGEST_SIZE]{};

public:
    GP_NODISCARD constexpr bool operator==(const BLAKE3Digest&) const noexcept = default;
};

/// \brief 128-bit key used by SipHash.
struct SipHashKey
{
public:
    UInt64 k0{};
    UInt64 k1{};

public:
    GP_NODISCARD constexpr bool operator==(const SipHashKey&) const noexcept = default;
};

}   // namespace GP::Crypto
