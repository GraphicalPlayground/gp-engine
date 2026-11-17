// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/HashMix.hpp"
#include "hashing/StreamingHasher.hpp"
#include <ranges>
#include <type_traits>

namespace gp::hashing
{

namespace detail
{

/// @brief Matches pair-like engine types exposing `FirstType`, `SecondType`, `.first`, and `.second`.
template <typename InT>
concept IsPairLike = requires(const InT& pair) {
    typename InT::FirstType;
    typename InT::SecondType;
    pair.first;
    pair.second;
};

/// @brief Matches optional-like engine types exposing `ValueType`, `.hasValue()`, and `.value()`.
template <typename InT>
concept IsOptionalLike = requires(const InT& opt) {
    typename InT::ValueType;
    {
        opt.hasValue()
    } -> gp::concepts::IsConvertibleTo<bool>;
    opt.value();
};

/// @brief Matches interval-like engine types exposing `ElementType`, `.min`, `.max`, and `.isValid()`.
template <typename InT>
concept IsIntervalLike = requires(const InT& iv) {
    typename InT::ElementType;
    iv.min;
    iv.max;
    {
        iv.isValid()
    } -> gp::concepts::IsConvertibleTo<bool>;
};

/// @brief Matches smart-pointer types exposing `ElementType`, `.get()`, and `operator bool()` without `.getHash()`.
template <typename InT>
concept IsSmartPointerLike = requires(const InT& ptr) {
    typename InT::ElementType;
    ptr.get();
    static_cast<bool>(ptr);
} && !concepts::HasMemberGetHash<InT> && !concepts::HasMemberHash<InT> && !concepts::IsCharacterSequence<InT>;

/// @brief Matches contiguous sized ranges (`gp::Array`, `gp::VectorView`, `gp::Vector`) excluding strings and C-arrays.
template <typename InT>
concept IsContiguousElementRange = gp::concepts::IsContiguousRange<InT> && gp::concepts::IsSizedRange<InT> &&
                                   !concepts::IsCharacterSequence<InT> && !gp::concepts::IsArray<InT>;

/// @brief Detects whether `HashTraits<InT>::hash(value)` is specialized.
template <typename InT>
concept HasHashTraitsHash = requires(const std::remove_cvref_t<InT>& value) {
    {
        HashTraits<std::remove_cvref_t<InT>>::hash(value)
    } -> gp::concepts::IsIntegral;
};

}   // namespace detail

/// @brief Primary `HashTraits` customization point (unspecialized by default).
/// @details Specialize `HashTraits<MyType>` in `gp::hashing` to plug custom types into `hashOf()`, `hashValues()`, and
///          `StreamingHasherBase::append()` across all algorithms. Providing `append(hasher, value)` automatically
///          enables `hashOf<AnyAlgorithm>(value)`.
/// @tparam InT Unqualified value type.
/// @tparam InEnable SFINAE / concept enablement parameter.
template <typename InT, typename InEnable>
struct HashTraits
{};

/// @brief `HashTraits` specialization for 128-bit `Hash128` digests.
template <>
struct HashTraits<Hash128>
{
    /// @brief Folds @p value into a 64-bit hash key via `Hash128::foldTo64()`.
    /// @param[in] value Source 128-bit digest.
    /// @return 64-bit mixed hash.
    [[nodiscard]] static constexpr gp::UInt64 hash(const Hash128& value) noexcept
    {
        return value.foldTo64();
    }

    /// @brief Streams both 64-bit halves of @p value into @p hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    /// @param[in] value Source 128-bit digest.
    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const Hash128& value) noexcept
    {
        hasher.append(value.low);
        hasher.append(value.high);
    }
};

/// @brief `HashTraits` specialization for runtime `HashDigest` values.
template <>
struct HashTraits<HashDigest>
{
    /// @brief Converts @p value to a 64-bit hash via `HashDigest::toUInt64()`.
    /// @param[in] value Source runtime digest.
    /// @return 64-bit hash value.
    [[nodiscard]] static constexpr gp::UInt64 hash(const HashDigest& value) noexcept
    {
        return value.toUInt64();
    }

    /// @brief Streams digest bits, width, and originating algorithm into @p hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    /// @param[in] value Source runtime digest.
    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const HashDigest& value) noexcept
    {
        hasher.append(value.bits);
        hasher.append(value.width);
        hasher.append(value.algorithm);
    }
};

