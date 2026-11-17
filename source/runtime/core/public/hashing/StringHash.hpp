// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashMix.hpp"
#include "hashing/Murmur3.hpp"
#include "hashing/StreamingHasher.hpp"
#include "hashing/XxHash.hpp"
#include <compare>
#include <type_traits>   // IWYU pragma: keep

namespace gp::hashing
{

/// @brief Fixed-size compile-time and runtime string hash handle modeled after Ubisoft Anvil StringIDs.
/// @details Occupies only the underlying digest size (`4` bytes for 32-bit, `8` bytes for 64-bit, `16` bytes for
///          128-bit) and is trivially copyable. Empty strings map to `kEmptyHash` (`0`) so default-constructed values
///          act as an invalid sentinel. Integer-backed variants convert implicitly to `DigestType` for `switch`
///          dispatch with compile-time literals such as `"AlbedoMap"_fnv64` or `"MeshHeader"_crc32`.
/// @tparam InHasher Concrete algorithm satisfying `concepts::IsStreamingHasher`.
/// @tparam InCase ASCII case sensitivity (`HashCase::CaseSensitive` or `HashCase::IgnoreCase`).
template <concepts::IsStreamingHasher InHasher = Fnv1a64, HashCase InCase = HashCase::CaseSensitive>
class BasicStringHash
{
public:
    /// @brief Hasher algorithm type used to compute digests.
    using HasherType = InHasher;

    /// @brief Digest output type (`gp::UInt32`, `gp::UInt64`, or `Hash128`).
    using DigestType = typename InHasher::DigestType;

    /// @brief Configured ASCII case-sensitivity mode.
    static constexpr HashCase kCaseSensitivity = InCase;

    /// @brief Canonical zero sentinel returned for default-constructed or empty string hashes.
    static constexpr DigestType kEmptyHash = DigestType{};

public:
    /// @brief Default-constructs an empty string hash (`isEmpty() == true`).
    [[nodiscard]] constexpr BasicStringHash() noexcept = default;

    /// @brief Wraps a precomputed raw digest integer or `Hash128`.
    /// @param[in] rawHash Precomputed digest value.
    [[nodiscard]] constexpr explicit BasicStringHash(const DigestType rawHash) noexcept
        : m_hash(rawHash)
    {}

    /// @brief Computes a string hash from a character pointer and explicit length (`constexpr`-capable).
    /// @tparam InCharT Character code-unit type.
    /// @param[in] str Character pointer (`nullptr` or `count == 0` yields `kEmptyHash`).
    /// @param[in] count Number of code units to hash.
    template <gp::concepts::IsCharacter InCharT>
    [[nodiscard]] constexpr explicit BasicStringHash(const InCharT* const str, const gp::USize count) noexcept
        : m_hash((str == nullptr || count == 0u) ? kEmptyHash : InHasher::hash(str, count, InCase))
    {}

    /// @brief Computes a string hash from a null-terminated C-string (`constexpr`-capable).
    /// @tparam InCharT Character code-unit type.
    /// @param[in] str Null-terminated string pointer (`nullptr` or `""` yields `kEmptyHash`).
    template <gp::concepts::IsCharacter InCharT>
    [[nodiscard]] constexpr explicit BasicStringHash(const InCharT* const str) noexcept
        : BasicStringHash(str, detail::cStringLength(str))
    {}

    /// @brief Computes a string hash from a string-like container or view (`StringView`, `String`, `FixedString`).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to hash (empty string yields `kEmptyHash`).
    template <concepts::IsCharacterSequence InStrT>
    [[nodiscard]] constexpr explicit BasicStringHash(const InStrT& str) noexcept
        : BasicStringHash(str.data(), str.size())
    {}

    /// @brief Constructs a `BasicStringHash` directly from a precomputed raw digest value.
    /// @param[in] rawHash Raw digest value.
    /// @return Constructed `BasicStringHash`.
    [[nodiscard]] static constexpr BasicStringHash fromRaw(const DigestType rawHash) noexcept
    {
        return BasicStringHash(rawHash);
    }

public:
    /// @brief Returns the underlying digest value.
    /// @return Computed digest, or `kEmptyHash` if empty.
    [[nodiscard]] constexpr DigestType value() const noexcept
    {
        return m_hash;
    }

