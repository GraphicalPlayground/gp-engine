// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/Murmur3.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/SipHash.hpp"
#include "hashing/Wyhash.hpp"
#include "hashing/XxHash.hpp"

namespace gp::hashing
{

/// @brief Zero-allocation runtime streaming hasher dispatching across any `HashAlgorithm` enumerator.
/// @details Holds an inline tagged union of all built-in streaming accumulators on the stack with no heap allocation
///          or virtual dispatch. Exposes the same `reset()`, `update()`, `updateIgnoreCase()`, and `finalize*()`
///          surface as the concrete compile-time hashers.
class DynamicHasher final
{
public:
    /// @brief Constructs a `DynamicHasher` configured for @p algorithm using its canonical default seed.
    /// @param[in] algorithm Target algorithm identifier (defaults to `HashAlgorithm::Fnv1a64`).
    [[nodiscard]] constexpr explicit DynamicHasher(const HashAlgorithm algorithm = HashAlgorithm::Fnv1a64) noexcept
        : m_storage(makeStorage(sanitizeAlgorithm(algorithm)))
        , m_algorithm(sanitizeAlgorithm(algorithm))
    {}

public:
    /// @brief Returns the currently selected `HashAlgorithm`.
    /// @return Active `HashAlgorithm` enumerator.
    [[nodiscard]] constexpr HashAlgorithm algorithm() const noexcept
    {
        return m_algorithm;
    }

    /// @brief Returns the native output digest bit width (`32`, `64`, or `128`).
    /// @return Digest width in bits.
    [[nodiscard]] constexpr gp::UInt32 digestBits() const noexcept
    {
        return algorithmDigestBits(m_algorithm);
    }

    /// @brief Resets the active hasher to its canonical default seed.
    constexpr void reset() noexcept
    {
        m_storage = makeStorage(m_algorithm);
    }

    /// @brief Switches algorithm and resets the active accumulator to its canonical default seed.
    /// @param[in] algorithm New algorithm to activate.
    constexpr void reset(const HashAlgorithm algorithm) noexcept
    {
        m_algorithm = sanitizeAlgorithm(algorithm);
        m_storage = makeStorage(m_algorithm);
    }

    /// @brief Resets the current algorithm with an explicit 64-bit seed (truncated to 32 bits for 32-bit hashes).
    /// @param[in] seed Explicit seed value.
    constexpr void resetWithSeed(const gp::UInt64 seed) noexcept
    {
        m_storage = makeSeededStorage(m_algorithm, seed);
    }

    /// @brief Resets the active accumulator with a 128-bit seed (used as 128-bit key for `SipHash24` or 128-bit seed
    ///        for `Murmur3_128`, folded for narrower algorithms).
    /// @param[in] seed 128-bit seed or key value.
    constexpr void resetWithSeed(const Hash128& seed) noexcept
    {
        if (m_algorithm == HashAlgorithm::Murmur3_128)
        {
            m_storage = StateStorage(Murmur3_128(seed));
        }
        else if (m_algorithm == HashAlgorithm::SipHash24)
        {
            m_storage = StateStorage(SipHash24(seed));
        }
        else if (m_algorithm == HashAlgorithm::Sha256)
        {
            m_storage = StateStorage(Sha256(Hash256(seed.high, seed.low, 0ULL, 0ULL)));
        }
        else
        {
            m_storage = makeSeededStorage(m_algorithm, seed.foldTo64());
        }
    }

    /// @brief Resets the active accumulator with a 256-bit seed (used directly by `Sha256`, folded otherwise).
    /// @param[in] seed 256-bit seed value.
    constexpr void resetWithSeed(const Hash256& seed) noexcept
    {
        if (m_algorithm == HashAlgorithm::Sha256)
        {
            m_storage = StateStorage(Sha256(seed));
        }
        else if (m_algorithm == HashAlgorithm::Murmur3_128 || m_algorithm == HashAlgorithm::SipHash24)
        {
            resetWithSeed(seed.foldTo128());
        }
        else
        {
            m_storage = makeSeededStorage(m_algorithm, seed.foldTo64());
        }
    }