/// @brief Computes a 64-bit hash for any hashable value using the engine's canonical fast path.
/// @details Uses `HashTraits<T>::hash`, member `.hash()` / `.getHash()`, FNV-1a for strings, `splitMix64` for
///          integers/normalized floats, `hashPointer` for addresses, or structural field hashing for `Pair`,
///          `Optional`, `Interval`, smart pointers, and contiguous arrays/ranges.
/// @tparam InT Value type to hash.
/// @param[in] value Input value.
/// @return 64-bit hash value.
template <typename InT>
[[nodiscard]] constexpr gp::UInt64 hashOf(const InT& value) noexcept
{
    using DecayedT = std::remove_cvref_t<InT>;

    if constexpr (detail::HasHashTraitsHash<DecayedT>)
    {
        return static_cast<gp::UInt64>(HashTraits<DecayedT>::hash(value));
    }
    else if constexpr (detail::HasHashTraitsAppend<DecayedT, Fnv1a64>)
    {
        Fnv1a64 hasher;
        HashTraits<DecayedT>::append(hasher, value);
        return hasher.finalize();
    }
    else if constexpr (concepts::HasMemberHash<DecayedT>)
    {
        return static_cast<gp::UInt64>(value.hash());
    }
    else if constexpr (concepts::HasMemberGetHash<DecayedT>)
    {
        return static_cast<gp::UInt64>(value.getHash());
    }
    else if constexpr (concepts::HasMemberHashAppend<DecayedT, Fnv1a64>)
    {
        Fnv1a64 hasher;
        value.hashAppend(hasher);
        return hasher.finalize();
    }
    else if constexpr (concepts::HasAdlHashAppend<DecayedT, Fnv1a64>)
    {
        Fnv1a64 hasher;
        gpHashAppend(hasher, value);
        return hasher.finalize();
    }
    else if constexpr (concepts::IsCharacterSequence<DecayedT>)
    {
        return Fnv1a64::hash(value.data(), value.size());
    }
    else if constexpr (gp::concepts::IsBool<DecayedT>)
    {
        return hashInt64(static_cast<gp::UInt64>(value ? 1ULL : 0ULL));
    }
    else if constexpr (gp::concepts::IsIntegral<DecayedT>)
    {
        return hashInt64(value);
    }
    else if constexpr (gp::concepts::IsEnum<DecayedT>)
    {
        return hashInt64(static_cast<std::underlying_type_t<DecayedT>>(value));
    }
    else if constexpr (gp::concepts::IsFloatingPoint<DecayedT>)
    {
        if constexpr (sizeof(DecayedT) == 4u)
        {
            return hashFloat(static_cast<gp::Float32>(value));
        }
        else if constexpr (sizeof(DecayedT) == 8u)
        {
            return hashDouble(static_cast<gp::Float64>(value));
        }
    }
    else if constexpr (gp::concepts::IsNullPointer<DecayedT>)
    {
        return 0ULL;
    }
    else if constexpr (gp::concepts::IsPointer<DecayedT>)
    {
        using PointeeT = std::remove_cv_t<std::remove_pointer_t<DecayedT>>;
        if constexpr (gp::concepts::IsCharacter<PointeeT>)
        {
            return Fnv1a64::hashCString(value);
        }
        else
        {
            return hashPointer(value);
        }
    }
    else if constexpr (gp::concepts::IsArray<DecayedT>)
    {
        using ElemT = std::remove_cv_t<std::remove_extent_t<DecayedT>>;
        constexpr gp::USize kCount = std::extent_v<DecayedT>;
        if constexpr (gp::concepts::IsCharacter<ElemT>)
        {
            return Fnv1a64::hash(value, detail::boundedCharLength(value, kCount));
        }
        else
        {
            gp::UInt64 seed = static_cast<gp::UInt64>(kCount);
            for (gp::USize i = 0u; i < kCount; ++i)
            {
                seed = hashCombine(seed, hashOf(value[i]));
            }
            return seed;
        }
    }
    else if constexpr (detail::IsPairLike<DecayedT>)
    {
        return hashCombine(hashOf(value.first), hashOf(value.second));
    }
    else if constexpr (detail::IsOptionalLike<DecayedT>)
    {
        if (!value.hasValue())
        {
            return 0ULL;
        }
        return hashCombine(0x9E3779B97F4A7C15ULL, hashOf(value.value()));
    }
    else if constexpr (detail::IsIntervalLike<DecayedT>)
    {
        return hashCombine(hashOf(value.min), hashOf(value.max));
    }
    else if constexpr (detail::IsSmartPointerLike<DecayedT>)
    {
        return hashPointer(value.get());
    }
    else if constexpr (detail::IsContiguousElementRange<DecayedT>)
    {
        using ValueT = std::ranges::range_value_t<DecayedT>;
        if constexpr (sizeof(ValueT) == 1u && concepts::IsByteLike<ValueT>)
        {
            return Fnv1a64::hash(std::ranges::data(value), std::ranges::size(value));
        }
        else
        {
            gp::UInt64 seed = static_cast<gp::UInt64>(std::ranges::size(value));
            for (const ValueT& elem: value)
            {
                seed = hashCombine(seed, hashOf(elem));
            }
            return seed;
        }
    }
    else
    {
        static_assert(sizeof(DecayedT) == 0u, "Type is not hashable with gp::hashing::hashOf.");
        return 0ULL;
    }
}

