// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/Murmur3.hpp"
#include "hashing/RuntimeHash.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/StreamingHasher.hpp"   // IWYU pragma: keep
#include "hashing/Wyhash.hpp"
#include "hashing/XxHash.hpp"
#include <compare>

namespace gp::hashing
{

namespace detail
{

/// @brief Slice referencing one normalized path segment within an unowned `StringView`.
struct PathSegmentSlice
{
    gp::USize offset{ 0u };
    gp::USize length{ 0u };
};

/// @brief Streams a lexically normalized asset or VFS path into any streaming hasher without allocating.
/// @tparam InSinkHasher Concrete or dynamic streaming hasher exposing `.update()`, `.updateIgnoreCase()`, and
///                      `.updateByte()`.
/// @param[in,out] hasher Target accumulator.
/// @param[in] path Raw OS or VFS path string.
/// @param[in] hashCase Controls whether ASCII letters fold to lowercase before hashing.
template <typename InSinkHasher>
constexpr void
    normalizeAndStreamPathImpl(InSinkHasher& hasher, const gp::StringView path, const HashCase hashCase) noexcept
{
    if (path.isEmpty())
    {
        return;
    }

    constexpr gp::USize kMaxSegments = 64u;
    PathSegmentSlice segments[kMaxSegments]{};
    gp::USize segmentCount = 0u;

    const char* const raw = path.data();
    const gp::USize len = path.size();
    const bool isAbsolute = (raw[0] == '/' || raw[0] == '\\');

    gp::USize cursor = 0u;
    while (cursor < len)
    {
        while (cursor < len && (raw[cursor] == '/' || raw[cursor] == '\\'))
        {
            ++cursor;
        }
        if (cursor >= len)
        {
            break;
        }

        const gp::USize segStart = cursor;
        while (cursor < len && raw[cursor] != '/' && raw[cursor] != '\\')
        {
            ++cursor;
        }
        const gp::USize segLen = cursor - segStart;

        if (segLen == 1u && raw[segStart] == '.')
        {
            continue;
        }

        if (segLen == 2u && raw[segStart] == '.' && raw[segStart + 1u] == '.')
        {
            if (segmentCount > 0u)
            {
                const PathSegmentSlice& top = segments[segmentCount - 1u];
                const bool topIsParent = (top.length == 2u && raw[top.offset] == '.' && raw[top.offset + 1u] == '.');
                if (!topIsParent)
                {
                    --segmentCount;
                    continue;
                }
            }
            if (!isAbsolute && segmentCount < kMaxSegments)
            {
                segments[segmentCount++] = PathSegmentSlice{ segStart, segLen };
            }
            continue;
        }

        if (segmentCount < kMaxSegments)
        {
            segments[segmentCount++] = PathSegmentSlice{ segStart, segLen };
        }
    }

    if (isAbsolute)
    {
        hasher.updateByte(static_cast<gp::UInt8>('/'));
    }

    for (gp::USize i = 0u; i < segmentCount; ++i)
    {
        if (i > 0u)
        {
            hasher.updateByte(static_cast<gp::UInt8>('/'));
        }
        const PathSegmentSlice& seg = segments[i];
        if (hashCase == HashCase::CaseSensitive)
        {
            hasher.update(raw + seg.offset, seg.length);
        }
        else
        {
            hasher.updateIgnoreCase(raw + seg.offset, seg.length);
        }
    }
}

}   // namespace detail

/// @brief Normalizes `\` separators, collapses `.` / `..`, trims redundant slashes, and streams into @p hasher.
/// @tparam InHasher Concrete hasher satisfying `concepts::IsStreamingHasher`.
/// @param[in,out] hasher Target accumulator.
/// @param[in] path Raw OS or VFS path view.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::IgnoreCase` for cross-platform parity).
template <concepts::IsStreamingHasher InHasher>
constexpr void normalizeAndStreamPath(
    InHasher& hasher, const gp::StringView path, const HashCase hashCase = HashCase::IgnoreCase
) noexcept
{
    detail::normalizeAndStreamPathImpl(hasher, path, hashCase);
}

/// @brief Normalizes and streams a path into a runtime `DynamicHasher`.
/// @param[in,out] hasher Target dynamic accumulator.
/// @param[in] path Raw OS or VFS path view.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::IgnoreCase`).
constexpr void normalizeAndStreamPath(
    DynamicHasher& hasher, const gp::StringView path, const HashCase hashCase = HashCase::IgnoreCase
) noexcept
{
    detail::normalizeAndStreamPathImpl(hasher, path, hashCase);
}

/// @brief Hashes a lexically normalized asset or VFS path using compile-time algorithm @p InHasher.
/// @details Modeled after Decima and Anvil asset-path normalization so `"Textures\\Hero/../Hero/./Body.dds"` and
///          `"textures/hero/body.dds"` produce identical digests across platforms.
/// @tparam InHasher Concrete hasher satisfying `concepts::IsStreamingHasher` (defaults to `Fnv1a64`).
/// @param[in] path Raw path view.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::IgnoreCase`).
/// @param[in] seed Optional initial seed (defaults to `InHasher::kDefaultSeed`).
/// @return Computed path digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64>
[[nodiscard]] constexpr typename InHasher::DigestType hashPath(
    const gp::StringView path,
    const HashCase hashCase = HashCase::IgnoreCase,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    if (path.isEmpty())
    {
        return typename InHasher::DigestType{};
    }
    InHasher hasher(seed);
    normalizeAndStreamPath(hasher, path, hashCase);
    return hasher.finalize();
}

/// @brief Hashes a lexically normalized path from a null-terminated C-string.
/// @tparam InHasher Concrete hasher satisfying `concepts::IsStreamingHasher` (defaults to `Fnv1a64`).
/// @param[in] path Null-terminated path pointer.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::IgnoreCase`).
/// @param[in] seed Optional initial seed.
/// @return Computed path digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64>
[[nodiscard]] constexpr typename InHasher::DigestType hashPath(
    const char* const path,
    const HashCase hashCase = HashCase::IgnoreCase,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return hashPath<InHasher>(gp::StringView(path), hashCase, seed);
}

/// @brief Hashes a lexically normalized path from a string-like container (`gp::String`, `gp::FixedString`, etc.).
/// @tparam InHasher Concrete hasher satisfying `concepts::IsStreamingHasher` (defaults to `Fnv1a64`).
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence` other than `gp::StringView`.
/// @param[in] path Path string object.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::IgnoreCase`).
/// @param[in] seed Optional initial seed.
/// @return Computed path digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64, concepts::IsCharacterSequence InStrT>
requires(!gp::concepts::IsSameAs<std::remove_cvref_t<InStrT>, gp::StringView>)
[[nodiscard]] constexpr typename InHasher::DigestType hashPath(
    const InStrT& path,
    const HashCase hashCase = HashCase::IgnoreCase,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return hashPath<InHasher>(gp::StringView(path.data(), path.size()), hashCase, seed);
}