    /// @brief Handles null-pointer zero-byte updates as a no-op.
    /// @param[in] data Null pointer literal.
    /// @param[in] sizeInBytes Ignored byte count.
    constexpr void update([[maybe_unused]] gp::NullPtrT data, [[maybe_unused]] const gp::USize sizeInBytes) noexcept
    {}

    /// @brief Feeds a `gp::UInt8` buffer into the active accumulator (`constexpr`-capable).
    /// @param[in] data Byte buffer pointer.
    /// @param[in] sizeInBytes Number of bytes to consume.
    constexpr void update(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        dispatch(
            [data, sizeInBytes]<typename InHasher>(InHasher& hasher) noexcept -> void
        {
            hasher.update(data, sizeInBytes);
        }
        );
    }

    /// @brief Feeds a `gp::Byte` buffer into the active accumulator (`constexpr`-capable).
    /// @param[in] data Byte buffer pointer.
    /// @param[in] sizeInBytes Number of bytes to consume.
    constexpr void update(const gp::Byte* const data, const gp::USize sizeInBytes) noexcept
    {
        dispatch(
            [data, sizeInBytes]<typename InHasher>(InHasher& hasher) noexcept -> void
        {
            hasher.update(data, sizeInBytes);
        }
        );
    }

    /// @brief Feeds a character pointer range into the active accumulator (`constexpr`-capable).
    /// @tparam InCharT Character code-unit type.
    /// @param[in] data Character pointer.
    /// @param[in] count Number of character code units to consume.
    template <gp::concepts::IsCharacter InCharT>
    constexpr void update(const InCharT* const data, const gp::USize count) noexcept
    {
        dispatch(
            [data, count]<typename InHasher>(InHasher& hasher) noexcept -> void
        {
            hasher.update(data, count);
        }
        );
    }

    /// @brief Feeds an untyped runtime memory buffer into the active accumulator.
    /// @param[in] data Buffer address.
    /// @param[in] sizeInBytes Number of bytes to consume.
    void update(const void* const data, const gp::USize sizeInBytes) noexcept
    {
        dispatch(
            [data, sizeInBytes]<typename InHasher>(InHasher& hasher) noexcept -> void
        {
            hasher.update(data, sizeInBytes);
        }
        );
    }

    /// @brief Feeds a string-like container or view into the active accumulator (`constexpr`-capable).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to consume.
    template <concepts::IsCharacterSequence InStrT>
    constexpr void update(const InStrT& str) noexcept
    {
        update(str.data(), str.size());
    }

    /// @brief Feeds a character range after ASCII lowercase folding (`constexpr`-capable).
    /// @tparam InCharT Character code-unit type.
    /// @param[in] data Character pointer.
    /// @param[in] count Number of code units to fold and consume.
    template <gp::concepts::IsCharacter InCharT>
    constexpr void updateIgnoreCase(const InCharT* const data, const gp::USize count) noexcept
    {
        dispatch(
            [data, count]<typename InHasher>(InHasher& hasher) noexcept -> void
        {
            hasher.updateIgnoreCase(data, count);
        }
        );
    }

    /// @brief Feeds a string-like container or view after ASCII lowercase folding (`constexpr`-capable).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to fold and consume.
    template <concepts::IsCharacterSequence InStrT>
    constexpr void updateIgnoreCase(const InStrT& str) noexcept
    {
        updateIgnoreCase(str.data(), str.size());
    }

    /// @brief Feeds a single raw byte into the active accumulator (`constexpr`-capable).
    /// @param[in] byte Byte value to consume.
    constexpr void updateByte(const gp::UInt8 byte) noexcept
    {
        dispatch(
            [byte]<typename InHasher>(InHasher& hasher) noexcept -> void
        {
            hasher.updateByte(byte);
        }
        );
    }

