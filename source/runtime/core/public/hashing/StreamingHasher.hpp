// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashMix.hpp"
#include <bit>
#include <cstring>
#include <type_traits>

namespace gp::hashing
{

namespace detail
{

/// @brief Folds an ASCII uppercase character `'A'..'Z'` to lowercase without consulting OS locales.
/// @tparam InCharT Character code-unit type.
/// @param[in] ch Input code unit.
/// @return Lowercase ASCII code unit if @p ch is `'A'..'Z'`, otherwise @p ch unchanged.
template <gp::concepts::IsCharacter InCharT>
[[nodiscard]] constexpr InCharT toAsciiLower(const InCharT ch) noexcept
{
    using UnsignedT = std::make_unsigned_t<InCharT>;
    const UnsignedT u = static_cast<UnsignedT>(ch);
    if (u >= static_cast<UnsignedT>('A') && u <= static_cast<UnsignedT>('Z'))
    {
        return static_cast<InCharT>(u + static_cast<UnsignedT>('a' - 'A'));
    }
    return ch;
}

/// @brief Computes bounded null-terminated code-unit length for fixed character arrays and string literals.
/// @tparam InCharT Character code-unit type.
/// @param[in] str Character array pointer.
/// @param[in] maxCount Maximum number of elements in the array.
/// @return Number of characters before the first null terminator, or @p maxCount if none is present.
template <gp::concepts::IsCharacter InCharT>
[[nodiscard]] constexpr gp::USize boundedCharLength(const InCharT* const str, const gp::USize maxCount) noexcept
{
    if (str == nullptr || maxCount == 0u)
    {
        return 0u;
    }
    gp::USize len = 0u;
    while (len < maxCount && str[len] != InCharT{ 0 })
    {
        ++len;
    }
    return len;
}

/// @brief Computes full null-terminated code-unit length of a C-string (`0` when `str == nullptr`).
/// @tparam InCharT Character code-unit type.
/// @param[in] str Null-terminated character pointer, or `nullptr`.
/// @return Number of characters preceding the null terminator.
template <gp::concepts::IsCharacter InCharT>
[[nodiscard]] constexpr gp::USize cStringLength(const InCharT* const str) noexcept
{
    if (str == nullptr)
    {
        return 0u;
    }
    gp::USize len = 0u;
    while (str[len] != InCharT{ 0 })
    {
        ++len;
    }
    return len;
}

/// @brief Reads an unaligned 32-bit unsigned integer in little-endian byte order at compile time or runtime.
/// @param[in] ptr Source byte pointer with at least 4 readable bytes.
/// @return Little-endian decoded 32-bit word.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 readLittleEndian32(const gp::UInt8* const ptr) noexcept
{
    if (!std::is_constant_evaluated())
    {
        if constexpr (std::endian::native == std::endian::little)
        {
            gp::UInt32 value = 0u;
            std::memcpy(&value, ptr, sizeof(gp::UInt32));
            return value;
        }
    }
    return static_cast<gp::UInt32>(ptr[0]) | (static_cast<gp::UInt32>(ptr[1]) << 8u) |
           (static_cast<gp::UInt32>(ptr[2]) << 16u) | (static_cast<gp::UInt32>(ptr[3]) << 24u);
}

/// @brief Reads an unaligned 64-bit unsigned integer in little-endian byte order at compile time or runtime.
/// @param[in] ptr Source byte pointer with at least 8 readable bytes.
/// @return Little-endian decoded 64-bit word.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 readLittleEndian64(const gp::UInt8* const ptr) noexcept
{
    if (!std::is_constant_evaluated())
    {
        if constexpr (std::endian::native == std::endian::little)
        {
            gp::UInt64 value = 0ULL;
            std::memcpy(&value, ptr, sizeof(gp::UInt64));
            return value;
        }
    }
    return static_cast<gp::UInt64>(ptr[0]) | (static_cast<gp::UInt64>(ptr[1]) << 8u) |
           (static_cast<gp::UInt64>(ptr[2]) << 16u) | (static_cast<gp::UInt64>(ptr[3]) << 24u) |
           (static_cast<gp::UInt64>(ptr[4]) << 32u) | (static_cast<gp::UInt64>(ptr[5]) << 40u) |
           (static_cast<gp::UInt64>(ptr[6]) << 48u) | (static_cast<gp::UInt64>(ptr[7]) << 56u);
}

/// @brief Copies @p count bytes at compile time or runtime via `std::memcpy`.
/// @param[out] dst Destination byte buffer.
/// @param[in] src Source byte buffer.
/// @param[in] count Number of bytes to copy.
GP_FORCEINLINE_HINT constexpr void
    copyBytes(gp::UInt8* const dst, const gp::UInt8* const src, const gp::USize count) noexcept
{
    if (count == 0u)
    {
        return;
    }
    if (!std::is_constant_evaluated())
    {
        std::memcpy(dst, src, count);
        return;
    }
    for (gp::USize i = 0u; i < count; ++i)
    {
        dst[i] = src[i];
    }
}

/// @brief Detects whether `HashTraits<InT>::append(hasher, value)` is specialized for @p InT.
template <typename InT, typename InHasher>
concept HasHashTraitsAppend = requires(InHasher& hasher, const std::remove_cvref_t<InT>& value) {
    HashTraits<std::remove_cvref_t<InT>>::append(hasher, value);
};

}   // namespace detail

/// @brief Shared CRTP base giving every hash algorithm one uniform streaming, one-shot, and `constexpr` API.
/// @details Each concrete algorithm (`Fnv1a32`, `Fnv1a64`, `BasicCrc32`, `Murmur3_32`, `Murmur3_128`, `XxHash32`,
///          `XxHash64`) implements only `resetImpl`, `updateBytesImpl`, and `finalizeImpl`. This base provides all
///          byte, character, string-view, case-insensitive, and structured-field overloads without duplicating loops.
/// @tparam InDerived Concrete CRTP hasher subclass.
/// @tparam InDigestType Digest output type (`gp::UInt32`, `gp::UInt64`, or `Hash128`).
/// @tparam InDefaultSeed Initial seed used by default-constructed hashers and one-shot calls.
/// @tparam InAlgorithm Algorithm identifier enumerator.
template <typename InDerived, typename InDigestType, InDigestType InDefaultSeed, HashAlgorithm InAlgorithm>
class StreamingHasherBase
{
public:
    /// @brief Digest output type produced by `finalize()` and `hash()`.
    using DigestType = InDigestType;