/// @brief Hashes a lexically normalized path using a runtime-selected `HashAlgorithm`.
/// @param[in] algorithm Target algorithm identifier.
/// @param[in] path Raw path view.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::IgnoreCase`).
/// @return Tagged `HashDigest`.
[[nodiscard]] constexpr HashDigest hashPath(
    const HashAlgorithm algorithm, const gp::StringView path, const HashCase hashCase = HashCase::IgnoreCase
) noexcept
{
    DynamicHasher hasher(algorithm);
    if (path.isEmpty())
    {
        return HashDigest(gp::UInt64{ 0ULL }, algorithm);
    }
    normalizeAndStreamPath(hasher, path, hashCase);
    return hasher.finalize();
}

/// @brief Lightweight canonical asset-path digest handle modeled after Decima path digests and Anvil resource IDs.
/// @tparam InHasher Concrete hasher satisfying `concepts::IsStreamingHasher`.
/// @tparam InCase ASCII case-sensitivity mode (`HashCase::IgnoreCase` by default).
template <concepts::IsStreamingHasher InHasher = Fnv1a64, HashCase InCase = HashCase::IgnoreCase>
class BasicPathHash
{
public:
    /// @brief Underlying hasher type.
    using HasherType = InHasher;

    /// @brief Digest output type (`gp::UInt32`, `gp::UInt64`, `Hash128`, or `Hash256`).
    using DigestType = typename InHasher::DigestType;

    /// @brief Configured ASCII case-sensitivity mode.
    static constexpr HashCase kCaseSensitivity = InCase;

    /// @brief Zero sentinel representing an empty or invalid path.
    static constexpr DigestType kEmptyHash = DigestType{};

public:
    /// @brief Default-constructs an empty path hash (`isEmpty() == true`).
    [[nodiscard]] constexpr BasicPathHash() noexcept = default;

    /// @brief Wraps a precomputed raw path digest.
    /// @param[in] rawHash Precomputed digest value.
    [[nodiscard]] constexpr explicit BasicPathHash(const DigestType rawHash) noexcept
        : m_hash(rawHash)
    {}

    /// @brief Normalizes and hashes a character pointer and explicit length (`constexpr`-capable).
    /// @param[in] path Character pointer.
    /// @param[in] count Number of characters.
    [[nodiscard]] constexpr explicit BasicPathHash(const char* const path, const gp::USize count) noexcept
        : m_hash(hashPath<InHasher>(gp::StringView(path, count), InCase))
    {}

