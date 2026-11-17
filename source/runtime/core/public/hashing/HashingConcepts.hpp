// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::hashing::concepts
{

/// @brief Concept satisfied by valid digest output types (`gp::UInt32`, `gp::UInt64`, or `Hash128`).
/// @tparam InT Candidate digest type.
template <typename InT>
concept IsHashDigest = gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::UInt32> ||
                       gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::UInt64> ||
                       gp::concepts::IsSameAs<std::remove_cvref_t<InT>, Hash128>;

/// @brief Concept satisfied by raw byte-like elements (`gp::UInt8`, `gp::Byte`, `char`, `char8_t`, `gp::Int8`).
/// @tparam InT Candidate element type.
template <typename InT>
concept IsByteLike =
    gp::concepts::IsSameAs<std::remove_cv_t<InT>, gp::UInt8> ||
    gp::concepts::IsSameAs<std::remove_cv_t<InT>, gp::Byte> || gp::concepts::IsSameAs<std::remove_cv_t<InT>, char> ||
    gp::concepts::IsSameAs<std::remove_cv_t<InT>, char8_t> || gp::concepts::IsSameAs<std::remove_cv_t<InT>, gp::Int8>;

/// @brief Concept satisfied by string-like types exposing character `.data()`, `.size()`, and `TraitsType`.
/// @details Matches `BasicStringView`, `BasicFixedString`, and `BasicString` without including their headers.
/// @tparam InT Candidate string type.
template <typename InT>
concept IsCharacterSequence = requires(const std::remove_cvref_t<InT>& str) {
    typename std::remove_cvref_t<InT>::TraitsType;
    typename std::remove_cvref_t<InT>::ValueType;
    requires gp::concepts::IsCharacter<typename std::remove_cvref_t<InT>::ValueType>;
    {
        str.data()
    } -> gp::concepts::IsConvertibleTo<const typename std::remove_cvref_t<InT>::ValueType*>;
    {
        str.size()
    } -> gp::concepts::IsConvertibleTo<gp::USize>;
};

/// @brief Concept enforcing the unified streaming and one-shot contract implemented by all `gp::hashing` algorithms.
/// @tparam InT Candidate hasher type (`Fnv1a32`, `Fnv1a64`, `Crc32`, `Crc32C`, `Murmur3_32`, `Murmur3_128`,
///             `XxHash32`, `XxHash64`).
template <typename InT>
concept IsStreamingHasher = requires(
    InT hasher,
    const InT constHasher,
    const gp::UInt8* bytePtr,
    const gp::Byte* stdBytePtr,
    const char* charPtr,
    const void* rawPtr,
    const gp::USize len,
    const typename InT::DigestType seed
) {
    typename InT::DigestType;
    requires IsHashDigest<typename InT::DigestType>;
    {
        InT::kAlgorithm
    } -> gp::concepts::IsConvertibleTo<HashAlgorithm>;
    {
        InT::kDefaultSeed
    } -> gp::concepts::IsConvertibleTo<typename InT::DigestType>;
    {
        InT::kDigestBits
    } -> gp::concepts::IsConvertibleTo<gp::USize>;
    {
        hasher.reset()
    } noexcept;
    {
        hasher.reset(seed)
    } noexcept;
    {
        hasher.update(bytePtr, len)
    } noexcept;
    {
        hasher.update(stdBytePtr, len)
    } noexcept;
    {
        hasher.update(charPtr, len)
    } noexcept;
    {
        hasher.update(rawPtr, len)
    } noexcept;
    {
        hasher.updateIgnoreCase(charPtr, len)
    } noexcept;
    {
        constHasher.finalize()
    } noexcept -> gp::concepts::IsSameAs<typename InT::DigestType>;
    {
        InT::hash(bytePtr, len)
    } noexcept -> gp::concepts::IsSameAs<typename InT::DigestType>;
    {
        InT::hash(charPtr, len)
    } noexcept -> gp::concepts::IsSameAs<typename InT::DigestType>;
    {
        InT::hashIgnoreCase(charPtr, len)
    } noexcept -> gp::concepts::IsSameAs<typename InT::DigestType>;
};

/// @brief Concept matching streaming hashers that produce a 32-bit digest (`gp::UInt32`).
/// @tparam InT Candidate hasher type.
template <typename InT>
concept IsStreamingHasher32 = IsStreamingHasher<InT> && gp::concepts::IsSameAs<typename InT::DigestType, gp::UInt32>;

/// @brief Concept matching streaming hashers that produce a 64-bit digest (`gp::UInt64`).
/// @tparam InT Candidate hasher type.
template <typename InT>
concept IsStreamingHasher64 = IsStreamingHasher<InT> && gp::concepts::IsSameAs<typename InT::DigestType, gp::UInt64>;

/// @brief Concept matching streaming hashers that produce a 128-bit digest (`Hash128`).
/// @tparam InT Candidate hasher type.
template <typename InT>
concept IsStreamingHasher128 = IsStreamingHasher<InT> && gp::concepts::IsSameAs<typename InT::DigestType, Hash128>;

/// @brief Concept satisfied by types implementing a member `hashAppend(hasher)` hook.
/// @tparam InT Candidate value type.
/// @tparam InHasher Hasher type passed to `hashAppend`.
template <typename InT, typename InHasher>
concept HasMemberHashAppend = requires(const InT& value, InHasher& hasher) { value.hashAppend(hasher); };

/// @brief Concept satisfied by types providing an ADL-discoverable `gpHashAppend(hasher, value)` overload.
/// @tparam InT Candidate value type.
/// @tparam InHasher Hasher type passed to `gpHashAppend`.
template <typename InT, typename InHasher>
concept HasAdlHashAppend = requires(const InT& value, InHasher& hasher) { gpHashAppend(hasher, value); };

/// @brief Concept satisfied by types providing a const `.hash()` member returning an integral digest.
/// @tparam InT Candidate value type.
template <typename InT>
concept HasMemberHash = requires(const InT& value) {
    {
        value.hash()
    } -> gp::concepts::IsIntegral;
};

/// @brief Concept satisfied by types providing a const `.getHash()` member returning an integral digest.
/// @tparam InT Candidate value type.
template <typename InT>
concept HasMemberGetHash = requires(const InT& value) {
    {
        value.getHash()
    } -> gp::concepts::IsIntegral;
};

}   // namespace gp::hashing::concepts