/// @brief Computes a digest of @p value using an explicit streaming algorithm (`Crc32`, `Fnv1a32`, `XxHash64`, etc.).
/// @details Character sequences, null-terminated C-strings, character array literals, and byte arrays feed raw bytes
///          directly into @p InHasher without extra framing so `hashOf<Crc32>("123456789") == Crc32::hash(...)`.
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @tparam InT Value type to hash.
/// @param[in] value Input value.
/// @param[in] seed Optional initial seed (defaults to `InHasher::kDefaultSeed`).
/// @return Digest of type `typename InHasher::DigestType`.
template <concepts::IsStreamingHasher InHasher, typename InT>
[[nodiscard]] constexpr typename InHasher::DigestType
    hashOf(const InT& value, const typename InHasher::DigestType seed = InHasher::kDefaultSeed) noexcept
{
    using DecayedT = std::remove_cvref_t<InT>;

    if constexpr (concepts::IsCharacterSequence<DecayedT>)
    {
        return InHasher::hash(value.data(), value.size(), seed);
    }
    else if constexpr (gp::concepts::IsPointer<DecayedT>)
    {
        using PointeeT = std::remove_cv_t<std::remove_pointer_t<DecayedT>>;
        if constexpr (gp::concepts::IsCharacter<PointeeT>)
        {
            return InHasher::hashCString(value, seed);
        }
        else
        {
            InHasher hasher(seed);
            hasher.append(value);
            return hasher.finalize();
        }
    }
    else if constexpr (gp::concepts::IsArray<DecayedT>)
    {
        using ElemT = std::remove_cv_t<std::remove_extent_t<DecayedT>>;
        constexpr gp::USize kCount = std::extent_v<DecayedT>;
        if constexpr (gp::concepts::IsCharacter<ElemT>)
        {
            return InHasher::hash(value, detail::boundedCharLength(value, kCount), seed);
        }
        else if constexpr (sizeof(ElemT) == 1u && concepts::IsByteLike<ElemT>)
        {
            return InHasher::hash(value, kCount, seed);
        }
        else
        {
            InHasher hasher(seed);
            hasher.append(value);
            return hasher.finalize();
        }
    }
    else if constexpr (detail::IsContiguousElementRange<DecayedT>)
    {
        using ValueT = std::ranges::range_value_t<DecayedT>;
        if constexpr (sizeof(ValueT) == 1u && concepts::IsByteLike<ValueT>)
        {
            const gp::USize count = std::ranges::size(value);
            if constexpr (
                gp::concepts::IsSameAs<ValueT, gp::UInt8> || gp::concepts::IsSameAs<ValueT, gp::Byte> ||
                gp::concepts::IsSameAs<ValueT, char>
            )
            {
                return InHasher::hash(std::ranges::data(value), count, seed);
            }
            else
            {
                InHasher hasher(seed);
                const ValueT* const ptr = std::ranges::data(value);
                for (gp::USize i = 0u; i < count; ++i)
                {
                    hasher.updateByte(static_cast<gp::UInt8>(ptr[i]));
                }
                return hasher.finalize();
            }
        }
        else
        {
            InHasher hasher(seed);
            hasher.append(value);
            return hasher.finalize();
        }
    }
    else
    {
        InHasher hasher(seed);
        hasher.append(value);
        return hasher.finalize();
    }
}