    /// @brief Implicit conversion to `DigestType` for direct use in `switch` statements and `case` labels.
    /// @return Underlying digest value.
    [[nodiscard]] constexpr operator DigestType() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns a 64-bit hash key for `gp::hashing::hashOf` and hash-table lookups.
    /// @return 64-bit representation of the stored digest.
    [[nodiscard]] constexpr gp::UInt64 hash() const noexcept
    {
        if constexpr (gp::concepts::IsSameAs<DigestType, Hash128>)
        {
            return m_hash.foldTo64();
        }
        else if constexpr (gp::concepts::IsSameAs<DigestType, gp::UInt32>)
        {
            return static_cast<gp::UInt64>(prospectorMix32(m_hash));
        }
        else
        {
            return m_hash;
        }
    }

    /// @brief Streams the underlying digest into any `InTargetHasher`.
    /// @tparam InTargetHasher Target streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InTargetHasher>
    constexpr void hashAppend(InTargetHasher& hasher) const noexcept
    {
        hasher.append(m_hash);
    }

    /// @brief Checks whether this handle equals the empty sentinel (`value() == kEmptyHash`).
    /// @return True if empty, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_hash == kEmptyHash;
    }

    /// @brief Checks whether this handle holds a non-empty digest (`!isEmpty()`).
    /// @return True if non-empty, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return !isEmpty();
    }

    /// @brief Contextual boolean conversion returning `isValid()`.
    /// @return True if non-empty, false if empty.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Single-instruction equality comparison on the stored digest.
    /// @param[in] other Handle to compare against.
    /// @return True if digests are identical.
    [[nodiscard]] constexpr bool operator==(const BasicStringHash& other) const noexcept = default;

    /// @brief Three-way comparison on the stored digest.
    /// @param[in] other Handle to compare against.
    /// @return `std::strong_ordering` result.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const BasicStringHash& other) const noexcept = default;

private:
    DigestType m_hash{ kEmptyHash };
};

/// @brief Non-owning `StringView` paired with its precomputed `constexpr` digest (Decima/LLVM `CachedHashStringRef`).
/// @details Equality checks compare the cached integer digest in one instruction before inspecting characters, skipping
///          99.9%+ of string mismatches without dereferencing string memory while still verifying exact character
///          equality on matching hashes so collisions never produce false positives.
/// @tparam InHasher Concrete algorithm satisfying `concepts::IsStreamingHasher`.
/// @tparam InCase ASCII case sensitivity (`HashCase::CaseSensitive` or `HashCase::IgnoreCase`).
template <concepts::IsStreamingHasher InHasher = Fnv1a64, HashCase InCase = HashCase::CaseSensitive>
class HashedStringView
{
public:
    /// @brief Hasher algorithm type used to compute cached digests.
    using HasherType = InHasher;

    /// @brief Digest output type (`gp::UInt32`, `gp::UInt64`, or `Hash128`).
    using DigestType = typename InHasher::DigestType;

    /// @brief Typed `BasicStringHash` matching this view's algorithm and case mode.
    using StringHashType = BasicStringHash<InHasher, InCase>;

    /// @brief Configured ASCII case-sensitivity mode.
    static constexpr HashCase kCaseSensitivity = InCase;

public:
    /// @brief Default-constructs an empty view with zero digest.
    [[nodiscard]] constexpr HashedStringView() noexcept = default;

    /// @brief Constructs a `HashedStringView` from a `gp::StringView` and precomputes its digest (`constexpr`).
    /// @param[in] view Source string view.
    [[nodiscard]] constexpr HashedStringView(const gp::StringView view) noexcept
        : m_hash(StringHashType(view.data(), view.size()).value())
        , m_view(view)
    {}

    /// @brief Constructs a `HashedStringView` from a null-terminated C-string (`constexpr`).
    /// @param[in] str Null-terminated C-string (`nullptr` yields empty).
    [[nodiscard]] constexpr HashedStringView(const char* const str) noexcept
        : HashedStringView(gp::StringView(str))
    {}

    /// @brief Constructs a `HashedStringView` from a character pointer and explicit length (`constexpr`).
    /// @param[in] str Character pointer.
    /// @param[in] length Number of characters.
    [[nodiscard]] constexpr HashedStringView(const char* const str, const gp::USize length) noexcept
        : HashedStringView(gp::StringView(str, length))
    {}