    /// @brief Normalizes and hashes a null-terminated C-string path (`constexpr`-capable).
    /// @param[in] path Null-terminated path literal or string.
    [[nodiscard]] constexpr explicit BasicPathHash(const char* const path) noexcept
        : m_hash(hashPath<InHasher>(gp::StringView(path), InCase))
    {}

    /// @brief Normalizes and hashes a `gp::StringView` or string container.
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] path Path string object.
    template <concepts::IsCharacterSequence InStrT>
    [[nodiscard]] constexpr explicit BasicPathHash(const InStrT& path) noexcept
        : m_hash(hashPath<InHasher>(gp::StringView(path.data(), path.size()), InCase))
    {}

    /// @brief Constructs a `BasicPathHash` directly from a precomputed raw digest.
    /// @param[in] rawHash Raw digest value.
    /// @return Wrapped `BasicPathHash`.
    [[nodiscard]] static constexpr BasicPathHash fromRaw(const DigestType rawHash) noexcept
    {
        return BasicPathHash(rawHash);
    }

public:
    /// @brief Returns the underlying path digest.
    /// @return Digest value, or `kEmptyHash` when empty.
    [[nodiscard]] constexpr DigestType value() const noexcept
    {
        return m_hash;
    }

    /// @brief Implicit conversion to `DigestType` for `switch` dispatch on integer-backed path hashes.
    /// @return Underlying digest value.
    [[nodiscard]] constexpr operator DigestType() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns a 64-bit hash key for `gp::hashing::hashOf` and hash tables.
    /// @return 64-bit hash representation.
    [[nodiscard]] constexpr gp::UInt64 hash() const noexcept
    {
        if constexpr (gp::concepts::IsSameAs<DigestType, Hash256> || gp::concepts::IsSameAs<DigestType, Hash128>)
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

    /// @brief Streams the underlying digest into any target streaming hasher.
    /// @tparam InTargetHasher Target streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InTargetHasher>
    constexpr void hashAppend(InTargetHasher& hasher) const noexcept
    {
        hasher.append(m_hash);
    }

    /// @brief Checks whether the path hash equals `kEmptyHash`.
    /// @return True if empty, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_hash == kEmptyHash;
    }

    /// @brief Checks whether the path hash holds a non-empty digest.
    /// @return True if non-empty, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return !isEmpty();
    }

    /// @brief Contextual boolean conversion returning `isValid()`.
    /// @return True if valid, false if empty.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Exact digest equality comparison.
    [[nodiscard]] constexpr bool operator==(const BasicPathHash& other) const noexcept = default;

    /// @brief Three-way digest comparison.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const BasicPathHash& other) const noexcept = default;

private:
    DigestType m_hash{ kEmptyHash };
};

/// @section Canonical PathHash Aliases

using PathHash32 = BasicPathHash<Fnv1a32, HashCase::IgnoreCase>;
using PathHash64 = BasicPathHash<Fnv1a64, HashCase::IgnoreCase>;
using PathHash128 = BasicPathHash<Murmur3_128, HashCase::IgnoreCase>;
using PathXxHash64 = BasicPathHash<XxHash64, HashCase::IgnoreCase>;
using PathWyHash64 = BasicPathHash<Wyhash64, HashCase::IgnoreCase>;
using PathDigest256 = BasicPathHash<Sha256, HashCase::IgnoreCase>;

static_assert(sizeof(PathHash32) == 4u, "PathHash32 must remain 4 bytes.");
static_assert(sizeof(PathHash64) == 8u, "PathHash64 must remain 8 bytes.");
static_assert(sizeof(PathHash128) == 16u, "PathHash128 must remain 16 bytes.");
static_assert(sizeof(PathDigest256) == 32u, "PathDigest256 must remain 32 bytes.");

}   // namespace gp::hashing

namespace gp::literals
{

/// @brief Compile-time normalized 64-bit asset-path literal (`"Textures\\Hero/Body.dds"_path64`).
/// @param[in] str Literal path string.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `PathHash64`.
[[nodiscard]] consteval gp::hashing::PathHash64 operator""_path64(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::PathHash64(str, len);
}

/// @brief Compile-time normalized 128-bit Decima-style asset-path digest literal (`"Meshes\\Hero.mesh"_path128`).
/// @param[in] str Literal path string.
/// @param[in] len Byte length excluding null terminator.
/// @return Compile-time `PathHash128`.
[[nodiscard]] consteval gp::hashing::PathHash128 operator""_path128(const char* const str, const gp::USize len) noexcept
{
    return gp::hashing::PathHash128(str, len);
}

}   // namespace gp::literals