/// @brief Streams a pack of heterogeneous values through @p InHasher into one unified digest.
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher` (defaults to `Fnv1a64`).
/// @tparam InArgs Types of the input fields.
/// @param[in] args Input values appended sequentially.
/// @return Combined digest of type `typename InHasher::DigestType`.
template <concepts::IsStreamingHasher InHasher = Fnv1a64, typename... InArgs>
[[nodiscard]] constexpr typename InHasher::DigestType hashValues(const InArgs&... args) noexcept
{
    InHasher hasher;
    hasher.appendAll(args...);
    return hasher.finalize();
}

/// @brief Unified one-shot byte buffer hash parameterized by algorithm (defaults to `Fnv1a64`).
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @param[in] data Byte buffer pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @param[in] seed Optional initial seed (defaults to `InHasher::kDefaultSeed`).
/// @return Computed digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64>
[[nodiscard]] constexpr typename InHasher::DigestType hashBytes(
    const gp::UInt8* const data,
    const gp::USize sizeInBytes,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return InHasher::hash(data, sizeInBytes, seed);
}

/// @brief Unified one-shot byte buffer hash over `gp::Byte` (`std::byte`) pointers.
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @param[in] data `gp::Byte` buffer pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @param[in] seed Optional initial seed.
/// @return Computed digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64>
[[nodiscard]] constexpr typename InHasher::DigestType hashBytes(
    const gp::Byte* const data,
    const gp::USize sizeInBytes,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return InHasher::hash(data, sizeInBytes, seed);
}

/// @brief Unified one-shot byte buffer hash over untyped runtime memory pointers.
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @param[in] data Untyped buffer pointer.
/// @param[in] sizeInBytes Number of bytes to hash.
/// @param[in] seed Optional initial seed.
/// @return Computed digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64>
[[nodiscard]] inline typename InHasher::DigestType hashBytes(
    const void* const data,
    const gp::USize sizeInBytes,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return InHasher::hash(data, sizeInBytes, seed);
}

/// @brief Unified one-shot string hash parameterized by algorithm (defaults to `Fnv1a64`).
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
/// @param[in] str Character sequence to hash.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::CaseSensitive`).
/// @param[in] seed Optional initial seed.
/// @return Computed digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64, concepts::IsCharacterSequence InStrT>
[[nodiscard]] constexpr typename InHasher::DigestType hashString(
    const InStrT& str,
    const HashCase hashCase = HashCase::CaseSensitive,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return InHasher::hash(str, hashCase, seed);
}

/// @brief Unified one-shot null-terminated C-string hash parameterized by algorithm (defaults to `Fnv1a64`).
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @tparam InCharT Character code-unit type.
/// @param[in] str Null-terminated string pointer.
/// @param[in] hashCase ASCII case-sensitivity mode (defaults to `HashCase::CaseSensitive`).
/// @param[in] seed Optional initial seed.
/// @return Computed digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64, gp::concepts::IsCharacter InCharT>
[[nodiscard]] constexpr typename InHasher::DigestType hashString(
    const InCharT* const str,
    const HashCase hashCase = HashCase::CaseSensitive,
    const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return (hashCase == HashCase::CaseSensitive) ? InHasher::hashCString(str, seed)
                                                 : InHasher::hashCStringIgnoreCase(str, seed);
}

/// @brief Unified one-shot ASCII case-insensitive string hash parameterized by algorithm.
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
/// @param[in] str Character sequence to fold and hash.
/// @param[in] seed Optional initial seed.
/// @return Computed case-insensitive digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64, concepts::IsCharacterSequence InStrT>
[[nodiscard]] constexpr typename InHasher::DigestType
    hashStringIgnoreCase(const InStrT& str, const typename InHasher::DigestType seed = InHasher::kDefaultSeed) noexcept
{
    return InHasher::hashIgnoreCase(str, seed);
}

/// @brief Unified one-shot ASCII case-insensitive C-string hash parameterized by algorithm.
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @tparam InCharT Character code-unit type.
/// @param[in] str Null-terminated string pointer.
/// @param[in] seed Optional initial seed.
/// @return Computed case-insensitive digest.
template <concepts::IsStreamingHasher InHasher = Fnv1a64, gp::concepts::IsCharacter InCharT>
[[nodiscard]] constexpr typename InHasher::DigestType hashStringIgnoreCase(
    const InCharT* const str, const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    return InHasher::hashCStringIgnoreCase(str, seed);
}

