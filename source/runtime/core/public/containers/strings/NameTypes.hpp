// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/StringEnums.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include <compare>

namespace gp::container
{

/// @brief 32-bit packed handle identifying a deduplicated string entry inside `gp::NamePool` (ADR-0046).
/// @details Bit layout for valid non-zero entries:
///          - Bit  31      (`1 bit`) : Valid entry marker (`kNameValidIdBit = 0x80000000u`).
///          - Bits 25..30 (`6 bits`) : Shard index (`0..63`).
///          - Bits 11..24 (`14 bits`): Slab page index within the shard (`0..16383`).
///          - Bits 0..10  (`11 bits`): 8-byte aligned offset (`byteOffset >> 3`) within the `16 KiB` slab.
using NameEntryId = gp::UInt32;

/// @brief Sentinel `NameEntryId` representing `Name()` (`"None"` / empty identifier).
inline constexpr NameEntryId kNameNoneId = 0u;

/// @brief High-bit flag set on every valid non-empty `NameEntryId` inside `gp::NamePool`.
inline constexpr NameEntryId kNameValidIdBit = 0x80000000u;

/// @brief Sentinel internal number stored in `gp::Name` when no `_N` numeric suffix is present.
/// @details Following Unreal Engine's `NAME_NO_NUMBER_INTERNAL` convention, `0` indicates no suffix, while positive
///          values store `externalNumber + 1` (so `"Enemy_0"` stores `1`, and `"Enemy_42"` stores `43`).
inline constexpr gp::Int32 kNameNoNumber = 0;

/// @brief Maximum byte length of a base string that can be interned in `gp::NamePool`.
inline constexpr gp::USize kMaxNameLength = 1023u;

/// @brief 16-byte aligned header preceding each null-terminated UTF-8 string inside a `NamePool` slab.
/// @details Immediately after `sizeof(NameEntryHeader)` bytes in the slab lies `char[length + 1]` (null-terminated).
struct alignas(8) NameEntryHeader
{
    /// @brief Precomputed 64-bit FNV-1a ASCII case-insensitive hash of the base string.
    gp::UInt64 caseInsensitiveHash{ 0u };

    /// @brief Length of the UTF-8 base string in bytes (excluding the trailing null terminator).
    gp::UInt16 length{ 0u };

    /// @brief Reserved flags/metadata field for future editor/serialization extensions.
    gp::UInt16 flags{ 0u };

    /// @brief Explicit padding to 16 bytes so the trailing character array starts at a deterministic offset.
    gp::UInt32 reserved{ 0u };

    /// @brief Returns a pointer to the null-terminated UTF-8 character array stored immediately after this header.
    /// @return Pointer to the first character of the interned string.
    [[nodiscard]] const char* cString() const noexcept
    {
        return reinterpret_cast<const char*>(this + 1);
    }

    /// @brief Returns a non-owning `StringView` over the interned UTF-8 characters.
    /// @return `StringView(cString(), length)`.
    [[nodiscard]] BasicStringView<char> view() const noexcept
    {
        return BasicStringView<char>(cString(), static_cast<gp::USize>(length));
    }
};

static_assert(sizeof(NameEntryHeader) == 16u, "NameEntryHeader must be exactly 16 bytes.");
static_assert(alignof(NameEntryHeader) == 8u, "NameEntryHeader must be 8-byte aligned.");

/// @brief Compile-time and runtime 64-bit FNV-1a string hash identifier for `switch` dispatch (ADR-0046).
/// @details Computes the exact same ASCII case-insensitive (or case-sensitive) 64-bit FNV-1a hash used by
///          `gp::NamePool`. Can be evaluated in `constexpr` contexts and `switch (hash.value())` statements via
///          the `gp::literals::operator""_nh` literal without touching the runtime `NamePool`.
class NameHash
{
public:
    /// @brief Hash value of an empty string (`0`).
    static constexpr gp::UInt64 kEmptyHash = 0u;

public:
    /// @brief Default-constructs an empty `NameHash` (`value() == 0`).
    [[nodiscard]] constexpr NameHash() noexcept = default;