    /// @brief Enumerator identifying this algorithm for runtime metadata and dispatch.
    static constexpr HashAlgorithm kAlgorithm = InAlgorithm;

    /// @brief Canonical initial seed used when no explicit seed is supplied.
    static constexpr DigestType kDefaultSeed = InDefaultSeed;

    /// @brief Output digest width in bits (`32`, `64`, or `128`).
    static constexpr gp::USize kDigestBits = sizeof(DigestType) * 8u;

public:
    /// @brief Resets the internal streaming state to @p seed so the hasher can be reused without reallocation.
    /// @param[in] seed Initial seed value (defaults to `kDefaultSeed`).
    constexpr void reset(const DigestType seed = kDefaultSeed) noexcept
    {
        derived().resetImpl(seed);
    }

    /// @brief Handles null-pointer zero-length inputs unambiguously.
    /// @param[in] data Null pointer literal.
    /// @param[in] sizeInBytes Ignored byte count.
    constexpr void update([[maybe_unused]] gp::NullPtrT data, [[maybe_unused]] const gp::USize sizeInBytes) noexcept
    {}

    /// @brief Feeds a contiguous array of `gp::UInt8` bytes into the streaming state (`constexpr`-capable).
    /// @param[in] data Source byte pointer (safe when `nullptr`).
    /// @param[in] sizeInBytes Number of bytes to consume.
    constexpr void update(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        if (data == nullptr || sizeInBytes == 0u)
        {
            return;
        }
        derived().updateBytesImpl(data, sizeInBytes);
    }