    /// @brief Feeds a structured value into the active accumulator (`constexpr`-capable).
    /// @tparam InValueT Hashable value type.
    /// @param[in] value Input value to append.
    template <typename InValueT>
    constexpr void append(const InValueT& value) noexcept
    {
        dispatch(
            [&value]<typename InHasher>(InHasher& hasher) noexcept -> void
        {
            hasher.append(value);
        }
        );
    }

    /// @brief Computes and returns the unified `HashDigest` without mutating internal state.
    /// @return `HashDigest` carrying the native width, algorithm tag, and digest bits.
    [[nodiscard]] constexpr HashDigest finalize() const noexcept
    {
        switch (m_algorithm)
        {
        case HashAlgorithm::Fnv1a32:
            return HashDigest(m_storage.fnv32.finalize(), m_algorithm);
        case HashAlgorithm::Fnv1a64:
            return HashDigest(m_storage.fnv64.finalize(), m_algorithm);
        case HashAlgorithm::Crc32:
            return HashDigest(m_storage.crc32.finalize(), m_algorithm);
        case HashAlgorithm::Crc32C:
            return HashDigest(m_storage.crc32c.finalize(), m_algorithm);
        case HashAlgorithm::Murmur3_32:
            return HashDigest(m_storage.murmur32.finalize(), m_algorithm);
        case HashAlgorithm::Murmur3_128:
            return HashDigest(m_storage.murmur128.finalize(), m_algorithm);
        case HashAlgorithm::XxHash32:
            return HashDigest(m_storage.xx32.finalize(), m_algorithm);
        case HashAlgorithm::XxHash64:
            return HashDigest(m_storage.xx64.finalize(), m_algorithm);
        case HashAlgorithm::Wyhash64:
            return HashDigest(m_storage.wyhash64.finalize(), m_algorithm);
        case HashAlgorithm::SipHash24:
            return HashDigest(m_storage.sip24.finalize(), m_algorithm);
        case HashAlgorithm::Sha256:
            return HashDigest(m_storage.sha256.finalize(), m_algorithm);
        case HashAlgorithm::Count:
            // falls through
        default:
            return HashDigest(m_storage.fnv64.finalize(), HashAlgorithm::Fnv1a64);
        }
    }

    /// @brief Finalizes and converts the result to a 32-bit digest (folding wider digests if needed).
    /// @return 32-bit digest value.
    [[nodiscard]] constexpr gp::UInt32 finalize32() const noexcept
    {
        return finalize().toUInt32();
    }

    /// @brief Finalizes and converts the result to a 64-bit digest (folding wider digests if needed).
    /// @return 64-bit digest value.
    [[nodiscard]] constexpr gp::UInt64 finalize64() const noexcept
    {
        return finalize().toUInt64();
    }

    /// @brief Finalizes and returns a 128-bit digest (zero-extended for 32/64-bit, folded for 256-bit).
    /// @return 128-bit digest value.
    [[nodiscard]] constexpr Hash128 finalize128() const noexcept
    {
        return finalize().toHash128();
    }

    /// @brief Finalizes and returns the full 256-bit storage (`{ digest, 0, ... }` for narrower algorithms).
    /// @return 256-bit digest value.
    [[nodiscard]] constexpr Hash256 finalize256() const noexcept
    {
        return finalize().toHash256();
    }

private:
    union StateStorage
    {
        Fnv1a32 fnv32;
        Fnv1a64 fnv64;
        Crc32 crc32;
        Crc32C crc32c;
        Murmur3_32 murmur32;
        Murmur3_128 murmur128;
        XxHash32 xx32;
        XxHash64 xx64;
        Wyhash64 wyhash64;
        SipHash24 sip24;
        Sha256 sha256;

        constexpr explicit StateStorage(const Fnv1a32& value) noexcept
            : fnv32(value)
        {}