/// @brief Hashes an arbitrary iterator range `[first, last)` sequentially through @p InHasher.
/// @tparam InHasher Concrete hasher type satisfying `concepts::IsStreamingHasher`.
/// @tparam InIteratorT Forward/input iterator type.
/// @param[in] first Beginning of range.
/// @param[in] last End of range.
/// @param[in] seed Optional initial seed.
/// @return Combined digest over all elements in `[first, last)`.
template <concepts::IsStreamingHasher InHasher = Fnv1a64, typename InIteratorT>
[[nodiscard]] constexpr typename InHasher::DigestType hashRange(
    InIteratorT first, InIteratorT last, const typename InHasher::DigestType seed = InHasher::kDefaultSeed
) noexcept
{
    InHasher hasher(seed);
    gp::UInt64 count = 0ULL;
    for (; first != last; ++first)
    {
        hasher.append(*first);
        ++count;
    }
    hasher.append(count);
    return hasher.finalize();
}

/// @brief Combines @p seed with one or more arbitrary hashable values using `hashOf` and `hashCombine`.
/// @details Excluded when every input argument is already `gp::UInt64` so raw-digest overloads in `HashCombine.hpp`
///          match directly without re-mixing.
/// @tparam InFirstT First value type.
/// @tparam InRestT Remaining value types.
/// @param[in] seed Accumulated 64-bit hash state.
/// @param[in] first First value to hash and combine.
/// @param[in] rest Additional values folded left-to-right.
/// @return Combined 64-bit hash.
template <typename InFirstT, typename... InRestT>
requires(
    !(gp::concepts::IsSameAs<std::remove_cvref_t<InFirstT>, gp::UInt64> &&
      (gp::concepts::IsSameAs<std::remove_cvref_t<InRestT>, gp::UInt64> && ...))
)
[[nodiscard]] constexpr gp::UInt64 hashCombine(gp::UInt64 seed, const InFirstT& first, const InRestT&... rest) noexcept
{
    seed = hashCombine(seed, hashOf(first));
    ((seed = hashCombine(seed, hashOf(rest))), ...);
    return seed;
}

/// @brief Combines @p seed with one or more arbitrary hashable values using `hashOf` and `hashCombineFast`.
/// @tparam InFirstT First value type.
/// @tparam InRestT Remaining value types.
/// @param[in] seed Accumulated 64-bit hash state.
/// @param[in] first First value to hash and combine.
/// @param[in] rest Additional values folded left-to-right.
/// @return Combined 64-bit hash.
template <typename InFirstT, typename... InRestT>
requires(
    !(gp::concepts::IsSameAs<std::remove_cvref_t<InFirstT>, gp::UInt64> &&
      (gp::concepts::IsSameAs<std::remove_cvref_t<InRestT>, gp::UInt64> && ...))
) [[nodiscard]] constexpr gp::UInt64
    hashCombineFast(gp::UInt64 seed, const InFirstT& first, const InRestT&... rest) noexcept
{
    seed = hashCombineFast(seed, hashOf(first));
    ((seed = hashCombineFast(seed, hashOf(rest))), ...);
    return seed;
}

/// @brief `HashTraits` specialization for `Pair`-like types (`gp::Pair<T1, T2>`).
template <detail::IsPairLike InPairT>
struct HashTraits<InPairT>
{
    /// @brief Computes a 64-bit hash combining `first` and `second`.
    /// @param[in] pair Source pair.
    /// @return Combined 64-bit hash.
    [[nodiscard]] static constexpr gp::UInt64 hash(const InPairT& pair) noexcept
    {
        return hashCombine(hashOf(pair.first), hashOf(pair.second));
    }

    /// @brief Streams `first` and `second` into @p hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    /// @param[in] pair Source pair.
    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const InPairT& pair) noexcept
    {
        hasher.append(pair.first);
        hasher.append(pair.second);
    }
};

/// @brief `HashTraits` specialization for `Optional`-like types (`gp::Optional<T>`).
template <detail::IsOptionalLike InOptionalT>
struct HashTraits<InOptionalT>
{
    /// @brief Computes a 64-bit hash (`0` when empty, mixed value hash when engaged).
    /// @param[in] opt Source optional.
    /// @return 64-bit hash.
    [[nodiscard]] static constexpr gp::UInt64 hash(const InOptionalT& opt) noexcept
    {
        if (!opt.hasValue())
        {
            return 0ULL;
        }
        return hashCombine(0x9E3779B97F4A7C15ULL, hashOf(opt.value()));
    }