    /// @brief Constructs a `HashedStringView` from an explicit precomputed digest and matching `StringView`.
    /// @details Useful when reading serialized string tables that already store precomputed digests on disk.
    /// @param[in] precomputedHash Precomputed digest matching @p view under `InHasher` and `InCase`.
    /// @param[in] view Non-owning string view.
    [[nodiscard]] constexpr HashedStringView(const DigestType precomputedHash, const gp::StringView view) noexcept
        : m_hash(precomputedHash)
        , m_view(view)
    {}

public:
    /// @brief Returns the cached digest value as a raw `DigestType`.
    /// @return Precomputed digest.
    [[nodiscard]] constexpr DigestType hash() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns the cached digest wrapped as a typed `BasicStringHash`.
    /// @return Typed `StringHashType`.
    [[nodiscard]] constexpr StringHashType stringHash() const noexcept
    {
        return StringHashType::fromRaw(m_hash);
    }

    /// @brief Returns the underlying non-owning `gp::StringView`.
    /// @return Underlying string view.
    [[nodiscard]] constexpr gp::StringView view() const noexcept
    {
        return m_view;
    }

    /// @brief Returns a pointer to the underlying character data (may be null if empty).
    /// @return Underlying character pointer.
    [[nodiscard]] constexpr const char* data() const noexcept
    {
        return m_view.data();
    }

    /// @brief Returns the byte length of the referenced string.
    /// @return Character count.
    [[nodiscard]] constexpr gp::USize size() const noexcept
    {
        return m_view.size();
    }

    /// @brief Checks whether the referenced string is empty.
    /// @return True if `m_view.isEmpty()`, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_view.isEmpty();
    }

    /// @brief Checks equality by testing the cached digest first, then comparing characters only on a digest match.
    /// @param[in] other Right-hand `HashedStringView`.
    /// @return True if both views have matching hashes and equal characters under `InCase`.
    [[nodiscard]] constexpr bool operator==(const HashedStringView& other) const noexcept
    {
        if (m_hash != other.m_hash || m_view.size() != other.m_view.size())
        {
            return false;
        }
        if constexpr (InCase == HashCase::CaseSensitive)
        {
            return m_view == other.m_view;
        }
        else
        {
            return m_view.equalsIgnoreCase(other.m_view);
        }
    }

    /// @brief Lexicographical three-way comparison honoring `InCase`.
    /// @param[in] other Right-hand `HashedStringView`.
    /// @return `std::strong_ordering` result.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const HashedStringView& other) const noexcept
    {
        if constexpr (InCase == HashCase::CaseSensitive)
        {
            return m_view <=> other.m_view;
        }
        else
        {
            const gp::Int32 cmp = m_view.compareIgnoreCase(other.m_view);
            if (cmp < 0)
            {
                return std::strong_ordering::less;
            }
            if (cmp > 0)
            {
                return std::strong_ordering::greater;
            }
            return std::strong_ordering::equal;
        }
    }

    /// @brief Streams the underlying characters with length framing into any `InTargetHasher`.
    /// @tparam InTargetHasher Target streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InTargetHasher>
    constexpr void hashAppend(InTargetHasher& hasher) const noexcept
    {
        hasher.update(m_view);
        hasher.append(static_cast<gp::UInt64>(m_view.size()));
    }

private:
    DigestType m_hash{};
    gp::StringView m_view{};
};

/// @section Canonical StringHash and HashedStringView Aliases

using StringHash32 = BasicStringHash<Fnv1a32, HashCase::CaseSensitive>;
using StringHash32IgnoreCase = BasicStringHash<Fnv1a32, HashCase::IgnoreCase>;
using StringHash64 = BasicStringHash<Fnv1a64, HashCase::CaseSensitive>;
using StringHash64IgnoreCase = BasicStringHash<Fnv1a64, HashCase::IgnoreCase>;

using CrcStringHash32 = BasicStringHash<Crc32, HashCase::CaseSensitive>;
using CrcStringHash32IgnoreCase = BasicStringHash<Crc32, HashCase::IgnoreCase>;
using Crc32CStringHash = BasicStringHash<Crc32C, HashCase::CaseSensitive>;