        constexpr explicit StateStorage(const Fnv1a64& value) noexcept
            : fnv64(value)
        {}

        constexpr explicit StateStorage(const Crc32& value) noexcept
            : crc32(value)
        {}

        constexpr explicit StateStorage(const Crc32C& value) noexcept
            : crc32c(value)
        {}

        constexpr explicit StateStorage(const Murmur3_32& value) noexcept
            : murmur32(value)
        {}

        constexpr explicit StateStorage(const Murmur3_128& value) noexcept
            : murmur128(value)
        {}

        constexpr explicit StateStorage(const XxHash32& value) noexcept
            : xx32(value)
        {}

        constexpr explicit StateStorage(const XxHash64& value) noexcept
            : xx64(value)
        {}

        constexpr explicit StateStorage(const Wyhash64& value) noexcept
            : wyhash64(value)
        {}

        constexpr explicit StateStorage(const SipHash24& value) noexcept
            : sip24(value)
        {}

        constexpr explicit StateStorage(const Sha256& value) noexcept
            : sha256(value)
        {}
    };

    [[nodiscard]] static constexpr HashAlgorithm sanitizeAlgorithm(const HashAlgorithm algorithm) noexcept
    {
        if (static_cast<gp::UInt8>(algorithm) >= static_cast<gp::UInt8>(HashAlgorithm::Count))
        {
            return HashAlgorithm::Fnv1a64;
        }
        return algorithm;
    }

    [[nodiscard]] static constexpr StateStorage makeStorage(const HashAlgorithm algorithm) noexcept
    {
        switch (algorithm)
        {
        case HashAlgorithm::Fnv1a32:
            return StateStorage(Fnv1a32());
        case HashAlgorithm::Fnv1a64:
            return StateStorage(Fnv1a64());
        case HashAlgorithm::Crc32:
            return StateStorage(Crc32());
        case HashAlgorithm::Crc32C:
            return StateStorage(Crc32C());
        case HashAlgorithm::Murmur3_32:
            return StateStorage(Murmur3_32());
        case HashAlgorithm::Murmur3_128:
            return StateStorage(Murmur3_128());
        case HashAlgorithm::XxHash32:
            return StateStorage(XxHash32());
        case HashAlgorithm::XxHash64:
            return StateStorage(XxHash64());
        case HashAlgorithm::Wyhash64:
            return StateStorage(Wyhash64());
        case HashAlgorithm::SipHash24:
            return StateStorage(SipHash24());
        case HashAlgorithm::Sha256:
            return StateStorage(Sha256());
        case HashAlgorithm::Count:
            // falls through
        default:
            return StateStorage(Fnv1a64());
        }
    }

    [[nodiscard]] static constexpr StateStorage
        makeSeededStorage(const HashAlgorithm algorithm, const gp::UInt64 seed) noexcept
    {
        switch (algorithm)
        {
        case HashAlgorithm::Fnv1a32:
            return StateStorage(Fnv1a32(static_cast<gp::UInt32>(seed)));
        case HashAlgorithm::Fnv1a64:
            return StateStorage(Fnv1a64(seed));
        case HashAlgorithm::Crc32:
            return StateStorage(Crc32(static_cast<gp::UInt32>(seed)));
        case HashAlgorithm::Crc32C:
            return StateStorage(Crc32C(static_cast<gp::UInt32>(seed)));
        case HashAlgorithm::Murmur3_32:
            return StateStorage(Murmur3_32(static_cast<gp::UInt32>(seed)));
        case HashAlgorithm::Murmur3_128:
            return StateStorage(Murmur3_128(seed));
        case HashAlgorithm::XxHash32:
            return StateStorage(XxHash32(static_cast<gp::UInt32>(seed)));
        case HashAlgorithm::XxHash64:
            return StateStorage(XxHash64(seed));
        case HashAlgorithm::Wyhash64:
            return StateStorage(Wyhash64(seed));
        case HashAlgorithm::SipHash24:
            return StateStorage(SipHash24(seed));
        case HashAlgorithm::Sha256:
            return StateStorage(Sha256(Hash256(seed, 0ULL, 0ULL, 0ULL)));
        case HashAlgorithm::Count:
            // falls through
        default:
            return StateStorage(Fnv1a64(seed));
        }
    }