    /// @brief Feeds a contiguous array of `gp::Byte` (`std::byte`) values into the streaming state
    /// (`constexpr`-capable).
    /// @param[in] data Source `gp::Byte` pointer (safe when `nullptr`).
    /// @param[in] sizeInBytes Number of bytes to consume.
    constexpr void update(const gp::Byte* const data, const gp::USize sizeInBytes) noexcept
    {
        if (data == nullptr || sizeInBytes == 0u)
        {
            return;
        }
        if (!std::is_constant_evaluated())
        {
            derived().updateBytesImpl(reinterpret_cast<const gp::UInt8*>(data), sizeInBytes);
            return;
        }

        constexpr gp::USize kChunkBytes = 64u;
        gp::UInt8 buffer[kChunkBytes]{};
        gp::USize offset = 0u;
        while (offset < sizeInBytes)
        {
            const gp::USize remaining = sizeInBytes - offset;
            const gp::USize chunk = (remaining < kChunkBytes) ? remaining : kChunkBytes;
            for (gp::USize i = 0u; i < chunk; ++i)
            {
                buffer[i] = std::to_integer<gp::UInt8>(data[offset + i]);
            }
            derived().updateBytesImpl(buffer, chunk);
            offset += chunk;
        }
    }

    /// @brief Feeds a sequence of character code units into the streaming state (`constexpr`-capable).
    /// @details Single-byte characters (`char`, `char8_t`) feed directly as bytes; wider code units (`wchar_t`,
    ///          `char16_t`, `char32_t`) serialize in deterministic little-endian byte order.
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] data Character sequence pointer (safe when `nullptr`).
    /// @param[in] count Number of character code units to consume.
    template <gp::concepts::IsCharacter InCharT>
    constexpr void update(const InCharT* const data, const gp::USize count) noexcept
    {
        updateCharsImpl<false>(data, count);
    }

    /// @brief Feeds an untyped runtime byte buffer into the streaming state.
    /// @note Non-`constexpr` because C++ forbids casting `const void*` to `const gp::UInt8*` during constant
    ///       evaluation; pass `const gp::UInt8*`, `const gp::Byte*`, or `const char*` in `constexpr` contexts.
    /// @param[in] data Buffer address (safe when `nullptr`).
    /// @param[in] sizeInBytes Number of bytes to consume.
    void update(const void* const data, const gp::USize sizeInBytes) noexcept
    {
        if (data == nullptr || sizeInBytes == 0u)
        {
            return;
        }
        derived().updateBytesImpl(static_cast<const gp::UInt8*>(data), sizeInBytes);
    }

    /// @brief Feeds a string-like container or view (`StringView`, `String`, `FixedString`) into the stream.
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to feed (without trailing null terminator).
    template <concepts::IsCharacterSequence InStrT>
    constexpr void update(const InStrT& str) noexcept
    {
        update(str.data(), str.size());
    }

    /// @brief Feeds a null-terminated C-string into the streaming state (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] str Null-terminated character pointer (safe when `nullptr`).
    template <gp::concepts::IsCharacter InCharT>
    constexpr void updateCString(const InCharT* const str) noexcept
    {
        update(str, detail::cStringLength(str));
    }

    /// @brief Feeds a character sequence after folding ASCII `'A'..'Z'` to `'a'..'z'` (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] data Character sequence pointer (safe when `nullptr`).
    /// @param[in] count Number of character code units to fold and consume.
    template <gp::concepts::IsCharacter InCharT>
    constexpr void updateIgnoreCase(const InCharT* const data, const gp::USize count) noexcept
    {
        updateCharsImpl<true>(data, count);
    }

    /// @brief Feeds a string-like container or view after ASCII lowercase folding (`constexpr`-capable).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to fold and feed.
    template <concepts::IsCharacterSequence InStrT>
    constexpr void updateIgnoreCase(const InStrT& str) noexcept
    {
        updateIgnoreCase(str.data(), str.size());
    }