    /// @brief Constructs a `NameHash` directly from a precomputed 64-bit hash integer.
    /// @param[in] rawHash Precomputed 64-bit FNV-1a hash value.
    [[nodiscard]] constexpr explicit NameHash(const gp::UInt64 rawHash) noexcept
        : m_hash(rawHash)
    {}

    /// @brief Computes a `NameHash` from a UTF-8 `StringView`.
    /// @param[in] str String view to hash (empty view produces `0`).
    /// @param[in] searchCase Case-sensitivity mode (defaults to `SearchCase::IgnoreCase` to match `gp::Name`).
    [[nodiscard]] constexpr explicit NameHash(
        const BasicStringView<char> str, const SearchCase searchCase = SearchCase::IgnoreCase
    ) noexcept
        : m_hash(str.isEmpty() ? kEmptyHash : CharTraits<char>::hash(str.data(), str.size(), searchCase))
    {}

    /// @brief Computes a `NameHash` from a null-terminated UTF-8 C-string.
    /// @param[in] str Null-terminated C-string (`nullptr` or `""` produces `0`).
    /// @param[in] searchCase Case-sensitivity mode (defaults to `SearchCase::IgnoreCase`).
    [[nodiscard]] constexpr explicit NameHash(
        const char* const str, const SearchCase searchCase = SearchCase::IgnoreCase
    ) noexcept
        : NameHash(BasicStringView<char>(str), searchCase)
    {}

public:
    /// @brief Returns the underlying 64-bit hash integer.
    /// @return 64-bit FNV-1a hash value (`0` if empty).
    [[nodiscard]] constexpr gp::UInt64 value() const noexcept
    {
        return m_hash;
    }

    /// @brief Implicit conversion to `gp::UInt64` for seamless use in `switch` statements and `case` labels.
    /// @return 64-bit FNV-1a hash value.
    [[nodiscard]] constexpr operator gp::UInt64() const noexcept
    {
        return m_hash;
    }

    /// @brief Checks whether this hash is empty (`value() == 0`).
    /// @return True if `m_hash == 0`, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_hash == kEmptyHash;
    }

    /// @brief Checks whether this hash is non-empty (`value() != 0`).
    /// @return True if `m_hash != 0`, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_hash != kEmptyHash;
    }

    /// @brief Equality comparison between two `NameHash` instances.
    /// @param[in] other Right-hand `NameHash`.
    /// @return True if both hashes are identical.
    [[nodiscard]] constexpr bool operator==(const NameHash& other) const noexcept = default;

    /// @brief Three-way comparison between two `NameHash` instances.
    /// @param[in] other Right-hand `NameHash`.
    /// @return `std::strong_ordering` of the underlying 64-bit hash values.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const NameHash& other) const noexcept = default;

private:
    gp::UInt64 m_hash{ kEmptyHash };
};

}   // namespace gp::container

namespace gp::literals
{

/// @brief Compile-time user-defined literal producing a case-insensitive `gp::NameHash` (e.g., `"Pelvis"_nh`).
/// @param[in] str Character array literal.
/// @param[in] len Length of the literal in bytes.
/// @return `constexpr` `gp::container::NameHash`.
[[nodiscard]] consteval gp::container::NameHash operator""_nh(const char* const str, const gp::USize len) noexcept
{
    return gp::container::NameHash(
        gp::container::BasicStringView<char>(str, len), gp::container::SearchCase::IgnoreCase
    );
}

/// @brief Compile-time user-defined literal producing a case-sensitive `gp::NameHash` (e.g., `"Pelvis"_nhs`).
/// @param[in] str Character array literal.
/// @param[in] len Length of the literal in bytes.
/// @return `constexpr` case-sensitive `gp::container::NameHash`.
[[nodiscard]] consteval gp::container::NameHash operator""_nhs(const char* const str, const gp::USize len) noexcept
{
    return gp::container::NameHash(
        gp::container::BasicStringView<char>(str, len), gp::container::SearchCase::CaseSensitive
    );
}

}   // namespace gp::literals

namespace gp
{

using NameEntryId = container::NameEntryId;
using NameEntryHeader = container::NameEntryHeader;
using NameHash = container::NameHash;
using container::kMaxNameLength;
using container::kNameNoneId;
using container::kNameNoNumber;
using container::kNameValidIdBit;

}   // namespace gp