    template <typename InVisitor>
    constexpr void dispatch(InVisitor&& visitor) noexcept
    {
        switch (m_algorithm)
        {
        case HashAlgorithm::Fnv1a32:
            visitor(m_storage.fnv32);
            break;
        case HashAlgorithm::Fnv1a64:
            visitor(m_storage.fnv64);
            break;
        case HashAlgorithm::Crc32:
            visitor(m_storage.crc32);
            break;
        case HashAlgorithm::Crc32C:
            visitor(m_storage.crc32c);
            break;
        case HashAlgorithm::Murmur3_32:
            visitor(m_storage.murmur32);
            break;
        case HashAlgorithm::Murmur3_128:
            visitor(m_storage.murmur128);
            break;
        case HashAlgorithm::XxHash32:
            visitor(m_storage.xx32);
            break;
        case HashAlgorithm::XxHash64:
            visitor(m_storage.xx64);
            break;
        case HashAlgorithm::Wyhash64:
            visitor(m_storage.wyhash64);
            break;
        case HashAlgorithm::SipHash24:
            visitor(m_storage.sip24);
            break;
        case HashAlgorithm::Sha256:
            visitor(m_storage.sha256);
            break;
        case HashAlgorithm::Count:
            // falls through
        default:
            visitor(m_storage.fnv64);
            break;
        }
    }

private:
    StateStorage m_storage;
    HashAlgorithm m_algorithm{ HashAlgorithm::Fnv1a64 };
};

/// @brief Computes a unified `HashDigest` at compile time or runtime using the selected @p algorithm.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Byte buffer pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return Tagged `HashDigest`.
[[nodiscard]] constexpr HashDigest
    computeHash(const HashAlgorithm algorithm, const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
{
    DynamicHasher hasher(algorithm);
    hasher.update(data, sizeInBytes);
    return hasher.finalize();
}

/// @brief Computes a unified `HashDigest` over a character buffer at compile time or runtime.
/// @tparam InCharT Character code-unit type.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Character sequence pointer.
/// @param[in] count Number of characters to hash.
/// @return Tagged `HashDigest`.
template <gp::concepts::IsCharacter InCharT>
[[nodiscard]] constexpr HashDigest
    computeHash(const HashAlgorithm algorithm, const InCharT* const data, const gp::USize count) noexcept
{
    DynamicHasher hasher(algorithm);
    hasher.update(data, count);
    return hasher.finalize();
}

/// @brief Computes a unified `HashDigest` over an untyped runtime buffer.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Buffer address.
/// @param[in] sizeInBytes Byte length.
/// @return Tagged `HashDigest`.
[[nodiscard]] inline HashDigest
    computeHash(const HashAlgorithm algorithm, const void* const data, const gp::USize sizeInBytes) noexcept
{
    DynamicHasher hasher(algorithm);
    hasher.update(data, sizeInBytes);
    return hasher.finalize();
}

/// @brief Computes a unified `HashDigest` over a string-like container or view (`constexpr`-capable).
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] str Character sequence to hash.
/// @return Tagged `HashDigest`.
template <concepts::IsCharacterSequence InStrT>
[[nodiscard]] constexpr HashDigest computeHash(const HashAlgorithm algorithm, const InStrT& str) noexcept
{
    DynamicHasher hasher(algorithm);
    hasher.update(str);
    return hasher.finalize();
}

/// @brief Computes a 32-bit digest for @p algorithm over an untyped runtime buffer.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Buffer address.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 32-bit digest (folds 64-bit and 128-bit algorithms down to 32 bits).
[[nodiscard]] inline gp::UInt32
    computeHash32(const HashAlgorithm algorithm, const void* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toUInt32();
}

/// @brief Computes a 32-bit digest for @p algorithm over a byte buffer (`constexpr`-capable).
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Byte pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 32-bit digest.
[[nodiscard]] constexpr gp::UInt32
    computeHash32(const HashAlgorithm algorithm, const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toUInt32();
}

/// @brief Computes a 32-bit digest for @p algorithm over a string-like object (`constexpr`-capable).
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] str String to hash.
/// @return 32-bit digest.
template <concepts::IsCharacterSequence InStrT>
[[nodiscard]] constexpr gp::UInt32 computeHash32(const HashAlgorithm algorithm, const InStrT& str) noexcept
{
    return computeHash(algorithm, str).toUInt32();
}