    /// @brief Feeds a null-terminated C-string after ASCII lowercase folding (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] str Null-terminated character pointer (safe when `nullptr`).
    template <gp::concepts::IsCharacter InCharT>
    constexpr void updateCStringIgnoreCase(const InCharT* const str) noexcept
    {
        updateIgnoreCase(str, detail::cStringLength(str));
    }

    /// @brief Feeds a character pointer range honoring @p hashCase (`CaseSensitive` or `IgnoreCase`).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] data Character sequence pointer.
    /// @param[in] count Number of character code units to consume.
    /// @param[in] hashCase ASCII case-sensitivity mode.
    template <gp::concepts::IsCharacter InCharT>
    constexpr void update(const InCharT* const data, const gp::USize count, const HashCase hashCase) noexcept
    {
        if (hashCase == HashCase::CaseSensitive)
        {
            update(data, count);
        }
        else
        {
            updateIgnoreCase(data, count);
        }
    }

    /// @brief Feeds a string-like object honoring @p hashCase (`CaseSensitive` or `IgnoreCase`).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to feed.
    /// @param[in] hashCase ASCII case-sensitivity mode.
    template <concepts::IsCharacterSequence InStrT>
    constexpr void update(const InStrT& str, const HashCase hashCase) noexcept
    {
        update(str.data(), str.size(), hashCase);
    }

    /// @brief Feeds a single raw byte into the streaming state.
    /// @param[in] byte Byte value to consume.
    constexpr void updateByte(const gp::UInt8 byte) noexcept
    {
        derived().updateBytesImpl(&byte, 1u);
    }

    /// @brief Feeds a structured C++ value into the stream while framing variable-length fields.
    /// @details Resolution order:
    ///          1. `HashTraits<T>::append(hasher, value)` when specialized (including `HashOf.hpp` container traits).
    ///          2. Member `value.hashAppend(hasher)` or ADL `gpHashAppend(hasher, value)`.
    ///          3. Character sequences (`StringView`, `String`, `FixedString`), fed with trailing byte-count framing
    ///             so `("ab", "c")` and `("a", "bc")` stay distinct.
    ///          4. Null-terminated character pointers (`const char*`), framed by length.
    ///          5. Fixed C-arrays (`T[N]`), integrals, booleans, enums, normalized IEEE-754 floats, and raw pointers.
    /// @tparam InValueT Value type to serialize into the hash stream.
    /// @param[in] value Input value to append.
    template <typename InValueT>
    constexpr void append(const InValueT& value) noexcept
    {
        using DecayedT = std::remove_cvref_t<InValueT>;

        if constexpr (detail::HasHashTraitsAppend<DecayedT, InDerived>)
        {
            HashTraits<DecayedT>::append(derived(), value);
        }
        else if constexpr (concepts::HasMemberHashAppend<DecayedT, InDerived>)
        {
            value.hashAppend(derived());
        }
        else if constexpr (concepts::HasAdlHashAppend<DecayedT, InDerived>)
        {
            gpHashAppend(derived(), value);
        }
        else if constexpr (concepts::IsCharacterSequence<DecayedT>)
        {
            update(value.data(), value.size());
            appendIntegralLittleEndian(static_cast<gp::UInt64>(value.size()));
        }
        else if constexpr (gp::concepts::IsBool<DecayedT>)
        {
            updateByte(value ? 1u : 0u);
        }
        else if constexpr (gp::concepts::IsIntegral<DecayedT>)
        {
            appendIntegralLittleEndian(value);
        }
        else if constexpr (gp::concepts::IsEnum<DecayedT>)
        {
            appendIntegralLittleEndian(static_cast<std::underlying_type_t<DecayedT>>(value));
        }
        else if constexpr (gp::concepts::IsFloatingPoint<DecayedT>)
        {
            if constexpr (sizeof(DecayedT) == 4u)
            {
                appendIntegralLittleEndian(normalizeFloatBits32(static_cast<gp::Float32>(value)));
            }
            else if constexpr (sizeof(DecayedT) == 8u)
            {
                appendIntegralLittleEndian(normalizeFloatBits64(static_cast<gp::Float64>(value)));
            }
        }
        else if constexpr (gp::concepts::IsNullPointer<DecayedT>)
        {
            appendIntegralLittleEndian(static_cast<gp::UInt64>(0ULL));
        }
        else if constexpr (gp::concepts::IsPointer<DecayedT>)
        {
            using PointeeT = std::remove_cv_t<std::remove_pointer_t<DecayedT>>;
            if constexpr (gp::concepts::IsCharacter<PointeeT>)
            {
                const gp::USize len = detail::cStringLength(value);
                if (value != nullptr && len > 0u)
                {
                    update(value, len);
                }
                appendIntegralLittleEndian(static_cast<gp::UInt64>(len));
            }
            else
            {
                appendIntegralLittleEndian(hashPointer(value));
            }
        }
        else if constexpr (gp::concepts::IsArray<DecayedT>)
        {
            using ElemT = std::remove_cv_t<std::remove_extent_t<DecayedT>>;
            constexpr gp::USize kCount = std::extent_v<DecayedT>;
            if constexpr (gp::concepts::IsCharacter<ElemT>)
            {
                const gp::USize len = detail::boundedCharLength(value, kCount);
                update(value, len);
                appendIntegralLittleEndian(static_cast<gp::UInt64>(len));
            }
            else
            {
                for (gp::USize i = 0u; i < kCount; ++i)
                {
                    append(value[i]);
                }
            }
        }
        else if constexpr (concepts::HasMemberHash<DecayedT>)
        {
            appendIntegralLittleEndian(static_cast<gp::UInt64>(value.hash()));
        }
        else if constexpr (concepts::HasMemberGetHash<DecayedT>)
        {
            appendIntegralLittleEndian(static_cast<gp::UInt64>(value.getHash()));
        }
        else
        {
            static_assert(sizeof(DecayedT) == 0u, "Type is not hashable: specialize gp::hashing::HashTraits<T>.");
        }
    }