    /// @brief Streams a presence byte and, if engaged, the contained value into @p hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    /// @param[in] opt Source optional.
    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const InOptionalT& opt) noexcept
    {
        hasher.updateByte(opt.hasValue() ? 1u : 0u);
        if (opt.hasValue())
        {
            hasher.append(opt.value());
        }
    }
};

/// @brief `HashTraits` specialization for `Interval`-like types (`gp::Interval<T>`).
template <detail::IsIntervalLike InIntervalT>
struct HashTraits<InIntervalT>
{
    /// @brief Computes a 64-bit hash over `min` and `max`.
    /// @param[in] interval Source interval.
    /// @return Combined 64-bit hash.
    [[nodiscard]] static constexpr gp::UInt64 hash(const InIntervalT& interval) noexcept
    {
        return hashCombine(hashOf(interval.min), hashOf(interval.max));
    }

    /// @brief Streams `min` and `max` into @p hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    /// @param[in] interval Source interval.
    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const InIntervalT& interval) noexcept
    {
        hasher.append(interval.min);
        hasher.append(interval.max);
    }
};

/// @brief `HashTraits` specialization for smart-pointer types (`gp::UniquePtr`, `gp::SharedPtr`) without `.getHash()`.
template <detail::IsSmartPointerLike InPtrT>
struct HashTraits<InPtrT>
{
    /// @brief Computes the address hash of `ptr.get()`.
    /// @param[in] ptr Source smart pointer.
    /// @return 64-bit address hash (`0` when null).
    [[nodiscard]] static constexpr gp::UInt64 hash(const InPtrT& ptr) noexcept
    {
        return hashPointer(ptr.get());
    }

    /// @brief Streams the address hash of `ptr.get()` into @p hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    /// @param[in] ptr Source smart pointer.
    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const InPtrT& ptr) noexcept
    {
        hasher.append(hashPointer(ptr.get()));
    }
};

/// @brief `HashTraits` specialization for contiguous non-string ranges (`gp::Array`, `gp::VectorView`).
template <detail::IsContiguousElementRange InRangeT>
struct HashTraits<InRangeT>
{
    /// @brief Computes a 64-bit hash over all elements and the element count.
    /// @param[in] range Source contiguous range.
    /// @return 64-bit hash.
    [[nodiscard]] static constexpr gp::UInt64 hash(const InRangeT& range) noexcept
    {
        using ValueT = std::ranges::range_value_t<InRangeT>;
        if constexpr (sizeof(ValueT) == 1u && concepts::IsByteLike<ValueT>)
        {
            return Fnv1a64::hash(std::ranges::data(range), std::ranges::size(range));
        }
        else
        {
            gp::UInt64 seed = static_cast<gp::UInt64>(std::ranges::size(range));
            for (const ValueT& elem: range)
            {
                seed = hashCombine(seed, hashOf(elem));
            }
            return seed;
        }
    }

    /// @brief Streams all elements and the element count into @p hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    /// @param[in] range Source contiguous range.
    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const InRangeT& range) noexcept
    {
        using ValueT = std::ranges::range_value_t<InRangeT>;
        if constexpr (sizeof(ValueT) == 1u && concepts::IsByteLike<ValueT>)
        {
            const gp::USize count = std::ranges::size(range);
            if constexpr (
                gp::concepts::IsSameAs<ValueT, gp::UInt8> || gp::concepts::IsSameAs<ValueT, gp::Byte> ||
                gp::concepts::IsSameAs<ValueT, char>
            )
            {
                hasher.update(std::ranges::data(range), count);
            }
            else
            {
                const ValueT* const ptr = std::ranges::data(range);
                for (gp::USize i = 0u; i < count; ++i)
                {
                    hasher.updateByte(static_cast<gp::UInt8>(ptr[i]));
                }
            }
            hasher.append(static_cast<gp::UInt64>(count));
        }
        else
        {
            for (const ValueT& elem: range)
            {
                hasher.append(elem);
            }
            hasher.append(static_cast<gp::UInt64>(std::ranges::size(range)));
        }
    }
};

}   // namespace gp::hashing