/// @brief Computes a 64-bit digest for @p algorithm over an untyped runtime buffer.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Buffer address.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 64-bit digest (zero-extends 32-bit digests or folds 128-bit digests).
[[nodiscard]] inline gp::UInt64
    computeHash64(const HashAlgorithm algorithm, const void* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toUInt64();
}

/// @brief Computes a 64-bit digest for @p algorithm over a byte buffer (`constexpr`-capable).
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Byte pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 64-bit digest.
[[nodiscard]] constexpr gp::UInt64
    computeHash64(const HashAlgorithm algorithm, const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toUInt64();
}

/// @brief Computes a 64-bit digest for @p algorithm over a string-like object (`constexpr`-capable).
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] str String to hash.
/// @return 64-bit digest.
template <concepts::IsCharacterSequence InStrT>
[[nodiscard]] constexpr gp::UInt64 computeHash64(const HashAlgorithm algorithm, const InStrT& str) noexcept
{
    return computeHash(algorithm, str).toUInt64();
}

/// @brief Computes a 128-bit digest for @p algorithm over an untyped runtime buffer.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Buffer address.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 128-bit digest (`{ value, 0 }` for 32/64-bit algorithms).
[[nodiscard]] inline Hash128
    computeHash128(const HashAlgorithm algorithm, const void* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toHash128();
}

/// @brief Computes a 128-bit digest for @p algorithm over a byte buffer (`constexpr`-capable).
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Byte pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 128-bit digest.
[[nodiscard]] constexpr Hash128
    computeHash128(const HashAlgorithm algorithm, const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toHash128();
}

/// @brief Computes a 128-bit digest for @p algorithm over a string-like object (`constexpr`-capable).
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] str String to hash.
/// @return 128-bit digest.
template <concepts::IsCharacterSequence InStrT>
[[nodiscard]] constexpr Hash128 computeHash128(const HashAlgorithm algorithm, const InStrT& str) noexcept
{
    return computeHash(algorithm, str).toHash128();
}

/// @brief Computes a 256-bit digest for @p algorithm over an untyped runtime buffer.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Buffer address.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 256-bit digest (`{ value, 0, ... }` for narrower algorithms).
[[nodiscard]] inline Hash256
    computeHash256(const HashAlgorithm algorithm, const void* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toHash256();
}

/// @brief Computes a 256-bit digest for @p algorithm over a byte buffer (`constexpr`-capable).
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] data Byte pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @return 256-bit digest.
[[nodiscard]] constexpr Hash256
    computeHash256(const HashAlgorithm algorithm, const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
{
    return computeHash(algorithm, data, sizeInBytes).toHash256();
}

/// @brief Computes a 256-bit digest for @p algorithm over a string-like object (`constexpr`-capable).
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
/// @param[in] algorithm Target `HashAlgorithm` enumerator.
/// @param[in] str String to hash.
/// @return 256-bit digest.
template <concepts::IsCharacterSequence InStrT>
[[nodiscard]] constexpr Hash256 computeHash256(const HashAlgorithm algorithm, const InStrT& str) noexcept
{
    return computeHash(algorithm, str).toHash256();
}

}   // namespace gp::hashing