    /// @brief Appends multiple values sequentially into this hasher.
    /// @tparam InArgs Value types to append in order.
    /// @param[in] args Values forwarded to `append()`.
    template <typename... InArgs>
    constexpr void appendAll(const InArgs&... args) noexcept
    {
        (append(args), ...);
    }

    /// @brief Computes and returns the final digest without mutating or resetting the internal state.
    /// @note Callers can read a checkpoint digest with `finalize()` and keep feeding more chunks with `update()`.
    /// @return Computed digest value.
    [[nodiscard]] constexpr DigestType finalize() const noexcept
    {
        return derived().finalizeImpl();
    }

public:
    /// @section Unified One-Shot Static Methods

    /// @brief Computes a one-shot digest for a `nullptr` zero-byte input (`constexpr`-capable).
    /// @param[in] data Null pointer literal.
    /// @param[in] sizeInBytes Ignored byte count.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Final digest for empty input.
    [[nodiscard]] static constexpr DigestType hash(
        [[maybe_unused]] gp::NullPtrT data,
        [[maybe_unused]] const gp::USize sizeInBytes,
        const DigestType seed = kDefaultSeed
    ) noexcept
    {
        InDerived hasher(seed);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a `gp::UInt8` buffer (`constexpr`-capable).
    /// @param[in] data Byte buffer pointer (safe when `nullptr`).
    /// @param[in] sizeInBytes Number of bytes to hash.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    [[nodiscard]] static constexpr DigestType
        hash(const gp::UInt8* const data, const gp::USize sizeInBytes, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.update(data, sizeInBytes);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a `gp::Byte` (`std::byte`) buffer (`constexpr`-capable).
    /// @param[in] data `gp::Byte` buffer pointer (safe when `nullptr`).
    /// @param[in] sizeInBytes Number of bytes to hash.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    [[nodiscard]] static constexpr DigestType
        hash(const gp::Byte* const data, const gp::USize sizeInBytes, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.update(data, sizeInBytes);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a character sequence (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] data Character pointer (safe when `nullptr`).
    /// @param[in] count Number of character code units to hash.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    template <gp::concepts::IsCharacter InCharT>
    [[nodiscard]] static constexpr DigestType
        hash(const InCharT* const data, const gp::USize count, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.update(data, count);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a string literal or fixed character array (`constexpr`-capable).
    /// @details Takes one argument so 2-argument `hash(literal, count)` calls never confuse `count` with `seed`.
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @tparam InN Array extent including any trailing null terminator.
    /// @param[in] literal Character array or string literal.
    /// @return Computed digest over characters preceding the first null terminator.
    template <gp::concepts::IsCharacter InCharT, gp::USize InN>
    [[nodiscard]] static constexpr DigestType hash(const InCharT (&literal)[InN]) noexcept
    {
        return hash(literal, detail::boundedCharLength(literal, InN));
    }

    /// @brief Computes a one-shot digest over a fixed `gp::UInt8` byte array (`constexpr`-capable).
    /// @tparam InN Array extent in bytes.
    /// @param[in] bytes Fixed byte array.
    /// @return Computed digest.
    template <gp::USize InN>
    [[nodiscard]] static constexpr DigestType hash(const gp::UInt8 (&bytes)[InN]) noexcept
    {
        return hash(bytes, InN);
    }

    /// @brief Computes a one-shot digest over an untyped runtime memory buffer.
    /// @param[in] data Buffer address (safe when `nullptr`).
    /// @param[in] sizeInBytes Number of bytes to hash.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    [[nodiscard]] static DigestType
        hash(const void* const data, const gp::USize sizeInBytes, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.update(data, sizeInBytes);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a string-like container or view (`constexpr`-capable).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to hash.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    template <concepts::IsCharacterSequence InStrT>
    [[nodiscard]] static constexpr DigestType hash(const InStrT& str, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.update(str);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a character buffer using @p hashCase (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] data Character pointer (safe when `nullptr`).
    /// @param[in] count Number of characters to hash.
    /// @param[in] hashCase ASCII case-sensitivity mode.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    template <gp::concepts::IsCharacter InCharT>
    [[nodiscard]] static constexpr DigestType hash(
        const InCharT* const data, const gp::USize count, const HashCase hashCase, const DigestType seed = kDefaultSeed
    ) noexcept
    {
        InDerived hasher(seed);
        hasher.update(data, count, hashCase);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a string-like object using @p hashCase (`constexpr`-capable).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to hash.
    /// @param[in] hashCase ASCII case-sensitivity mode.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    template <concepts::IsCharacterSequence InStrT>
    [[nodiscard]] static constexpr DigestType
        hash(const InStrT& str, const HashCase hashCase, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.update(str, hashCase);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot digest over a null-terminated C-string (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] str Null-terminated string pointer (safe when `nullptr`).
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed digest.
    template <gp::concepts::IsCharacter InCharT>
    [[nodiscard]] static constexpr DigestType
        hashCString(const InCharT* const str, const DigestType seed = kDefaultSeed) noexcept
    {
        return hash(str, detail::cStringLength(str), seed);
    }

    /// @brief Computes a one-shot ASCII case-insensitive digest over a character buffer (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] data Character pointer (safe when `nullptr`).
    /// @param[in] count Number of character code units to fold and hash.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed case-insensitive digest.
    template <gp::concepts::IsCharacter InCharT>
    [[nodiscard]] static constexpr DigestType
        hashIgnoreCase(const InCharT* const data, const gp::USize count, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.updateIgnoreCase(data, count);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot ASCII case-insensitive digest over a string literal (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @tparam InN Array extent including any trailing null terminator.
    /// @param[in] literal Character array or string literal.
    /// @return Computed case-insensitive digest.
    template <gp::concepts::IsCharacter InCharT, gp::USize InN>
    [[nodiscard]] static constexpr DigestType hashIgnoreCase(const InCharT (&literal)[InN]) noexcept
    {
        return hashIgnoreCase(literal, detail::boundedCharLength(literal, InN));
    }

    /// @brief Computes a one-shot ASCII case-insensitive digest over a string-like object (`constexpr`-capable).
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] str Character sequence to fold and hash.
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed case-insensitive digest.
    template <concepts::IsCharacterSequence InStrT>
    [[nodiscard]] static constexpr DigestType
        hashIgnoreCase(const InStrT& str, const DigestType seed = kDefaultSeed) noexcept
    {
        InDerived hasher(seed);
        hasher.updateIgnoreCase(str);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot ASCII case-insensitive digest over a null-terminated C-string (`constexpr`-capable).
    /// @tparam InCharT Character type satisfying `gp::concepts::IsCharacter`.
    /// @param[in] str Null-terminated string pointer (safe when `nullptr`).
    /// @param[in] seed Optional initial seed (defaults to `kDefaultSeed`).
    /// @return Computed case-insensitive digest.
    template <gp::concepts::IsCharacter InCharT>
    [[nodiscard]] static constexpr DigestType
        hashCStringIgnoreCase(const InCharT* const str, const DigestType seed = kDefaultSeed) noexcept
    {
        return hashIgnoreCase(str, detail::cStringLength(str), seed);
    }

protected:
    /// @brief Protected default constructor restricted to CRTP derived hashers.
    constexpr StreamingHasherBase() noexcept = default;

private:
    [[nodiscard]] constexpr InDerived& derived() noexcept
    {
        return static_cast<InDerived&>(*this);
    }

    [[nodiscard]] constexpr const InDerived& derived() const noexcept
    {
        return static_cast<const InDerived&>(*this);
    }

    template <bool InIgnoreCase, gp::concepts::IsCharacter InCharT>
    constexpr void updateCharsImpl(const InCharT* const data, const gp::USize count) noexcept
    {
        if (data == nullptr || count == 0u)
        {
            return;
        }

        if constexpr (!InIgnoreCase && sizeof(InCharT) == 1u)
        {
            if (!std::is_constant_evaluated())
            {
                derived().updateBytesImpl(reinterpret_cast<const gp::UInt8*>(data), count);
                return;
            }
        }

        using UnsignedCharT = std::make_unsigned_t<InCharT>;
        constexpr gp::USize kChunkBytes = 64u;
        gp::UInt8 buffer[kChunkBytes]{};
        gp::USize bufferedBytes = 0u;

        for (gp::USize i = 0u; i < count; ++i)
        {
            const InCharT ch = InIgnoreCase ? detail::toAsciiLower(data[i]) : data[i];
            const UnsignedCharT u = static_cast<UnsignedCharT>(ch);
            if constexpr (sizeof(InCharT) == 1u)
            {
                buffer[bufferedBytes++] = static_cast<gp::UInt8>(u);
            }
            else
            {
                for (gp::USize b = 0u; b < sizeof(InCharT); ++b)
                {
                    buffer[bufferedBytes++] =
                        static_cast<gp::UInt8>((u >> (b * 8u)) & static_cast<UnsignedCharT>(0xFFu));
                }
            }

            if (bufferedBytes == kChunkBytes)
            {
                derived().updateBytesImpl(buffer, bufferedBytes);
                bufferedBytes = 0u;
            }
        }

        if (bufferedBytes > 0u)
        {
            derived().updateBytesImpl(buffer, bufferedBytes);
        }
    }

    template <gp::concepts::IsIntegral InIntT>
    constexpr void appendIntegralLittleEndian(const InIntT value) noexcept
    {
        using UnsignedT = std::make_unsigned_t<InIntT>;
        const UnsignedT u = static_cast<UnsignedT>(value);
        gp::UInt8 bytes[sizeof(InIntT)]{};
        for (gp::USize i = 0u; i < sizeof(InIntT); ++i)
        {
            bytes[i] = static_cast<gp::UInt8>((u >> (i * 8u)) & static_cast<UnsignedT>(0xFFu));
        }
        derived().updateBytesImpl(bytes, sizeof(InIntT));
    }
};

}   // namespace gp::hashing