using MurmurStringHash32 = BasicStringHash<Murmur3_32, HashCase::CaseSensitive>;
using XxStringHash32 = BasicStringHash<XxHash32, HashCase::CaseSensitive>;
using XxStringHash64 = BasicStringHash<XxHash64, HashCase::CaseSensitive>;

using HashedStringView64 = HashedStringView<Fnv1a64, HashCase::CaseSensitive>;
using HashedStringView64IgnoreCase = HashedStringView<Fnv1a64, HashCase::IgnoreCase>;
using HashedStringView32 = HashedStringView<Fnv1a32, HashCase::CaseSensitive>;

static_assert(sizeof(StringHash32) == 4u, "StringHash32 must remain 4 bytes.");
static_assert(sizeof(StringHash64) == 8u, "StringHash64 must remain 8 bytes.");

}   // namespace gp::hashing

namespace gp::literals
{

/// @brief Compile-time 32-bit case-sensitive FNV-1a literal (`"Position"_fnv32`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `StringHash32`.
[[nodiscard]] consteval gp::hashing::StringHash32 operator""_fnv32(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::StringHash32(str, len);
}

/// @brief Compile-time 32-bit ASCII case-insensitive FNV-1a literal (`"Position"_fnv32i`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `StringHash32IgnoreCase`.
[[nodiscard]] consteval gp::hashing::StringHash32IgnoreCase
    operator""_fnv32i(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::StringHash32IgnoreCase(str, len);
}

/// @brief Compile-time 64-bit case-sensitive FNV-1a literal (`"Roughness"_fnv64`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `StringHash64`.
[[nodiscard]] consteval gp::hashing::StringHash64 operator""_fnv64(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::StringHash64(str, len);
}

/// @brief Compile-time 64-bit ASCII case-insensitive FNV-1a literal (`"Roughness"_fnv64i`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `StringHash64IgnoreCase`.
[[nodiscard]] consteval gp::hashing::StringHash64IgnoreCase
    operator""_fnv64i(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::StringHash64IgnoreCase(str, len);
}

/// @brief Compile-time IEEE 802.3 CRC-32 string literal (`"PNG"_crc32`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `CrcStringHash32`.
[[nodiscard]] consteval gp::hashing::CrcStringHash32
    operator""_crc32(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::CrcStringHash32(str, len);
}

/// @brief Compile-time ASCII case-insensitive IEEE 802.3 CRC-32 string literal (`"Shader"_crc32i`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `CrcStringHash32IgnoreCase`.
[[nodiscard]] consteval gp::hashing::CrcStringHash32IgnoreCase
    operator""_crc32i(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::CrcStringHash32IgnoreCase(str, len);
}

/// @brief Compile-time Castagnoli CRC-32C string literal (`"Chunk"_crc32c`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `Crc32CStringHash`.
[[nodiscard]] consteval gp::hashing::Crc32CStringHash
    operator""_crc32c(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::Crc32CStringHash(str, len);
}

/// @brief Compile-time MurmurHash3 32-bit string literal (`"Entity"_murmur32`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `MurmurStringHash32`.
[[nodiscard]] consteval gp::hashing::MurmurStringHash32
    operator""_murmur32(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::MurmurStringHash32(str, len);
}

/// @brief Compile-time XXH32 string literal (`"Packet"_xxh32`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `XxStringHash32`.
[[nodiscard]] consteval gp::hashing::XxStringHash32
    operator""_xxh32(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::XxStringHash32(str, len);
}

/// @brief Compile-time XXH64 string literal (`"ShaderCache"_xxh64`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `XxStringHash64`.
[[nodiscard]] consteval gp::hashing::XxStringHash64
    operator""_xxh64(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::XxStringHash64(str, len);
}

/// @brief Compile-time cached `HashedStringView64` literal (`"AlbedoMap"_hs`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `HashedStringView64`.
[[nodiscard]] consteval gp::hashing::HashedStringView64
    operator""_hs(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::HashedStringView64(str, len);
}

/// @brief Compile-time case-insensitive cached `HashedStringView64IgnoreCase` literal (`"AlbedoMap"_hsi`).
/// @param[in] str Literal character array.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `HashedStringView64IgnoreCase`.
[[nodiscard]] consteval gp::hashing::HashedStringView64IgnoreCase
    operator""_hsi(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::HashedStringView64IgnoreCase(str, len);
}

}   // namespace gp::literals
